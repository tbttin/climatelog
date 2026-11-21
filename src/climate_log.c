#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include "log.h"
#include "db.h"
#include "cli.h"

#define MAX_CLI_JOBS 2

bool verbose = false;
static volatile sig_atomic_t keep_running = 1;

struct cli_job {
	db_status_t (*handler)(struct db_context *, const struct cli_args *);
	struct cli_args args;
};

static void handle_signal(int sig) { (void)sig; keep_running = 0; }
static void setup_signals();
static db_status_t parse_positive_int(const char *str, int *num);
static db_status_t parse_cli(int argc, char *argv[], struct cli_job jobs[MAX_CLI_JOBS + 1]);
static db_status_t run_cli(int argc, char *argv[], db_mode_t *mode);
static db_status_t handle_daemon();

void setup_signals() {
	struct sigaction sa = { .sa_handler = handle_signal }; /* It's your last moment, just break the infinite loop and clean up */
	if (sigaction(SIGINT, &sa, NULL) < 0 || sigaction(SIGTERM, &sa, NULL) < 0) {
		LOG_ERR("Can NOT register system signal handler");
		exit(EXIT_FAILURE);
	}
}

db_status_t parse_positive_int(const char *str, int *num) {
	long l_num;
	char *endptr = NULL;
	errno = 0;

	l_num = strtol(str, &endptr, 10);
	if (*endptr != '\0' || endptr == str || l_num < 1 || l_num > INT_MAX || errno) {
		return DB_ERR;
	}

	*num = (int)l_num;
	return DB_SUCCESS;
}

db_status_t parse_cli(int argc, char *argv[], struct cli_job jobs[static MAX_CLI_JOBS + 1]) {
	int job_count = 0;
	int opt;
	uint32_t seen_opts_m = 0;
	while ((opt = getopt(argc, argv, "vst:h")) != -1) {
		if (opt >= 'a' && opt <= 'z') {
			uint32_t opt_bit = 1U << (opt - 'a');
			if (seen_opts_m & opt_bit) {
				LOG_ERR("Duplicate option: \"-%c\"", opt);
				return DB_ERR;
			}
			seen_opts_m |= opt_bit;
		}

		switch (opt) {
			case 's':
				jobs[job_count++].handler = db_cli_print_status;
				break;
			case 't': {
				int num;
				db_status_t status;

				if ((status = parse_positive_int(optarg, &num)) != DB_SUCCESS) {
					LOG_ERR("Option -t have an invalid argument \"%s\". It must be a positive number", optarg);
					return status;
				}

				jobs[job_count++] = (struct cli_job){ .handler = db_cli_tail, .args = { num } };
				break;
			}
			case 'v':
				verbose = true;
				break;
			case 'h':
			default:
				LOG_ERR("Usage: %s [-s] [-t <num>] [-v] [-h]\n"
					"-s	Print short status\n"
					"-t	Print last <num> recent records\n"
					"-v	Print more output\n"
					"-h	Print usage"
					, argv[0]);
				return DB_ERR;
		}
	}

	if (optind < argc) {
		LOG_ERR("Unexpected argument: \"%s\"", argv[optind]);
		return DB_ERR;
	}

	return DB_SUCCESS;
}

db_status_t run_cli(int argc, char *argv[], db_mode_t *mode) {
	struct cli_job jobs[MAX_CLI_JOBS + 1] = { 0 };

	db_status_t status = parse_cli(argc, argv, jobs);
	if (status != DB_SUCCESS) { return status; }

	if (jobs[0].handler != NULL) {
		*mode = DB_MODE_CLI;
		struct db_context *pdb = NULL;
		if ((status = db_init(&pdb, *mode)) != DB_SUCCESS) {
			LOG_ERR("Error: %s. Failed to init database mode %d", db_strstt(status), *mode);
			db_close(pdb);
			return status;
		}

		for (int i = 0; jobs[i].handler != NULL; i++) {
			if ((status = jobs[i].handler(pdb, &jobs[i].args)) != DB_SUCCESS) {
				db_close(pdb);
				return status;
			}
		}

		db_close(pdb);
	}

	return DB_SUCCESS;
}

db_status_t handle_daemon() {
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

		if ((status = db_ingest_log(pdb, &log)) != DB_SUCCESS) {
			db_close(pdb);
			return status;
		}

		struct timespec ts = { .tv_sec = 1, .tv_nsec = 0 };
		nanosleep(&ts, NULL);
	}

	db_close(pdb);
	LOG_INFO("Clean exit completed");

	return DB_SUCCESS;
}

int main(int argc, char *argv[]) {
	db_mode_t mode = DB_MODE_DAEMON;

	if (run_cli(argc, argv, &mode) != DB_SUCCESS) { return EXIT_FAILURE; }
	if (mode == DB_MODE_CLI) { return EXIT_SUCCESS; }

	setup_signals();
	if (handle_daemon() != DB_SUCCESS) { return EXIT_FAILURE; }

	return EXIT_SUCCESS;
}

