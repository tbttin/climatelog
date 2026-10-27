#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include "sensor.h"
#include "db.h"
#include "config.h"

static const char *
get_db_file() {
	const char *db_file = getenv("CLIMATELOG_DB_FILE");
	if (db_file) {
		return db_file;
	}
	return SQLITE_DB_FILE;
}

static void
check_sqlite_rc(int rc, const char *msg, struct sqlite3 *pdb) {
    if (rc != SQLITE_OK && rc != SQLITE_DONE) {
        fprintf(stderr, "SQLite Error (%s): %s\n", msg, sqlite3_errmsg(pdb));
        if (pdb) {
		sqlite3_close(pdb);
	}
        exit(EXIT_FAILURE);
    }
}

struct sqlite3 *
init_sqlite_db() {
	const char *db_file = get_db_file();
	struct sqlite3 *pdb = nullptr;
	/* Open database file to READ, CREATE if not exists */
	int rc = sqlite3_open_v2(db_file, &pdb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
	check_sqlite_rc(rc, "Openning Database", pdb);

	/* Database otimizations */
	/* Turn on WAL mode (allows concurrent reading/writing) */
	rc = sqlite3_exec(pdb, "PRAGMA journal_mode = WAL;", NULL, NULL, NULL);
	check_sqlite_rc(rc, "Setting WAL mode", pdb);
	/* Loosen disk sync constraints (massive speedup, minimal crash risk) */
	rc = sqlite3_exec(pdb, "PRAGMA synchronous = NORMAL;", NULL, NULL, NULL);
	check_sqlite_rc(rc, "Setting Synchronous", pdb);
	/* Tell threads to wait up to 2000ms if the database is busy/locked */
	sqlite3_busy_timeout(pdb, SQLITE_TIMEOUT_BUSY);

	/* Create table for data log */
	const char *create_table_sql = "CREATE TABLE IF NOT EXISTS climate_logs ( \
		id INTEGER PRIMARY KEY AUTOINCREMENT, \
		device_id TEXT NOT NULL, \
		temperature REAL NOT NULL, \
		humidity REAL NOT NULL, \
		timestamp TEXT NOT NULL, \
		sensor_status INTEGER NOT NULL \
		);";
	rc = sqlite3_exec(pdb, create_table_sql, NULL, NULL, NULL);
	check_sqlite_rc(rc, "Creating Table", pdb);

	/* Optimize search by "timestamp" */
	const char *create_index_sql = "CREATE INDEX IF NOT EXISTS idx_climate_logs_device_time \
		ON climate_logs(device_id, timestamp DESC);";
	rc = sqlite3_exec(pdb, create_index_sql, NULL, NULL, NULL);
	check_sqlite_rc(rc, "Creating Index", pdb);

	return pdb;
}

struct sqlite3_stmt *
prepare_sqlite_insert_stmt(struct sqlite3 *pdb) {
	const char *insert_query = "INSERT INTO climate_logs (device_id, temperature, humidity, timestamp, sensor_status) VALUES (?, ?, ?, ?, ?);";
	struct sqlite3_stmt *stmt = nullptr;
	int rc = sqlite3_prepare_v3(pdb, insert_query, -1, SQLITE_PREPARE_PERSISTENT, &stmt, nullptr);
	check_sqlite_rc(rc, "Preparing Insert Statement", pdb);
	return stmt;
}

void
bind_climate_log_batch(struct sqlite3 *pdb, struct sqlite3_stmt *stmt, const struct climate_log *log) {
	/* Bind log values to sqlite insert statement */
	sqlite3_bind_text(stmt, 1, log->device_id, -1, SQLITE_STATIC);
	sqlite3_bind_double(stmt, 2, log->temp);
	sqlite3_bind_double(stmt, 3, log->humid);
	sqlite3_bind_text(stmt, 4, log->timestamp, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 5, log->sensor_status);

	/* Evaluate the SQL prepare statement */
	int rc = sqlite3_step(stmt);
	check_sqlite_rc(rc, "Preparing Batch", pdb);

	/* Reset SQL prepare statement (bindings) */
	sqlite3_reset(stmt);
}

int
get_db_batch_limit() {
	return SQLITE_BATCH_LIMIT;
}

void
close_sqlite3_db(struct sqlite3 *pdb) {
	int rc = sqlite3_close(pdb);
	check_sqlite_rc(rc, "Closing database", pdb);
}
