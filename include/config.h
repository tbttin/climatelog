#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

/* Climate simulation limits */
#define TEMP_MIN 15
#define TEMP_MAX 35
#define HUMID_MIN 40
#define HUMID_MAX 90
#define RAND_MAX_DEVICE 264

/* Database settings */
#ifndef CLIMATELOG_DB_FILE
#define CLIMATELOG_DB_FILE "/var/lib/climatelog/climatelog.db"
#endif
#define SQLITE_TIMEOUT_BUSY 2000

#define DB_DEVICE_MAX_CEILING 256
#define DB_EXIT_ON_DB_INVALID_INDEX 0
#define DB_RETENTION_PERIOD "-30 days"
#define DB_BATCH_LIMIT 60

#define LOCATION_NAME_BUF_MAX 32

/* TODO: Move some setting to actual config file */

#endif
