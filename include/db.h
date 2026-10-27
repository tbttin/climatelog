#ifndef DB_H_INCLUDED
#define DB_H_INCLUDED

struct sqlite3* init_sqlite_db();

struct sqlite3_stmt * prepare_sqlite_insert_stmt(struct sqlite3 *pdb);

int get_db_batch_limit();

void bind_climate_log_batch(struct sqlite3 *pdb, struct sqlite3_stmt *stmt, const struct climate_log *log);

void close_sqlite3_db(struct sqlite3 *pdb);

#endif /* DB_H_INCLUDED */

