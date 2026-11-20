#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include "log.h"
#include "db.h"
#include "cli.h"

typedef db_status_t (*cli_fn)(struct db_context *, const struct cli_args *);

struct cli_job {
	cli_fn handler;
	struct cli_args args;
};

static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int sig) { (void)sig; keep_running = 0; }
static void setup_signals();
static int parse_positive_int(const char *str, int *num);
static int handle_cli(int argc, char *argv[]);
static int handle_daemon();

void setup_signals() {
	struct sigaction sa = { .sa_handler = handle_signal }; /* It's your last moment, just break the infinite loop and clean up */
	if (sigaction(SIGINT, &sa, NULL) < 0 || sigaction(SIGTERM, &sa, NULL) < 0) {
		LOG_ERR("Can NOT register system signal handler");
		exit(EXIT_FAILURE);
	}
}

int parse_positive_int(const char *str, int *num) {
	long l_num;
	char *endptr = NULL;
	errno = 0;

	l_num = strtol(str, &endptr, 10);
	if (*endptr != '\0' || endptr == str || l_num <= 0 || l_num < INT_MIN || l_num > INT_MAX || errno) {
		return DB_ERR;
	}

	*num = (int)l_num;
	return EXIT_SUCCESS;
}

int handle_cli(int argc, char *argv[]) {
	struct cli_job cli_jobs[argc];
	int job_count = 0;

	int opt;
	while ((opt = getopt(argc, argv, "st:")) != -1) {
		switch (opt) {
			case 's':
				cli_jobs[job_count++] = (struct cli_job){ .handler = db_cli_print_status };
				break;
			case 't':
				int num;

				if (parse_positive_int(optarg, &num)) {
					LOG_ERR("Option -t have an invalid argument \"%s\". It must be a positive number", optarg);
					return DB_ERR;
				}

				cli_jobs[job_count++] = (struct cli_job){ .handler = db_cli_tail, .args = { num } };
				break;
			default:
				LOG_ERR("Usage: %s [-s] [-t <num>]\n"
					"-s	Print short status.\n"
					"-t	Print last <num> recent records."
					, argv[0]);
					return DB_ERR;
		}
	}

	if (optind < argc) {
		LOG_ERR("Unexpected argument: \"%s\"", argv[optind]);
		return DB_ERR;
	}

	struct db_context *pdb = NULL;
	if (job_count) {
		db_status_t status;
		if ((status = db_init(&pdb, DB_MODE_CLI)) != DB_SUCCESS) {
			LOG_ERR("Error: %s. Failed to init database mode %d", db_strstt(status), DB_MODE_CLI);
			db_close(pdb);
			return status;
		}

		for (int i = 0; i < job_count; i++) {
			if ((status = cli_jobs[i].handler(pdb, &cli_jobs[i].args)) != DB_SUCCESS) {
				db_close(pdb);
				return status;
			}
		}

		db_close(pdb);
	}

	return DB_SUCCESS;
}

int handle_daemon() {
	setvbuf(stdout, NULL, _IOLBF, 0); /* Use linebufered instead of systemd blockbuferd (with journald background pipe) */
	srand(time(NULL));

	struct db_context *pdb = NULL;
	db_status_t status;
	if ((status = db_init(&pdb, DB_MODE_DAEMON)) != DB_SUCCESS) {
		LOG_ERR("Error: %s. Failed to init database mode %d", db_strstt(status), DB_MODE_DAEMON);
		db_close(pdb);
		return status;
	}

	while (keep_running) {
		struct climate_log log;
		gen_climate_log(&log);

		if ((status = db_ingest_log(pdb, &log)) != DB_SUCCESS) { return status; }

		struct timespec ts = { .tv_sec = 1, .tv_nsec = 0 };
		nanosleep(&ts, NULL);
	}

	db_close(pdb);
	LOG_INFO("Clean exit completed");

	return DB_SUCCESS;
}

int main(int argc, char *argv[]) {
	setup_signals();

	db_status_t status = (argc > 1) ? handle_cli(argc, argv) : handle_daemon();

	return (status == DB_SUCCESS) ? EXIT_SUCCESS : EXIT_FAILURE;
}

