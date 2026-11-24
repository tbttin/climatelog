#include <stdbool.h>
#include "db_internal.h"
#include "cli.h"
#include "console_log.h"

struct climate_data {
	double temp;
	double humid;
	time_t timestamp;
	char *location;
	int device_id;
	bool sensor_failed;
};

static void cli_print_data(struct climate_data);

db_status_t db_cli_tail(struct db_context *ctx, const struct cli_args *args) {
	struct sqlite3_stmt *sl_stmt;
	SQL_TRY(sqlite3_prepare_v3(ctx->handle, "SELECT device_id, temperature, humidity, timestamp, sensor_status FROM climate_logs ORDER BY timestamp DESC LIMIT ?;", -1, SQLITE_PREPARE_NORMALIZE, &sl_stmt, NULL));

	sqlite3_bind_int(sl_stmt, 1, args->num);

	db_status_t status;
	while ((status = sqlite3_step(sl_stmt)) == SQLITE_ROW) {
		struct climate_data d;
		d.device_id = sqlite3_column_int(sl_stmt, 0);
		d.temp = sqlite3_column_double(sl_stmt, 1);
		d.humid = sqlite3_column_double(sl_stmt, 2);
		d.timestamp = sqlite3_column_int64(sl_stmt, 3);
		d.sensor_failed = sqlite3_column_int(sl_stmt, 4);
		db_get_device_loc(ctx, &d.location, d.device_id);
		cli_print_data(d);
	}

	if (status != SQLITE_DONE) {
		status = db_check_sqlite_rc(status, ctx->handle);
		sqlite3_finalize(sl_stmt);
		return status;
	}

	sqlite3_finalize(sl_stmt);
	return DB_SUCCESS;
}

db_status_t db_cli_print_status(struct db_context *ctx, const struct cli_args *unused) {
	(void)unused;
	const char *sql = "SELECT COUNT(*),"
		"COALESCE(SUM(sensor_status), 0)," /* SUM() returns NULL */
		"(SELECT COUNT(*) FROM devices) FROM climate_logs;";
	struct sqlite3_stmt *stmt = NULL;

	SQL_TRY(sqlite3_prepare_v3(ctx->handle, sql, -1, 0, &stmt, NULL));
	db_status_t status = db_check_sqlite_rc(sqlite3_step(stmt), ctx->handle);
	if (status != DB_SUCCESS) {
		sqlite3_finalize(stmt);
		return status;
	}

	LOG_INFO("Devices: %d | Records: %d | Failed records: %d", sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 0), sqlite3_column_int(stmt, 1));

	sqlite3_finalize(stmt);
	return DB_SUCCESS;
}

void cli_print_data(struct climate_data d) {
	char time_buf[32];
	strftime(time_buf, sizeof time_buf, "%Y-%m-%d_%H:%M:%S", localtime(&d.timestamp));
	LOG_INFO("dev_id=%3d, loc=%s, temp=%.1f, humid=%.1f, status=%s, timestamp=%s",
		 d.device_id, d.location ? d.location : "Unknown", d.temp, d.humid, d.sensor_failed ? "Failed" : "Ok", time_buf);
}

