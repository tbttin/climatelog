#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include "sensor.h"
#include "db.h"
#include "config.h"

/* #define LOG_ERR(fmt, ...) fprintf(stderr, "[ERROR] %s:%d (%s): " fmt "\n", \ */
/* 				  __FILE__, __LINE__, __func__, ##__VA_ARGS__) */

struct device {
	int hardware_id;
	char location[LOCATION_NAME_BUF_MAX];
};

struct db_context {
	struct sqlite3 *handle;
	struct sqlite3_stmt *insert_log_stmt;
	struct sqlite3_stmt *insert_device_stmt;

	struct device *devices;
	int device_count;
	int device_max;
};

static bool is_id_out_of_bounds(struct db_context *ctx, int id);
static bool is_id_out_of_ceiling(int id);
static bool is_new_device(struct db_context *ctx, int device_id);
static db_status_t ensure_device_capacity(struct db_context *ctx, int idx);
static db_status_t expand_device_arr(struct db_context *ctx, int target_idx);
static db_status_t load_devices_from_db(struct db_context *ctx);
static void check_rc(int rc, const char *msg, struct db_context *ctx);
static void db_fatal_panic(struct db_context *ctx, const char *mssg);

struct db_context *
db_init() {
	// Step 1: Allocate context structure cleanly using calloc (all inner pointers start as NULL)
	struct db_context *ctx = calloc(1, sizeof(*ctx));
	if (!ctx) {
		LOG_ERR("Can NOT allocate memory for database context");
		goto error_cleanup;
		return NULL;
	}

	// Step 2: Open SQLite connection
	const char *db_file = getenv("CLIMATELOG_DB_FILE") ? getenv("CLIMATELOG_DB_FILE") : CLIMATELOG_DB_FILE;
	int rc = sqlite3_open_v2(db_file, &ctx->handle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
	if (rc != SQLITE_OK) {
		LOG_ERR("Can't open database file \"%s\". Error code: %d, %s", db_file, sqlite3_extended_errcode(ctx->handle), sqlite3_errmsg(ctx->handle));
		goto error_cleanup; // Jump directly to cleanup path
	}

	// Step 3: Allocate the tracking array
	ctx->device_max = ctx->device_count = 0;
	if (ensure_device_capacity(ctx, DB_DEVICE_MAX_INIT) != DB_SUCCESS) {
		goto error_cleanup;
	}
	/* ctx->devices[0].hardware_id = 0; */

	// Step 4: Schema execution (Using cascading checks)
	if (sqlite3_exec(ctx->handle, "PRAGMA journal_mode = WAL;", NULL, NULL, NULL) != SQLITE_OK ||
	    sqlite3_exec(ctx->handle, "PRAGMA auto_vacuum = FULL;", NULL, NULL, NULL) != SQLITE_OK ||
	    sqlite3_busy_timeout(ctx->handle, SQLITE_TIMEOUT_BUSY) != SQLITE_OK ||
	    sqlite3_exec(ctx->handle, "CREATE TABLE IF NOT EXISTS devices (hardware_id INTEGER PRIMARY KEY, location TEXT NOT NULL);", NULL, NULL, NULL) != SQLITE_OK ||
	    sqlite3_exec(ctx->handle, "CREATE TABLE IF NOT EXISTS climate_logs (id INTEGER PRIMARY KEY AUTOINCREMENT, device_id INTEGER NOT NULL, temperature REAL NOT NULL, humidity REAL NOT NULL, timestamp INTEGER NOT NULL, sensor_status INTEGER NOT NULL);", NULL, NULL, NULL) != SQLITE_OK ||
	    sqlite3_exec(ctx->handle, "CREATE INDEX IF NOT EXISTS idx_logs_timestamp ON climate_logs (timestamp);", NULL, NULL, NULL) != SQLITE_OK) {
		LOG_ERR("Init Error: Database schema setup failed");
		goto error_cleanup;
	}

	// Step 5: Compile Prepared Statements
	if (sqlite3_prepare_v3(ctx->handle, "INSERT INTO devices VALUES (?, ?);", -1, SQLITE_PREPARE_PERSISTENT, &ctx->insert_device_stmt, NULL) != SQLITE_OK ||
	    sqlite3_prepare_v3(ctx->handle, "INSERT INTO climate_logs (device_id, temperature, humidity, timestamp, sensor_status) VALUES (?, ROUND(?, 1), ROUND(?, 1), ?, ?);", -1, SQLITE_PREPARE_PERSISTENT, &ctx->insert_log_stmt, NULL) != SQLITE_OK) {
		LOG_ERR("Init Error: Failed to compile prepared statements");
		goto error_cleanup;
	}

	// Step 6: Load historical data from disk
	if (load_devices_from_db(ctx) != DB_SUCCESS) {
		LOG_ERR("Init Error: Failed to populate data from database records");
		goto error_cleanup;
	}

	return ctx; // All operations succeeded!

error_cleanup:
	// Centered Clean-Up Block: Safely handles partial allocations using your existing db_close
	db_close(ctx);
	return NULL; // Tell main() that initialization completely failed
}

db_status_t
db_register_device_if_new(struct db_context *ctx, int device_id) {
	if (ensure_device_capacity(ctx, device_id) == DB_ERR_ID_EXCEEDS_CEILING) {
		return DB_ERR_ID_EXCEEDS_CEILING;
	}

	if (!is_new_device(ctx, device_id)) return DB_ID_ALREADY_EXISTS;

	static const char *rooms[] = {"ROOM_A", "ROOM_B", "ROOM_C", "ROOM_D", "ROOM_E"};
	const char *assigned_room = rooms[rand() % 5];

	sqlite3_bind_int(ctx->insert_device_stmt, 1, device_id);
	sqlite3_bind_text(ctx->insert_device_stmt, 2, assigned_room, -1, SQLITE_STATIC);

	sqlite3_step(ctx->insert_device_stmt);
	sqlite3_reset(ctx->insert_device_stmt);

	struct device d;
	d.hardware_id = device_id;
	snprintf(d.location, LOCATION_NAME_BUF_MAX, "%s", assigned_room);
	ctx->devices[device_id] = d;
	ctx->device_count++;

	return DB_SUCCESS;
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

void
db_purge_old_logs(struct db_context *ctx) {
	const char *sql = "DELETE FROM climate_logs WHERE timestamp < unixepoch('now', '" DB_RETENTION_PERIOD "');";
	check_rc(sqlite3_exec(ctx->handle, sql, NULL, NULL, NULL), "Purging logs", ctx);
	int r = sqlite3_changes(ctx->handle);
	if (r) {
		LOG_INFO("Purging Climate Logs...%d-row deleted", r);
	}
}

void
db_begin_transaction(struct db_context *ctx) {
	check_rc(sqlite3_exec(ctx->handle, "BEGIN TRANSACTION;", NULL, NULL, NULL), "Starting database transaction", ctx);

}

void
db_end_transaction(struct db_context *ctx) {
	check_rc(sqlite3_exec(ctx->handle, "COMMIT;", NULL, NULL, NULL), "Ending database transaction", ctx);
}

void
db_rollback(struct db_context *ctx) {
	sqlite3_exec(ctx->handle, "ROLLBACK;", NULL, NULL, NULL);
}

int
db_get_batch_limit() {
	return DB_BATCH_LIMIT;
}

db_status_t
db_get_device_loc(struct db_context *ctx, const char **ret_loc, int id) {
	if (is_id_out_of_bounds(ctx, id)) {
		LOG_ERR("%s (%d)", db_strerror(DB_ERR_ID_OUT_OF_BOUNDS), id);
		*ret_loc = NULL;
		return DB_ERR_ID_OUT_OF_BOUNDS;
	}

	*ret_loc = ctx->devices[id].location;
	return DB_SUCCESS;
}

int
climate_log_serialize(struct db_context *ctx, const struct climate_log *log, char *dest_buf, int size) {
	int id = log->device_id;
	char *loc = ctx->devices[id].location;
	const char *status = log->sensor_failed ? "Bad" : "Good";
	return snprintf(dest_buf, size, "dev=%02d loc=%-6s temp=%4.1fC humid=%4.1f%% status=%s",
                    id, loc, log->temp, log->humid, status);
}

const char *
db_strerror(db_status_t stt) {
	switch (stt) {
		case DB_SUCCESS: return "Success";
		case DB_ERR_ID_OUT_OF_BOUNDS:
		case DB_ERR_ID_EXCEEDS_CEILING: return "Invalid device ID";
		default: return "Unknown enternal database error code";
	}
}

void
db_close(struct db_context *ctx) {
	if (!ctx) return;

	free(ctx->devices);
	sqlite3_finalize(ctx->insert_log_stmt);
	sqlite3_finalize(ctx->insert_device_stmt);
	sqlite3_close_v2(ctx->handle);
	free(ctx);
}

static bool
is_id_out_of_bounds(struct db_context *ctx, int id) {
	return (id >= (ctx->device_max + 1) || id < 1);
}

static bool
is_id_out_of_ceiling(int id) {
	return (id > DB_DEVICE_MAX_CEILING || id < 1);
}

static bool
is_new_device(struct db_context *ctx, int id) {
	if (is_id_out_of_bounds(ctx, id)) return 0;

	/* Default value in calloc allocated array is 0, means this device id is new */
	return ctx->devices[id].hardware_id == 0;
}

static db_status_t
expand_device_arr(struct db_context *ctx, int target_idx) {
	int new_size = 2 * ctx->device_max;
	if (target_idx > new_size) { new_size = target_idx; }
	if (new_size > DB_DEVICE_MAX_CEILING) { new_size = DB_DEVICE_MAX_CEILING; }

	struct device *new_all = realloc(ctx->devices, sizeof(*ctx->devices) * (new_size + 1));
	if (!new_all) {
		LOG_ERR("Can NOT re-allocate devices array");
		db_fatal_panic(ctx, "Exit immediately");
	}

	memset(new_all + ctx->device_max + 1, 0, sizeof(*new_all) * (new_size - ctx->device_max));
	ctx->device_max = new_size;
	ctx->devices = new_all;

	return DB_SUCCESS;
}

static db_status_t
ensure_device_capacity(struct db_context *ctx, int id) {
	if (is_id_out_of_ceiling(id)) {
		LOG_ERR("Invalid device ID (%d), valid range %d..%d", id, 1, DB_DEVICE_MAX_CEILING);
		return DB_ERR_ID_EXCEEDS_CEILING;
	}

	if (is_id_out_of_bounds(ctx, id)) {
		return expand_device_arr(ctx, id);
	}

	return DB_SUCCESS;
}

static db_status_t
load_devices_from_db(struct db_context *ctx) {
	struct sqlite3_stmt *select_stmt = NULL;
	check_rc(sqlite3_prepare_v3(ctx->handle, "SELECT * FROM devices;", -1, 0, &select_stmt, NULL), "Loading devices from database", ctx);
	int skip_count = 0;
	while (sqlite3_step(select_stmt) == SQLITE_ROW) {
		struct device dev;
		dev.hardware_id = sqlite3_column_int(select_stmt, 0);
		char *loc = (char *) sqlite3_column_text(select_stmt, 1);
		snprintf(dev.location, LOCATION_NAME_BUF_MAX, "%s",
			 loc ? loc : "UNKNOWN");

		db_status_t stt = ensure_device_capacity(ctx, dev.hardware_id);
		if (stt == DB_ERR_ID_EXCEEDS_CEILING) {
			if (DB_EXIT_ON_DB_INVALID_INDEX) {
				sqlite3_finalize(select_stmt);
				return stt;
			}

			LOG_INFO("Skipping divice (%d)", dev.hardware_id);
			skip_count++;
			continue; /* Warning and skip this naughty ID */
		}

		ctx->devices[dev.hardware_id] = dev; /* Use device ID as its index in devices array */
		ctx->device_count++;
	}
	LOG_INFO("%d-device memory loaded succussfully, %d-device skipped", ctx->device_count, skip_count);
	sqlite3_finalize(select_stmt);
	return DB_SUCCESS;
}

static void
check_rc(int rc, const char *msg, struct db_context *ctx) {
	if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW) {
		LOG_ERR("CRITICAL SQLite Failure (%s): %s", msg, sqlite3_errmsg(ctx->handle));

		// Centralized emergency rollback and closure sequence
		sqlite3_exec(ctx->handle, "ROLLBACK;", NULL, NULL, NULL);
		db_close(ctx);

		exit(EXIT_FAILURE); // Now it's safe to exit since memory and files are closed
	}
}

static void
db_fatal_panic(struct db_context *ctx, const char *mssg) {
	LOG_ERR("FATAL ENGINE COLLASPE: %s", mssg);

	if (ctx && ctx->handle) {
		sqlite3_exec(ctx->handle, "ROLLBACK;", NULL, NULL, NULL);
	}

	db_close(ctx);
	exit(EXIT_FAILURE);
}

