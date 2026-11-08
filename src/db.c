#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sensor.h"
#include "db.h"
#include "config.h"

#define LOG_ERR(fmt, ...) fprintf(stderr, "[ERROR] %s:%d (%s): " fmt "\n", \
				  __FILE__, __LINE__, __func__, ##__VA_ARGS__)

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

static void
check_rc(int rc, const char *msg, struct db_context *ctx) {
	if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW) {
		fprintf(stderr, "CRITICAL SQLite Failure (%s): %s\n", msg, sqlite3_errmsg(ctx->handle));

		// Centralized emergency rollback and closure sequence
		sqlite3_exec(ctx->handle, "ROLLBACK;", NULL, NULL, NULL);
		db_close(ctx);

		exit(EXIT_FAILURE); // Now it's safe to exit since memory and files are closed
	}
}

static void
db_fatal_panic(struct db_context *ctx, const char *mssg) {
	fprintf(stderr, "FATAL ENGINE COLAPE: %s\n", mssg);

	if (ctx && ctx->handle) {
		sqlite3_exec(ctx->handle, "ROLLBACK;", NULL, NULL, NULL);
	}

	db_close(ctx);
	exit(EXIT_FAILURE);
}

static bool
is_out_of_slot(struct db_context *ctx, int idx) {
	/* no 0th index, unused */
	return (idx >= (ctx->device_max + 1));
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
	fprintf(stdout, "New %d-slot array are allocated. For ID %d\n", new_size - ctx->device_max, target_idx);
	ctx->device_max = new_size;
	ctx->devices = new_all;

	return DB_SUCCESS;
}

static db_status_t
ensure_device_capacity(struct db_context *ctx, int idx) {
	if (idx <= 0 || idx > DB_DEVICE_MAX_CEILING) {
		LOG_ERR("Invalid device ID (%d), expected ID range: [%d, %d]", idx, 0, DB_DEVICE_MAX_CEILING);
		return DB_ERR_INVALID_INDEX;
	}

	if (is_out_of_slot(ctx, idx)) {
		return expand_device_arr(ctx, idx);
	}

	return DB_SUCCESS;
}

static db_status_t
load_devices_from_db(struct db_context *ctx) {
	struct sqlite3_stmt *select_stmt = NULL;
	check_rc(sqlite3_prepare_v3(ctx->handle, "SELECT * FROM devices;", -1, 0, &select_stmt, NULL), "Loading devices", ctx);
	int skip_count = 0;
	while (sqlite3_step(select_stmt) == SQLITE_ROW) {
		struct device dev;
		dev.hardware_id = sqlite3_column_int(select_stmt, 0);
		char *loc = (char *) sqlite3_column_text(select_stmt, 1);
		snprintf(dev.location, LOCATION_NAME_BUF_MAX, "%s",
			 loc ? loc : "UNKNOWN");

		db_status_t stt = ensure_device_capacity(ctx, dev.hardware_id);
		if (stt == DB_ERR_INVALID_INDEX) {
			if (DB_EXIT_ON_DB_INVALID_INDEX) {
				sqlite3_finalize(select_stmt);
				return stt;
			}

			fprintf(stdout, "Skip divice ID (%d)\n", dev.hardware_id);
			skip_count++;
			continue; /* Just skip this naughty ID */
		}

		/* Use device ID as its index in devices array */
		ctx->devices[dev.hardware_id] = dev;
		ctx->device_count++;
	}
	fprintf(stdout, "%d-device loaded succussfully, %d-device skipped\n", ctx->device_count, skip_count);
	sqlite3_finalize(select_stmt);
	return DB_SUCCESS;
}

struct db_context *
db_init() {
	// Step 1: Allocate context structure cleanly using calloc (all inner pointers start as NULL)
	struct db_context *ctx = calloc(1, sizeof(*ctx));
	if (!ctx) {
		LOG_ERR("Can NOT allocate memory for database context");
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
		fprintf(stderr, "Init Error: Database schema setup failed.\n");
		goto error_cleanup;
	}

	// Step 5: Compile Prepared Statements
	if (sqlite3_prepare_v3(ctx->handle, "INSERT INTO devices VALUES (?, ?);", -1, SQLITE_PREPARE_PERSISTENT, &ctx->insert_device_stmt, NULL) != SQLITE_OK ||
	    sqlite3_prepare_v3(ctx->handle, "INSERT INTO climate_logs (device_id, temperature, humidity, timestamp, sensor_status) VALUES (?, ROUND(?, 1), ROUND(?, 1), ?, ?);", -1, SQLITE_PREPARE_PERSISTENT, &ctx->insert_log_stmt, NULL) != SQLITE_OK) {
		fprintf(stderr, "Init Error: Failed to compile prepared statements.\n");
		goto error_cleanup;
	}

	// Step 6: Load historical data from disk
	if (load_devices_from_db(ctx) != DB_SUCCESS) {
		fprintf(stderr, "Init Error: Failed to populate data from database records.\n");
		goto error_cleanup;
	}

	return ctx; // All operations succeeded!

error_cleanup:
	// Centered Clean-Up Block: Safely handles partial allocations using your existing db_close
	db_close(ctx);
	return NULL; // Tell main() that initialization completely failed
}

static bool
is_new_device(struct db_context *ctx, int device_id) {
	/* Default value in calloc allocated array is 0, means this device id is new */
	return ctx->devices[device_id].hardware_id == 0;
}

db_status_t
db_register_device_if_new(struct db_context *ctx, int device_id) {
	if (ensure_device_capacity(ctx, device_id) == DB_ERR_INVALID_INDEX) {
		return DB_ERR_INVALID_INDEX;
	}

	if (!is_new_device(ctx, device_id)) return DB_SUCCESS;

	printf("%d - New device connected\n", device_id);

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

int
db_get_batch_limit() {
	return DB_BATCH_LIMIT;
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
db_purge_old_logs(struct db_context *ctx) {
	const char *sql = "DELETE FROM climate_logs WHERE timestamp < unixepoch('now', '" DB_RETENTION_PERIOD "');";
	check_rc(sqlite3_exec(ctx->handle, sql, NULL, NULL, NULL), "Purging logs", ctx);
	int r = sqlite3_changes(ctx->handle);
	if (r) {
		printf("Purging Climate Logs...%d-row deleted.\n", r);
	}
}

int
climate_log_serialize(struct db_context *ctx, const struct climate_log *log, char *dest_buf, int size) {
	int id = log->device_id;
	char *loc = ctx->devices[id].location;
	return snprintf(dest_buf, size, "Device: %2d-%s | Temp: %.1fC | Humid: %.1f%% | Time: %s", id, loc, log->temp, log->humid, ctime(&log->timestamp));
}

const char *
db_strerror(db_status_t stt) {
	switch (stt) {
		case DB_SUCCESS: return "Success";
		case DB_ERR_INVALID_INDEX: return "Invalid device ID";
		default: return "Uknown enternal database error code";
	}
}

void
db_rollback(struct db_context *ctx) {
	sqlite3_exec(ctx->handle, "ROLLBACK;", NULL, NULL, NULL);
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
