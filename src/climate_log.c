#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include "sensor.h"
#include "db.h"

static volatile sig_atomic_t keep_running = 1;

static void handle_shutdown(int sig) { (void)sig; keep_running = 0; }

int
main(void) {
	/* setlinebuf(stdout); */

	struct sigaction sa = { .sa_handler = handle_shutdown };
	if (sigaction(SIGINT, &sa, NULL) < 0 || sigaction(SIGTERM, &sa, NULL) < 0) {
		LOG_ERR("Can NOT register system signal handler");
		return EXIT_FAILURE;
	}

	srand(time(NULL));
	struct db_context *db = db_init();
	if (!db) {
		LOG_ERR("Fatal error: Failed to init database");
		return EXIT_FAILURE;
	}

	int batch_count = 0;
	bool in_transaction = false;
	while (keep_running) {
		if (!in_transaction) {
			db_begin_transaction(db);
			in_transaction = true;
		}

		struct climate_log log;
		gen_climate_log(&log);
		const char *dev_loc = NULL;
		db_status_t rs = db_register_device_if_new(db, log.device_id);
		if (rs == DB_ERR_ID_EXCEEDS_CEILING) {
			LOG_WARN("Skipping log processing cycle for problematic device id=%d", log.device_id);
			continue;
		} else if (rs == DB_SUCCESS) {
			db_get_device_loc(db, &dev_loc, log.device_id);
			LOG_INFO("New device regisdered (id=%d dev_loc=%s)", log.device_id, dev_loc);
		}

		if (log.sensor_failed) {
			db_get_device_loc(db, &dev_loc, log.device_id);
			LOG_WARN("Sensor fault: dev=%02d dev_loc=%-6s temp=%4.1fC humid=%4.1f%%", log.device_id, dev_loc, log.temp, log.humid);
		}
		db_write_log_batch(db, &log);

		if (++batch_count >= db_get_batch_limit()) {
			if (in_transaction) {
				LOG_INFO("End transaction, start insertion - executing a %d-row flush to disk...", batch_count);
				db_end_transaction(db);
				in_transaction = false;
			}

			db_purge_old_logs(db);

			batch_count = 0;
		}
		struct timespec ts = { .tv_sec = 1, .tv_nsec = 0 };
		nanosleep(&ts, NULL);
	}

	if (batch_count > 0 && in_transaction) {
		db_end_transaction(db);
		LOG_INFO("Terminated on signal. %d-device pack flushed to the disk", batch_count);
	} else if (in_transaction) {
		db_rollback(db);
	}

	db_close(db);
	LOG_INFO("Clean exit complete");
	return EXIT_SUCCESS;
}
