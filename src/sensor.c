#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include "config.h"
#include "sensor.h"

static const char *device_ids[] = {
	"ZONE_A_DTH22",
	"ZONE_B_SHT31",
	"ZONE_C_DTH22"
};

static const int ACTIVE_DEVICE_COUNT = sizeof(device_ids)/sizeof(device_ids[0]);

const char *
gen_device_id(long rand_val) {
	int i = rand_val % ACTIVE_DEVICE_COUNT; /* From 0 to ACTIVE_DEVICE_COUNT - 1 inclusive */
	return device_ids[i];
}

double
gen_temp(struct tm *now) {
	long secs_of_day = now->tm_sec + (60 * now->tm_min) + (3600 * now->tm_hour);
	/* Use -cosine() to simulate real life temperature throughout the day
	 * cos(0) = 1 */
	double mid_point = (TEMP_MAX + TEMP_MIN) / 2.0;
	double swing_amplitude = (TEMP_MAX - TEMP_MIN) / 2.0;
	double swing = swing_amplitude * cos(secs_of_day * (2.0 * M_PI / 86400));
	return mid_point - swing;
}

double
gen_humid(double temp) {
	/* Calculate humid base on temp. Humid is the opposite of temp */
	double tmp_percent = (temp - TEMP_MIN) / (TEMP_MAX - TEMP_MIN); /* (TEMP_MAX - TEMP_MIN) is the temp span */
	return HUMID_MIN + (1 - tmp_percent) * (HUMID_MAX - HUMID_MIN);
}

bool
gen_sensor_status(long rand_val) { 
	double x = (double) rand_val / RAND_MAX;
	return (x <= 0.01); /* 1% the sensor will fail */
}
 
void
set_climate_log(struct climate_log *log) {
	long time_t_now = time(nullptr);
	struct tm *tm_now = localtime(&time_t_now);
	long rand_val = rand();
	log->device_id = gen_device_id(rand_val);
	log->temp = gen_temp(tm_now);
	log->humid = gen_humid(log->temp);
	strftime(log->timestamp, TIMESTAMP_BUF_SIZE, "%Y-%m-%d %H:%M:%S", tm_now);
	log->sensor_status = gen_sensor_status(rand_val);
	if (log->sensor_status) {
		log->temp += 60;
		log->humid += 90;
	}
}

void
climate_log_show(const struct climate_log *log) {
	printf("===========================================================\n");
	printf("device_id: %s\n", log->device_id);
	printf("temperature: %lf\n", log->temp);
	printf("humidity: %lf\n", log->humid);
	printf("timestamp: %s\n", log->timestamp);
	printf("sensor_status: %d\n", log->sensor_status);
}
