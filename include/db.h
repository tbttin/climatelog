#ifndef DB_H_INCLUDED
#define DB_H_INCLUDED

#include "sensor.h"

#define LOG_ERR(fmt, ...)  fprintf(stderr, "<3>[%s:%d] " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) fprintf(stderr, "<4>" fmt "\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) fprintf(stdout, "<6>" fmt "\n", ##__VA_ARGS__)

typedef enum {
	DB_SUCCESS = 0,
	DB_ID_ALREADY_EXISTS,

	DB_ERR,
	DB_ERR_ID_OUT_OF_BOUNDS,
	DB_ERR_ID_EXCEEDS_CEILING,

	DB_SAFE_STATUS_RANGE,
} db_status_t;

struct db_context;

/* Database core functions */
struct db_context* db_init();
db_status_t db_register_device_if_new(struct db_context *ctx, int device_id);
void db_write_log_batch(struct db_context *ctx, const struct climate_log *log);
void db_purge_old_logs(struct db_context *ctx);
void db_begin_transaction(struct db_context *ctx);
void db_end_transaction(struct db_context *ctx);
void db_rollback(struct db_context *ctx);
void db_close(struct db_context *ctx);

/* Database utilities */
int db_get_batch_limit();
db_status_t db_get_device_loc(struct db_context *ctx, const char **res_loc, int idx);
const char * db_strerror(db_status_t stt);

#endif
