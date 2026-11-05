#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "sensor.h"
#include "db.h"

static volatile sig_atomic_t keep_running = 1;

static void handle_shutdown(int sig) { (void)sig; keep_running = 0; }

static void
ui_show_log(struct db_context *db, const struct climate_log *log) {
	char buf[128];
	climate_log_serialize(db, log, buf, 128);
	printf("%s\n", buf);
}

int
main(void) {
	struct sigaction sa = { .sa_handler = handle_shutdown };
	if (sigaction(SIGINT, &sa, NULL) < 0 || sigaction(SIGTERM, &sa, NULL) < 0) return EXIT_FAILURE;

	srand(time(NULL));
	struct db_context *db = db_init();
	if (!db) {
		fprintf(stderr, "Fatal error: Failed to init database.\n");
		return EXIT_FAILURE;
	}

	int batch_count = 0;
	db_begin_transaction(db);

	while (keep_running) {
		struct climate_log log;
		gen_climate_log(&log);

		db_register_device_if_new(db, log.device_id);
		db_write_log_batch(db, &log);
		ui_show_log(db, &log);

		if (++batch_count >= db_get_batch_limit()) {
			printf("End transaction, executing a %d-row flush to disk...\n", batch_count);
			db_end_transaction(db);

			db_purge_old_logs(db);
			db_run_maintenance(db);

			db_begin_transaction(db);
			batch_count = 0;
		}
		sleep(1);
	}

	if (batch_count > 0) db_end_transaction(db);

	db_close(db);
	printf("Clean exit complete.\n");
	return EXIT_SUCCESS;
}
