#ifndef SENSOR_H_INCLUDED
#define SENSOR_H_INCLUDED
#include "config.h"

struct climate_log {
	double temp;
	double humid;
	char timestamp[TIMESTAMP_BUF_SIZE];
	const char *device_id;
	bool sensor_status;
};

void climate_log_rand(struct climate_log *log);

void climate_log_show(const struct climate_log *log);

#endif /* SENSOR_H_INCLUDED */
