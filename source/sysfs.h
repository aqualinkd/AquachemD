#ifndef SYSFS_H
#define SYSFS_H

#include <stdbool.h>





#define SYSFS_DEVICE_PATH 256

// Status / Error codes
#define SYSFS_SUCCESS          0
#define SYSFS_ERROR           -1
#define SYSFS_REGEXP_ERROR    -2
#define SYSFS_NOT_FOUND       -3
#define SYSFS_FILE_ACCESS_ERR -4

typedef enum {
    PARSER_RAW,
    PARSER_REGEX
} parser_type_t;

typedef struct {
    char path[SYSFS_DEVICE_PATH];
    parser_type_t parser_type;
    char *regex_pattern;
    float multiplier;
    float offset;
} sysfs_sensor_t;

typedef struct {
    float value;
    float raw;
    int   status;
} sysfs_reading_t;

// API
bool sysfs_init_sensor(sysfs_sensor_t *cfg);
sysfs_reading_t sysfs_read_sensor(sysfs_sensor_t *cfg);

#endif // SYSFS_H