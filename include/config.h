#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

/* Climate simulation limits */
#define TEMP_MIN 15
#define TEMP_MAX 35
#define HUMID_MIN 40
#define HUMID_MAX 90
#define RAND_MAX_DEVICE 22

#define MAX_DEVICE 30
#define LOCATION_NAME_BUF_MAX 32

/* Database settings */
#define CLIMATELOG_DB_FILE "/var/lib/climatelog/climatelog.db"
#define SQLITE_TIMEOUT_BUSY 2000
#define DB_BATCH_LIMIT 10
#define RETENTION_PERIOD "-20 seconds"

#endif
