#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include "sensor.h"

static volatile sig_atomic_t keep_running = 1;

static void
on_shutdown(int signum) {
	(void) signum;
	keep_running = 0;
}

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
	if (init_signal_handlers()) {
		fprintf(stderr, "Fatal error: Failed to configure OS signal traps.\n");
		return EXIT_FAILURE;
	}

	srand(time(nullptr));
	struct climate_log log;
	while (keep_running) {
		set_climate_log(&log);
		climate_log_show(&log);
		sleep(1);
	}

	return EXIT_SUCCESS;
}
