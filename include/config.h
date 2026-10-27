#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

/* climate_log configs */
#define TEMP_MIN 15
#define TEMP_MAX 35
#define HUMID_MIN 40
#define HUMID_MAX 90
#define TIMESTAMP_BUF_SIZE 32
#define TIMESTAMP_FORMAT "%Y-%m-%d %H:%M:%S"

/* Database configs */
#define SQLITE_DB_FILE "/usr/lib/climatelog/climatelog.db"
#define SQLITE_TIMEOUT_BUSY 2000
/* Number of row for each time insert */
#define SQLITE_BATCH_LIMIT 10

#endif /* CONFIG_H_INCLUDED */

