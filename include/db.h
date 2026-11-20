#ifndef DB_H_INCLUDED
#define DB_H_INCLUDED

#include "sensor.h"

typedef enum db_mode {
	DB_MODE_CLI = 0,
	DB_MODE_DAEMON,
} db_mode_t;

typedef enum db_status {
	DB_SUCCESS = 0,
	DB_ID_ALREADY_EXISTS,

	DB_ERR,
	DB_ERR_NOMEM,
	DB_ERR_ID_OUT_OF_BOUNDS,
	DB_ERR_ID_EXCEEDS_CEILING,
	DB_ERR_BUSY,
	DB_ERR_NOT_FOUND,
	DB_ERR_PERM,

	DB_SAFE_STATUS_RANGE,
} db_status_t;

struct db_context;

db_status_t db_init(struct db_context **out_ctx, db_mode_t mode);
db_status_t db_flush(struct db_context *ctx);
db_status_t db_ingest_log(struct db_context *ctx, const struct climate_log *log);
const char *db_strstt(db_status_t s);
void db_close(struct db_context *ctx);

#endif /* DB_H_INCLUDED */
