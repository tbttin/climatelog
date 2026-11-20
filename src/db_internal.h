#ifndef DB_INTERNAL_H_INCLUDED
#define DB_INTERNAL_H_INCLUDED

#include <stdbool.h>
#include <sqlite3.h>
#include "config.h"
#include "db.h"

#define SQL_TRY(expr) do { \
	db_status_t _s = db_check_sqlite_rc((expr), ctx->handle); \
	if (_s != DB_SUCCESS) return _s; \
} while (0)

struct device {
	int hardware_id;
	char location[LOCATION_NAME_BUF_MAX]; /* More optimized with room_id */
};

struct db_context {
	struct sqlite3 *handle;
	struct sqlite3_stmt *insert_log_stmt;
	struct sqlite3_stmt *insert_device_stmt;
	struct sqlite3_stmt *purge_device_stmt;

	bool in_transaction;
	int batch_count;

	struct device *devices;
	int device_count;
	int device_max;
};

bool is_id_out_of_bounds(struct db_context *ctx, int id);
db_status_t db_check_sqlite_rc(int rc, struct sqlite3 *ctx);
db_status_t db_get_device_loc(struct db_context *ctx, char **res_loc, int idx);

#endif /* DB_INTERNAL_H_INCLUDED */

