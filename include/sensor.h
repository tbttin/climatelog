#ifndef SENSOR_H_INCLUDED
#define SENSOR_H_INCLUDED

#include <time.h>
#include <stdbool.h>

struct climate_log {
	double temp;
	double humid;
	time_t timestamp;
	int device_id;
	bool sensor_failed;
};

void gen_climate_log(struct climate_log *log);

#endif