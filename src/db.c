#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "db_internal.h"
#include "console_log.h"

static bool is_id_out_of_ceilings(int id);
static bool is_new_device(struct db_context *ctx, int device_id);
static db_status_t expand_device_arr(struct db_context *ctx, int target_idx);
static db_status_t ensure_device_capacity(struct db_context *ctx, int idx);
static db_status_t load_devices_from_db(struct db_context *ctx);
static db_status_t db_register_device_if_new(struct db_context *ctx, int device_id);
static db_status_t db_write_log_batch(struct db_context *ctx, const struct climate_log *log);
static db_status_t db_purge_old_logs(struct db_context *ctx);
static db_status_t db_begin_transaction(struct db_context *ctx);
static db_status_t db_end_transaction(struct db_context *ctx);
static db_status_t db_rollback(struct db_context *ctx);
static int db_get_batch_limit();
static int db_get_ceiling();

db_status_t db_init(struct db_context **out_ctx, db_mode_t mode) {
	struct db_context *ctx = calloc(1, sizeof(*ctx));
	if (!ctx) { return DB_ERR_NOMEM; }
	*out_ctx = ctx;

	const char *db_file = getenv("CLIMATELOG_DB_FILE") ? getenv("CLIMATELOG_DB_FILE") : CLIMATELOG_DB_FILE;
	int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
	if (mode == DB_MODE_CLI)
	{
		flags = SQLITE_OPEN_READONLY;
	}

	SQL_TRY(sqlite3_open_v2(db_file, &ctx->handle, flags, NULL));
	SQL_TRY(sqlite3_busy_timeout(ctx->handle, SQLITE_TIMEOUT_BUSY));

	ctx->device_max = ctx->device_count = 0;
	db_status_t status;
	if ((status = ensure_device_capacity(ctx, db_get_batch_limit() / 4)) != DB_SUCCESS) { return status; }
	if ((status = load_devices_from_db(ctx)) != DB_SUCCESS) { return status; }

	if (mode == DB_MODE_CLI) { return DB_SUCCESS; }

	LOG_INFO("DB file: %s, batch size: %d, retention time: %s, device ID ceiling: %d", db_file, db_get_batch_limit(), DB_RETENTION_PERIOD, db_get_ceiling());

	const char *init_sql =
		"PRAGMA journal_mode = WAL;"
		"CREATE TABLE IF NOT EXISTS climate_logs (id INTEGER PRIMARY KEY AUTOINCREMENT, device_id INTEGER NOT NULL, temperature REAL NOT NULL, humidity REAL NOT NULL, timestamp INTEGER NOT NULL, sensor_status INTEGER NOT NULL);"
		"CREATE TABLE IF NOT EXISTS devices (hardware_id INTEGER PRIMARY KEY, location TEXT NOT NULL);"
		"CREATE INDEX IF NOT EXISTS idx_logs_timestamp ON climate_logs (timestamp);";
	SQL_TRY(sqlite3_exec(ctx->handle, init_sql, NULL, NULL, NULL));

	SQL_TRY(sqlite3_prepare_v3(ctx->handle, "INSERT INTO devices VALUES (?, ?);", -1, SQLITE_PREPARE_PERSISTENT, &ctx->insert_device_stmt, NULL));
	SQL_TRY(sqlite3_prepare_v3(ctx->handle, "INSERT INTO climate_logs (device_id, temperature, humidity, timestamp, sensor_status) VALUES (?, ROUND(?, 1), ROUND(?, 1), ?, ?);", -1, SQLITE_PREPARE_PERSISTENT, &ctx->insert_log_stmt, NULL));
	SQL_TRY(sqlite3_prepare_v3(ctx->handle, "DELETE FROM climate_logs WHERE timestamp < unixepoch('now', ?);", -1, SQLITE_PREPARE_PERSISTENT, &ctx->purge_device_stmt, NULL));

	return DB_SUCCESS;
}

db_status_t db_flush(struct db_context *ctx) {
	if (!ctx || !ctx->in_transaction) { return DB_SUCCESS; }

	if (ctx->batch_count) {
		db_status_t status = db_end_transaction(ctx);
		if (status != DB_SUCCESS) { return status; }
		ctx->batch_count = 0;
	}
	else {
		db_rollback(ctx);
	}

	ctx->in_transaction = false;
	return DB_SUCCESS;
}

db_status_t db_ingest_log(struct db_context *ctx, const struct climate_log *log) {
	db_status_t status;

	if (!ctx->in_transaction) {
		if ((status = db_begin_transaction(ctx)) != DB_SUCCESS) { return status; }
		ctx->in_transaction = true;
	}

	status = db_register_device_if_new(ctx, log->device_id);
	char *dev_loc = NULL;

	if (status == DB_ERR_ID_EXCEEDS_CEILING) {
		LOG_WARN("Skipping log processing cycle for problematic device id=%d", log->device_id);
		return DB_SUCCESS;
	} else if (status == DB_SUCCESS) {
		db_get_device_loc(ctx, &dev_loc, log->device_id);
		LOG_INFO("New device registered (id=%d dev_loc=%s)", log->device_id, dev_loc);
	} else if (status != DB_ID_ALREADY_EXISTS) {
		goto rollback;
	}

	if (log->sensor_failed) {
		db_get_device_loc(ctx, &dev_loc, log->device_id);
		LOG_WARN("Sensor fault: dev=%02d dev_loc=%-6s temp=%4.1fC humid=%4.1f%%", log->device_id, dev_loc, log->temp, log->humid);
	}

	if ((status = db_write_log_batch(ctx, log)) != DB_SUCCESS) {
		goto rollback;
	}

	if (++ctx->batch_count >= db_get_batch_limit()) {
		if ((status = db_flush(ctx)) != DB_SUCCESS) { return status; }
		db_purge_old_logs(ctx);
	}

	return DB_SUCCESS;

rollback:
	db_rollback(ctx);
	ctx->in_transaction = false;
	ctx->batch_count = 0;

	return status;
}

void db_close(struct db_context *ctx) {
	if (!ctx) return;

	db_flush(ctx);
	free(ctx->devices);
	sqlite3_finalize(ctx->insert_log_stmt);
	sqlite3_finalize(ctx->insert_device_stmt);
	sqlite3_finalize(ctx->purge_device_stmt);
	sqlite3_close_v2(ctx->handle);
	free(ctx);
}

db_status_t db_register_device_if_new(struct db_context *ctx, int device_id) {
	db_status_t status;
	if ((status = ensure_device_capacity(ctx, device_id)) != DB_SUCCESS) { return status; }

	if (!is_new_device(ctx, device_id)) { return DB_ID_ALREADY_EXISTS; }

	static const char *rooms[] = { "ROOM_A", "ROOM_B", "ROOM_C", "ROOM_D", "ROOM_E" };
	const char *assigned_room = rooms[rand() % 5];

	sqlite3_bind_int(ctx->insert_device_stmt, 1, device_id);
	sqlite3_bind_text(ctx->insert_device_stmt, 2, assigned_room, -1, SQLITE_STATIC);
	status = db_check_sqlite_rc(sqlite3_step(ctx->insert_device_stmt), ctx->handle);
	sqlite3_reset(ctx->insert_device_stmt);
	if (status != DB_SUCCESS) { return status; }

	struct device d;
	d.hardware_id = device_id;
	snprintf(d.location, LOCATION_NAME_BUF_MAX, "%s", assigned_room);
	ctx->devices[device_id] = d;
	ctx->device_count++;

	return DB_SUCCESS;
}

db_status_t db_write_log_batch(struct db_context *ctx, const struct climate_log *log) {
	sqlite3_bind_int(ctx->insert_log_stmt, 1, log->device_id);
	sqlite3_bind_double(ctx->insert_log_stmt, 2, log->temp);
	sqlite3_bind_double(ctx->insert_log_stmt, 3, log->humid);
	sqlite3_bind_int64(ctx->insert_log_stmt, 4, log->timestamp);
	sqlite3_bind_int(ctx->insert_log_stmt, 5, log->sensor_failed);

	db_status_t status = db_check_sqlite_rc(sqlite3_step(ctx->insert_log_stmt), ctx->handle);
	sqlite3_reset(ctx->insert_log_stmt);

	return (status != DB_SUCCESS) ? status : DB_SUCCESS;
}

db_status_t db_purge_old_logs(struct db_context *ctx) {
	sqlite3_bind_text(ctx->purge_device_stmt, 1, DB_RETENTION_PERIOD, -1, SQLITE_STATIC);
	SQL_TRY(sqlite3_step(ctx->purge_device_stmt));
	sqlite3_reset(ctx->purge_device_stmt);

	return DB_SUCCESS;
}

db_status_t db_begin_transaction(struct db_context *ctx) {
	SQL_TRY(sqlite3_exec(ctx->handle, "BEGIN TRANSACTION;", NULL, NULL, NULL));

	return DB_SUCCESS;
}

db_status_t db_end_transaction(struct db_context *ctx) {
	SQL_TRY(sqlite3_exec(ctx->handle, "COMMIT;", NULL, NULL, NULL));

	return DB_SUCCESS;
}

db_status_t db_rollback(struct db_context *ctx) {
	SQL_TRY(sqlite3_exec(ctx->handle, "ROLLBACK;", NULL, NULL, NULL));

	return DB_SUCCESS;
}

int db_get_ceiling() {
	int ceiling = DB_DEVICE_MAX_CEILING;
	if (ceiling < 4) {
		ceiling = 4;
	  	LOG_WARN("Ceiling is too small, new ceiling: %d", ceiling);
	}
	if (ceiling > 10000) {
		ceiling = 10000;
	  	LOG_WARN("Ceiling is too big, new ceiling: %d", ceiling);
	}

	return ceiling;
}

int db_get_batch_limit() {
	int limit = DB_BATCH_LIMIT;
	if (limit < 0 || limit > 1800) { limit = 1800; }

	return limit;
}

bool is_id_out_of_ceilings(int id) {
	int ceiling = db_get_ceiling();
	return (id > ceiling || id < 1);
}

bool is_new_device(struct db_context *ctx, int id) {
	if (is_id_out_of_bounds(ctx, id)) { return true; }

	/* Default value in calloc allocated array is 0, means this device id is new */
	return ctx->devices[id].hardware_id == 0;
}

db_status_t expand_device_arr(struct db_context *ctx, int target_idx) {
	int new_size = 2 * ctx->device_max;
	if (target_idx > new_size) { new_size = target_idx; }
	if (new_size > db_get_ceiling() ) { new_size = db_get_ceiling(); }

	struct device *new_all = realloc(ctx->devices, sizeof(*ctx->devices) * (new_size + 1));
	if (!new_all) {
		return DB_ERR_NOMEM;
	}

	/* Set all bytes to 0 (for device ID existence check) */
	memset(new_all + ctx->device_max + 1, 0, sizeof(*new_all) * (new_size - ctx->device_max));
	ctx->device_max = new_size;
	ctx->devices = new_all;

	return DB_SUCCESS;
}

db_status_t ensure_device_capacity(struct db_context *ctx, int id) {
	if (is_id_out_of_ceilings(id)) {
		return DB_ERR_ID_EXCEEDS_CEILING;
	}

	if (is_id_out_of_bounds(ctx, id)) {
		return expand_device_arr(ctx, id);
	}

	return DB_SUCCESS;
}

db_status_t load_devices_from_db(struct db_context *ctx) {
	struct sqlite3_stmt *select_stmt = NULL;
	SQL_TRY(sqlite3_prepare_v3(ctx->handle, "SELECT * FROM devices;", -1, 0, &select_stmt, NULL));
	int skip_count = 0;
	while (sqlite3_step(select_stmt) == SQLITE_ROW) {
		struct device dev;
		dev.hardware_id = sqlite3_column_int(select_stmt, 0);
		char *loc = (char *) sqlite3_column_text(select_stmt, 1);
		snprintf(dev.location, LOCATION_NAME_BUF_MAX, "%s",
			 loc ? loc : "UNKNOWN");

		db_status_t status = ensure_device_capacity(ctx, dev.hardware_id);
		if (status == DB_ERR_ID_EXCEEDS_CEILING) {
			if (DB_EXIT_ON_DB_INVALID_INDEX) {
				sqlite3_finalize(select_stmt);
				return status;
			}

			LOG_WARN("Device ID (%d) in database exceeds ceiling (%d)", dev.hardware_id, db_get_ceiling());
			skip_count++;
			continue; /* Warning and skip this naughty ID */
		}
		else if (status != DB_SUCCESS) {
			sqlite3_finalize(select_stmt);
			return status;
		}

		if (is_new_device(ctx, dev.hardware_id)) {
			ctx->devices[dev.hardware_id] = dev; /* Use device ID as its index in devices array */
			ctx->device_count++;
		}
	}

	LOG_INFO("%d-row loaded to memory successfully (%d total in database, %d skipped (due to ceiling))", ctx->device_count, ctx->device_count + skip_count, skip_count);

	sqlite3_finalize(select_stmt);
	return DB_SUCCESS;
}

db_status_t db_check_sqlite_rc(int rc, struct sqlite3 *ctx) {
	if (rc == SQLITE_OK || rc == SQLITE_DONE || rc == SQLITE_ROW) {
		return DB_SUCCESS;
	}

	int ext_rc = ctx ? sqlite3_extended_errcode(ctx) : rc;
	const char *err_mssg = ctx ? sqlite3_errmsg(ctx) : sqlite3_errstr(rc);

	LOG_ERR("SQLite error (rc = %d, ext = %d): %s", rc, ext_rc, err_mssg);

	switch (rc) {
		case SQLITE_BUSY:
		case SQLITE_LOCKED:
			return DB_ERR_BUSY;
		case SQLITE_CANTOPEN:
			return DB_ERR_NOT_FOUND;
		case SQLITE_PERM:
		case SQLITE_READONLY:
			return DB_ERR_PERM;
		default:
			return DB_ERR;
	}
}

const char *db_strstt(db_status_t s) {
	switch (s) {
		case DB_SUCCESS: return "Success";
		case DB_ID_ALREADY_EXISTS: return "ID existed";
		case DB_ERR_NOMEM: return "Run out of memory";
		case DB_ERR_ID_OUT_OF_BOUNDS: return "ID out of bounds";
		case DB_ERR_ID_EXCEEDS_CEILING: return "ID out of ceilings";
		case DB_ERR_BUSY: return "DB busy";
		case DB_ERR_NOT_FOUND: return "File not found";
		case DB_ERR_PERM: return "Not have permission";
		default: return "Unknown error";
	}
}

