#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sqlite3.h>
#include "sensor.h"
#include "db.h"

/* Main process flag */
static volatile sig_atomic_t keep_running = 1;

static void
on_shutdown(int signum) {
	(void) signum;
	keep_running = 0; /* Break the loop */
}

/* Set the handler */
static int
init_signal_handlers() {
	struct sigaction sa;

	sa.sa_handler = on_shutdown;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	if (sigaction(SIGINT, &sa, nullptr) < 0) {
		return -1;
	}
	if (sigaction(SIGTERM, &sa, nullptr) < 0) {
		return -1;
	}

	return 0;
}

int
main() {
	/* This program will exit cleanly after this */
	if (init_signal_handlers()) {
		fprintf(stderr, "Fatal error: Failed to configure OS signal traps.\n");
		return EXIT_FAILURE;
	}

	srand(time(nullptr));
	struct climate_log log;
	struct sqlite3 *pdb = init_sqlite_db();
	struct sqlite3_stmt *stmt_insert = prepare_sqlite_insert_stmt(pdb);
	/* Accumalate commands, waits for the signal ("COMMIT") and execute all commands at once
	 * sqlite ACID
	 * */
	sqlite3_exec(pdb, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
	int batch_counter = 0;
	while (keep_running) {
		climate_log_rand(&log);
		climate_log_show(&log);
		bind_climate_log_batch(pdb, stmt_insert, &log);
		batch_counter++;
		if (batch_counter >= get_db_batch_limit()) {
			/* Do the insert statement of this batch */
			sqlite3_exec(pdb, "COMMIT", nullptr, nullptr, nullptr);
			/* Start new transaction */
			sqlite3_exec(pdb, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
			batch_counter = 0;
		}
		sleep(1);
	}

	/* TODO: Handle SQLITE_BUSY */

	/* Clean up after recieved system signal */
	sqlite3_exec(pdb, "COMMIT", nullptr, nullptr, nullptr);
	sqlite3_finalize(stmt_insert);
	close_sqlite3_db(pdb);
	return EXIT_SUCCESS;
}
