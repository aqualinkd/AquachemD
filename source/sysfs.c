
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
#include <stdbool.h>
#include <string.h>
#include <fnmatch.h>

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


/********************************
 * 
 *   Scanning tools
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <fnmatch.h>
#include <limits.h>

// Helper to read strings safely from sysfs
static int read_sysfs_string(const char *path, char *out_buf, size_t max_len)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    if (fgets(out_buf, max_len, f) != NULL) {
        out_buf[strcspn(out_buf, "\r\n")] = 0;
        fclose(f);
        return 0;
    }
    fclose(f);
    return -1;
}

// Helper to read integer values from sysfs
static int read_sysfs_long(const char *path, long *out_val)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    if (fscanf(f, "%ld", out_val) == 1) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return -1;
}

// 1. Scan HWMON (Temperature & Voltage/ADC like ADS1115)
void scan_sysfs_hwmon(bool usesyslog)
{
    DIR *dir = opendir("/sys/class/hwmon");
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "hwmon", 5) != 0) continue;

        // Base directory buffer sized to 512 bytes
        char hwmon_dir[512];
        snprintf(hwmon_dir, sizeof(hwmon_dir), "/sys/class/hwmon/%s", entry->d_name);

        char chip_name[64] = "unknown";
        char name_path[PATH_MAX];
        snprintf(name_path, sizeof(name_path), "%s/name", hwmon_dir);
        read_sysfs_string(name_path, chip_name, sizeof(chip_name));

        DIR *hdir = opendir(hwmon_dir);
        if (!hdir) continue;

        struct dirent *hentry;
        while ((hentry = readdir(hdir)) != NULL) {
            bool is_temp = (fnmatch("temp*_input", hentry->d_name, 0) == 0);
            bool is_volt = (fnmatch("in*_input", hentry->d_name, 0) == 0);

            if (is_temp || is_volt) {
                char attr_path[PATH_MAX];
                snprintf(attr_path, sizeof(attr_path), "%s/%s", hwmon_dir, hentry->d_name);

                long val = 0;
                if (read_sysfs_long(attr_path, &val) == 0) {
                    if (is_temp) {
                        DIAG_LOG(usesyslog, "  [hwmon] %-15s %-12s : %.2f °C - (%s)\n",
                                 chip_name, hentry->d_name, val / 1000.0f, attr_path);
                    } else { // Voltage / ADC channel
                        DIAG_LOG(usesyslog, "  [hwmon] %-15s %-12s : %ld mV - (%s)\n",
                                 chip_name, hentry->d_name, val, attr_path);
                    }
                }
            }
        }
        closedir(hdir);
    }
    closedir(dir);
}

// 2. Scan Thermal Zones (SoC/CPU internal thermal sensors)
void scan_sysfs_thermal(bool usesyslog)
{
    DIR *dir = opendir("/sys/class/thermal");
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "thermal_zone", 12) != 0) continue;

        char temp_path[PATH_MAX], type_path[PATH_MAX], zone_type[64] = "thermal";
        snprintf(temp_path, sizeof(temp_path), "/sys/class/thermal/%s/temp", entry->d_name);
        snprintf(type_path, sizeof(type_path), "/sys/class/thermal/%s/type", entry->d_name);

        long val_mC = 0;
        if (read_sysfs_long(temp_path, &val_mC) == 0) {
            read_sysfs_string(type_path, zone_type, sizeof(zone_type));
            DIAG_LOG(usesyslog, "  [thermal] %-13s (%-14s) : %.2f °C - (%s)\n",
                     entry->d_name, zone_type, val_mC / 1000.0f, temp_path);
        }
    }
    closedir(dir);
}

// 3. Scan IIO Devices (Industrial I/O ADCs/Sensors)
void scan_sysfs_iio(bool usesyslog)
{
    DIR *dir = opendir("/sys/bus/iio/devices");
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "iio:device", 10) != 0) continue;

        // Base directory buffer sized to 512 bytes
        char device_dir[512];
        snprintf(device_dir, sizeof(device_dir), "/sys/bus/iio/devices/%s", entry->d_name);

        char chip_name[64] = "unknown";
        char name_path[PATH_MAX];
        snprintf(name_path, sizeof(name_path), "%s/name", device_dir);
        read_sysfs_string(name_path, chip_name, sizeof(chip_name));

        DIR *idir = opendir(device_dir);
        if (!idir) continue;

        struct dirent *ientry;
        while ((ientry = readdir(idir)) != NULL) {
            if (fnmatch("in_voltage*_raw", ientry->d_name, 0) == 0 ||
                fnmatch("in_voltage*_input", ientry->d_name, 0) == 0) {
                char attr_path[PATH_MAX];
                snprintf(attr_path, sizeof(attr_path), "%s/%s", device_dir, ientry->d_name);

                long val = 0;
                if (read_sysfs_long(attr_path, &val) == 0) {
                    DIAG_LOG(usesyslog, "  [iio] %-17s %-18s : %ld (raw) - (%s)\n",
                             chip_name, ientry->d_name, val, attr_path);
                }
            }
        }
        closedir(idir);
    }
    closedir(dir);
}

// Top-level sysfs scanner entry point
void sysfs_detect(bool usesyslog)
{
    //DIAG_LOG(usesyslog, "=============================================\n");
    DIAG_LOG(usesyslog, "\nScanning sysfs sensors...\n\n");
    scan_sysfs_hwmon(usesyslog);
    scan_sysfs_thermal(usesyslog);
    scan_sysfs_iio(usesyslog);
    //DIAG_LOG(usesyslog, "---------------------------------------------\n");
}