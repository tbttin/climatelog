#ifndef DB_H_INCLUDED
#define DB_H_INCLUDED

#include <sqlite3.h>
#include "sensor.h"

struct db_context;

struct db_context* db_init();
void db_close(struct db_context *ctx);

void db_register_device_if_new(struct db_context *ctx, int device_id);
void db_write_log_batch(struct db_context *ctx, const struct climate_log *log);
int db_get_batch_limit();
char * db_get_location(struct db_context *ctx, int device_id, char *dest_buf, size_t buf_len);
int climate_log_serialize(struct db_context *ctx, const struct climate_log *log, char *dest_buf, size_t buf_len);
void db_begin_transaction(struct db_context *ctx);
void db_end_transaction(struct db_context *ctx);

void db_purge_old_logs(struct db_context *ctx);
void db_run_maintenance(struct db_context *ctx);

#endif
