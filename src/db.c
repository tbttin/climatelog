#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sensor.h"
#include "db.h"
#include "config.h"

struct device {
	int hardware_id;
	char location[LOCATION_NAME_BUF_MAX];
};

struct db_context {
	struct sqlite3 *handle;
	struct sqlite3_stmt *insert_log_stmt;
	struct sqlite3_stmt *insert_device_stmt;

	struct device devices[MAX_DEVICE];
	int device_count;
};

static void
check_rc(int rc, const char *msg, struct sqlite3 *pdb) {
	if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW) {
		fprintf(stderr, "SQLite Error (%s): %s\n", msg, sqlite3_errmsg(pdb));
		exit(EXIT_FAILURE);
	}
}

static void
load_devices_from_db(struct db_context *ctx) {
	struct sqlite3_stmt *select_stmt = NULL;
	check_rc(sqlite3_prepare_v3(ctx->handle, "SELECT * FROM devices;", -1, 0, &select_stmt, NULL), "Loading devices", ctx->handle);
	ctx->device_count = 0;
	while (sqlite3_step(select_stmt) == SQLITE_ROW) {
		if (ctx->device_count >= MAX_DEVICE) {
			fprintf(stderr, "FATAL ERROR: Database device count exceeds fixed RAM array limit (%d). "
	                "Please increase MAX_DEVICE in your configuration.\n", MAX_DEVICE);
			sqlite3_finalize(select_stmt);
			exit(EXIT_FAILURE);
		}
		ctx->devices[ctx->device_count].hardware_id = sqlite3_column_int(select_stmt, 0);
		snprintf(ctx->devices[ctx->device_count].location, LOCATION_NAME_BUF_MAX, "%s", sqlite3_column_text(select_stmt, 1));
		ctx->device_count++;
	}
	sqlite3_finalize(select_stmt);
}

struct db_context *
db_init() {
	struct db_context *ctx = malloc(sizeof(*ctx));
	if (!ctx) return NULL;

	const char *db_file = getenv("CLIMATELOG_DB_FILE") ? getenv("CLIMATELOG_DB_FILE") : CLIMATELOG_DB_FILE;
	int rc = sqlite3_open_v2(db_file, &ctx->handle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
	if (rc != SQLITE_OK) {
		free(ctx);
		return NULL;
	}

	// Optimizations
	check_rc(sqlite3_exec(ctx->handle, "PRAGMA journal_mode = WAL;", NULL, NULL, NULL), "WAL mode", ctx->handle);
	check_rc(sqlite3_exec(ctx->handle, "PRAGMA synchronous = NORMAL;", NULL, NULL, NULL), "Synchronous", ctx->handle);
	sqlite3_busy_timeout(ctx->handle, SQLITE_TIMEOUT_BUSY);

	// Schema creation
	check_rc(sqlite3_exec(ctx->handle, "CREATE TABLE IF NOT EXISTS devices (hardware_id INTEGER PRIMARY KEY, location TEXT NOT NULL);", NULL, NULL, NULL), "Devices Table", ctx->handle);
	check_rc(sqlite3_exec(ctx->handle, "CREATE TABLE IF NOT EXISTS climate_logs (id INTEGER PRIMARY KEY AUTOINCREMENT, device_id INTEGER NOT NULL, temperature REAL NOT NULL, humidity REAL NOT NULL, timestamp INTEGER NOT NULL, sensor_status INTEGER NOT NULL);", NULL, NULL, NULL), "Logs Table", ctx->handle);
	check_rc(sqlite3_exec(ctx->handle, "CREATE INDEX IF NOT EXISTS idx_logs_dev_time ON climate_logs (device_id, timestamp DESC);", NULL, NULL, NULL), "Logs Index", ctx->handle);
	check_rc(sqlite3_exec(ctx->handle, "CREATE TABLE IF NOT EXISTS system_maintenances (task_name TEXT PRIMARY KEY, last_run_time INTEGER NOT NULL);", NULL, NULL, NULL), "Maint Table", ctx->handle);
	check_rc(sqlite3_exec(ctx->handle, "INSERT OR IGNORE INTO system_maintenances VALUES ('db_vacuum', 0);", NULL, NULL, NULL), "Seed Maint", ctx->handle);

	// Compile Prepared Statements
	check_rc(sqlite3_prepare_v3(ctx->handle, "INSERT OR IGNORE INTO devices VALUES (?, ?);", -1, SQLITE_PREPARE_PERSISTENT, &ctx->insert_device_stmt, NULL), "Prep Device Insert", ctx->handle);
	check_rc(sqlite3_prepare_v3(ctx->handle, "INSERT INTO climate_logs (device_id, temperature, humidity, timestamp, sensor_status) VALUES (?, ROUND(?, 1), ROUND(?, 1), ?, ?);", -1, SQLITE_PREPARE_PERSISTENT, &ctx->insert_log_stmt, NULL), "Prep Log Insert", ctx->handle);
	load_devices_from_db(ctx);

	return ctx;
}

static int
get_device_idx(struct db_context *ctx, int device_id) {
	int i = 0;
	while (i < ctx->device_count) {
		if (ctx->devices[i].hardware_id == device_id) { return i; }
		i++;
	}
	return -1;
}

void
db_register_device_if_new(struct db_context *ctx, int device_id) {
	if (ctx->device_count >= MAX_DEVICE) {
		fprintf(stderr, "FATAL ERROR: Database device count exceeds fixed RAM array limit (%d). Please increase MAX_DEVICE in your configuration.\n", MAX_DEVICE);
		exit(EXIT_FAILURE);
	}

	int i = get_device_idx(ctx, device_id);
	if (i != -1) { return; }

	printf("%d - New device connected. ", device_id);

	static const char *rooms[] = {"ROOM_A", "ROOM_B", "ROOM_C", "ROOM_D", "ROOM_E"};
	const char *assigned_room = rooms[rand() % 5];

	sqlite3_bind_int(ctx->insert_device_stmt, 1, device_id);
	sqlite3_bind_text(ctx->insert_device_stmt, 2, assigned_room, -1, SQLITE_STATIC);

	sqlite3_step(ctx->insert_device_stmt);
	sqlite3_reset(ctx->insert_device_stmt);

	ctx->devices[ctx->device_count].hardware_id = device_id;
	/* ctx->devices[ctx->device_count].location = assigned_room; */
	snprintf(ctx->devices[ctx->device_count].location, LOCATION_NAME_BUF_MAX, "%s", assigned_room);
	ctx->device_count++;
}

void
db_write_log_batch(struct db_context *ctx, const struct climate_log *log) {
	sqlite3_bind_int(ctx->insert_log_stmt, 1, log->device_id);
	sqlite3_bind_double(ctx->insert_log_stmt, 2, log->temp);
	sqlite3_bind_double(ctx->insert_log_stmt, 3, log->humid);
	sqlite3_bind_int64(ctx->insert_log_stmt, 4, log->timestamp);
	sqlite3_bind_int(ctx->insert_log_stmt, 5, log->sensor_failed);

	sqlite3_step(ctx->insert_log_stmt);
	sqlite3_reset(ctx->insert_log_stmt);
}

int
db_get_batch_limit() {
	return DB_BATCH_LIMIT;
}

void
db_begin_transaction(struct db_context *ctx) {
	sqlite3_exec(ctx->handle, "BEGIN TRANSACTION;", NULL, NULL, NULL);
}

void
db_end_transaction(struct db_context *ctx) {
	sqlite3_exec(ctx->handle, "COMMIT;", NULL, NULL, NULL);
}

char *
db_get_location(struct db_context *ctx, int device_id, char *dest_buf, size_t buf_len) {
	int i = get_device_idx(ctx, device_id);
	if (i != -1) {
		snprintf(dest_buf, buf_len, "%s", ctx->devices[i].location);
	} else {
		snprintf(dest_buf, buf_len, "UNKNOWN");
	}
	return dest_buf;
}

void
db_purge_old_logs(struct db_context *ctx) {
	const char *sql = "DELETE FROM climate_logs WHERE timestamp < unixepoch('now', '" RETENTION_PERIOD "');";
	check_rc(sqlite3_exec(ctx->handle, sql, NULL, NULL, NULL), "Purging logs", ctx->handle);
	int r = sqlite3_changes(ctx->handle);
	if (r) {
		printf("Purging Climate Logs...%d-row deleted.\n", r);
	}
}

void
db_run_maintenance(struct db_context *ctx) {
	const char *query = "SELECT EXISTS (SELECT 1 FROM system_maintenances WHERE task_name = 'db_vacuum' AND last_run_time <= unixepoch('now', '" RETENTION_PERIOD "'));";
	struct sqlite3_stmt *stmt;
	check_rc(sqlite3_prepare_v3(ctx->handle, query, -1, 0, &stmt, NULL), "Maint check parse", ctx->handle);

	int run_needed = 0;
	if (sqlite3_step(stmt) == SQLITE_ROW) run_needed = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);

	if (!run_needed) return;

	printf("Running scheduled database VACUUM...\n");
	sqlite3_reset(ctx->insert_log_stmt);
	sqlite3_reset(ctx->insert_device_stmt);

	check_rc(sqlite3_exec(ctx->handle, "VACUUM;", NULL, NULL, NULL), "Vacuum execute", ctx->handle);
	check_rc(sqlite3_exec(ctx->handle, "UPDATE system_maintenances SET last_run_time = unixepoch('now') WHERE task_name = 'db_vacuum';", NULL, NULL, NULL), "Maint track log", ctx->handle);
}

int
climate_log_serialize(struct db_context *ctx, const struct climate_log *log, char *dest_buf, size_t size) {
	char location[32];
	db_get_location(ctx, log->device_id, location, 32);
	return snprintf(dest_buf, size, "Device: %3d-%s | Temp: %.1fC | Humid: %.1f%% | Time: %s", log->device_id, location, log->temp, log->humid, ctime(&log->timestamp));
}

void
db_close(struct db_context *ctx) {
	sqlite3_finalize(ctx->insert_log_stmt);
	sqlite3_finalize(ctx->insert_device_stmt);
	sqlite3_close(ctx->handle);
	free(ctx);
}
