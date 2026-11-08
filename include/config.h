#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

/* Climate simulation limits */
#define TEMP_MIN 15
#define TEMP_MAX 35
#define HUMID_MIN 40
#define HUMID_MAX 90
#define RAND_MAX_DEVICE 55

/* Database settings */
#define SQLITE_TIMEOUT_BUSY 2000
#define CLIMATELOG_DB_FILE "/var/lib/climatelog/climatelog.db"

#define DB_DEVICE_MAX_INIT 20
#define DB_DEVICE_MAX_CEILING 50
#define DB_EXIT_ON_DB_INVALID_INDEX 0
#define DB_RETENTION_PERIOD "-30 seconds"
#define DB_BATCH_LIMIT 20

#define LOCATION_NAME_BUF_MAX 32

/* TODO: Move some setting to actual config file */

#endif
