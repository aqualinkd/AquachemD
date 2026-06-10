
/*
{
  "sensors": [
    {
      "id": "cpu_temp",
      "path": "/sys/class/thermal/thermal_zone0/temp",
      "parser": "raw",      // Read full file as float
      "factor": 0.001       // Divide by 1000 (milli-celsius to celsius)
    },
    {
      "id": "cpu_load_1m",
      "path": "/proc/loadavg",
      "parser": "regex",
      "regex": "^([0-9.]+)", // Capture the first group
      "factor": 0.25        // Divide by 4 CPUs
    }
  ]
}
*/
/*
typedef struct {
    char path[SYSFS_DEVICE_PATH];   // full path to sensor sysfs directory
    enum { PARSER_RAW, PARSER_REGEX } parser_type;
    char *regex_pattern; // Compiled once at startup
    float multiplier;
    float offset;
} sysfs_cfg_t;

typedef struct {
  float value;     // scaled + offset value in engineering units
  long  raw;       // raw integer direct from kernel
  int   status;    // W1_SUCCESS or error code
} sysfs_reading_t;

*/

/*
typedef struct {
    float (*operation)(float current_val, float factor);
} math_ops_t;

// Example transformation
float apply_math(float val, float factor) {
    return val * factor; // If you want division, store 1/4 as 0.25
}
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#include "sysfs.h"
#include "utils.h"

// Static holder for regex compilation
static regex_t compiled_regex;
static bool regex_initialized = false;

bool sysfs_init_sensor(sysfs_sensor_t *cfg) {
    if (cfg->parser_type == PARSER_REGEX && cfg->regex_pattern) {
        if (regcomp(&compiled_regex, cfg->regex_pattern, REG_EXTENDED) != 0) {
            return false;
        }
        regex_initialized = true;
    }
    return true;
}

sysfs_reading_t sysfs_read_sensor(sysfs_sensor_t *cfg) {
    // Initialize status to error until we succeed
    sysfs_reading_t result = {0.0f, 0.0f, SYSFS_ERROR};

    FILE *fp = fopen(cfg->path, "r");
    if (!fp) { 
        result.status = SYSFS_NOT_FOUND; 
        return result;
    }

    char buffer[128];
    if (fgets(buffer, sizeof(buffer), fp) == NULL) {
        fclose(fp);
        result.status = SYSFS_FILE_ACCESS_ERR; 
        return result;
    }
    fclose(fp);

    // Parsing Logic
    if (cfg->parser_type == PARSER_RAW) {
        // Simple numeric conversion using strtof to handle decimals
        char *endptr;
        result.raw = strtof(buffer, &endptr);
        if (endptr == buffer) { 
            result.status = SYSFS_ERROR; 
            return result;
        }
    } 
    else if (cfg->parser_type == PARSER_REGEX) {
        if (!regex_initialized) { 
            result.status = SYSFS_REGEXP_ERROR; 
            return result;
        }

        regmatch_t matches[2]; // Index 0 is whole match, 1 is capture group
        if (regexec(&compiled_regex, buffer, 2, matches, 0) == 0) {
            char val_str[32];
            int len = matches[1].rm_eo - matches[1].rm_so;
            
            if (len >= (int)sizeof(val_str)) { 
                result.status = SYSFS_REGEXP_ERROR; 
                return result;
            }
            
            strncpy(val_str, buffer + matches[1].rm_so, len);
            val_str[len] = '\0';
            
            // FIX 1: Use strtof() instead of strtol() so "0.11" doesn't become 0
            result.raw = strtof(val_str, NULL);
        } else {
            result.status = SYSFS_REGEXP_ERROR;
            return result;
        }
    }

    // FIX 2: Apply Math BEFORE logging it
    // Formula: (raw * multiplier) + offset
    result.value = (result.raw * cfg->multiplier) + cfg->offset;
    result.status = SYSFS_SUCCESS;

    // FIX 3: Update format token from %ld to %f for result.raw
    LOG(LOG_DEBUG, "Sysfs read raw value %f from %s, converted to %f\n", 
        result.raw, cfg->path, result.value);

    return result;
}