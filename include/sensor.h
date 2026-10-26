#ifndef _SENSOR_H_INCLUDED_
#define _SENSOR_H_INCLUDED_
#include "config.h"

struct climate_log {
	char *device_id;
	double temp;
	double humid;
	char timestamp[TIMESTAMP_BUF_SIZE];
	bool sensor_status;
};

void set_climate_log(struct climate_log *log);

void climate_log_show(const struct climate_log *log);

#endif /* _SENSOR_H_INCLUDED_ */

