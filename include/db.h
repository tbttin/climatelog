#ifndef DB_H_INCLUDED
#define DB_H_INCLUDED

#include <sqlite3.h>
#include "sensor.h"

typedef enum {
	DB_SUCCESS = 0,
	DB_ERR_INVALID_INDEX,
	DB_SAFE_RANGE,
} db_status_t;

struct db_context;

struct db_context* db_init();
void db_close(struct db_context *ctx);

db_status_t db_register_device_if_new(struct db_context *ctx, int device_id);
void db_write_log_batch(struct db_context *ctx, const struct climate_log *log);
int db_get_batch_limit();
int climate_log_serialize(struct db_context *ctx, const struct climate_log *log, char *dest, int size);
void db_begin_transaction(struct db_context *ctx);
void db_end_transaction(struct db_context *ctx);
void db_rollback(struct db_context *ctx);

void db_purge_old_logs(struct db_context *ctx);
const char * db_strerror(db_status_t stt);

#endif
