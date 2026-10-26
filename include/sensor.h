#ifndef SENSOR_H_INCLUDED
#define SENSOR_H_INCLUDED
#include "config.h"

struct climate_log {
	const char *device_id;
	double temp;
	double humid;
	const char timestamp[TIMESTAMP_BUF_SIZE];
	bool sensor_status;
};

void set_climate_log(struct climate_log *log);

void climate_log_show(const struct climate_log *log);

#endif /* SENSOR_H_INCLUDED */
