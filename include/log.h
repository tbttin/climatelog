#ifndef LOG_H_INCLUDED
#define LOG_H_INCLUDED

#include <stdio.h>
#include <stdbool.h>

extern bool verbose;

#define LOG_ERR(fmt, ...)  fprintf(stderr, "<3>" fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) fprintf(stderr, "<4>" fmt "\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) fprintf(stdout, "<6>" fmt "\n", ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) do \
	if (verbose) { fprintf(stdout, "<7>" fmt "\n", ##__VA_ARGS__); } \
while (0)

#endif /* LOG_H_INCLUDED */

