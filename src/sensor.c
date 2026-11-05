#include <stdlib.h>
#include <math.h>
#include "sensor.h"
#include "config.h"

void
gen_climate_log(struct climate_log *log) {
	time_t now_t = time(NULL);
	struct tm *now = localtime(&now_t);
	long rand_val = rand();

	log->device_id = rand_val % RAND_MAX_DEVICE;
	log->timestamp = now_t;
	log->sensor_failed = (((double)rand_val / RAND_MAX) <= 0.01);

	// Simulate daily temperature swing via cosine curve
	long secs_of_day = now->tm_sec + (60 * now->tm_min) + (3600 * now->tm_hour);
	double mid_point = (TEMP_MAX + TEMP_MIN) / 2.0;
	double swing = ((TEMP_MAX - TEMP_MIN) / 2.0) * cos(secs_of_day * (2.0 * M_PI / 86400));
	log->temp = mid_point - swing;

	// Humidity inversely proportional to temperature
	double temp_span = (log->temp - TEMP_MIN) / (TEMP_MAX - TEMP_MIN);
	log->humid = HUMID_MIN + ((1.0 - temp_span) * (HUMID_MAX - HUMID_MIN));

	if (log->sensor_failed) {
		log->temp += 60.0;
		log->humid += 90.0;
	}
}