#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

/* Climate simulation limits */
#define TEMP_MIN 15
#define TEMP_MAX 35
#define HUMID_MIN 40
#define HUMID_MAX 90
#define RAND_MAX_DEVICE 25

/* Database settings */
#define CLIMATELOG_DB_FILE "/var/lib/climatelog/climatelog.db"
#define SQLITE_TIMEOUT_BUSY 2000
#define DEVICE_MAX_INIT 5
#define DEVICE_MAX_CEILING 500
#define DB_BATCH_LIMIT 20
#define RETENTION_PERIOD "-30 seconds"

#define LOCATION_NAME_BUF_MAX 32

#endif
