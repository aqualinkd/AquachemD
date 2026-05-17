#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

#include "1wire.h"
#include "utils.h"


// Normalise the stored path to always be the directory (strip filename if given)
static void normalise_dir_path(char *path)
{
  char *p = strstr(path, "/temperature");
  if (p) { *p = '\0'; return; }
  p = strstr(path, "/w1_slave");
  if (p) { *p = '\0'; }
}

void populate_paths(w1_sensor_t *s)
{
  normalise_dir_path(s->path);

  if (s->path[0] == '/') { // Assume full path if starts with '/'
    snprintf(s->temp_path,  sizeof(s->temp_path),  "%s/temperature", s->path);
    snprintf(s->slave_path, sizeof(s->slave_path), "%s/w1_slave",    s->path);
  } else { // Prepend path
    snprintf(s->temp_path,  sizeof(s->temp_path),  "%s/%s/temperature", W1_BASE_PATH, s->path);
    snprintf(s->slave_path, sizeof(s->slave_path), "%s/%s/w1_slave",    W1_BASE_PATH, s->path);
  }
}

// ─── Discovery ────────────────────────────────────────────────────────────────

void w1_detect()
{
  DIR *dir = opendir(W1_BASE_PATH);
  if (!dir)
  {
    fprintf(stderr, "w1_detect: cannot open %s — is 1-wire enabled?\n", W1_BASE_PATH);
    fprintf(stderr, "  Check /boot/firmware/config.txt contains: dtoverlay=w1-gpio\n");
    return;
  }

  printf("\nScanning 1-wire bus at %s...\n\n", W1_BASE_PATH);

  int count = 0;
  struct dirent *entry;

  while ((entry = readdir(dir)) != NULL)
  {
    // Skip . and .. and the bus master entry
    // Also skip family code "00-" — phantom devices reported when bus is
    // enabled but no sensor is physically connected (line floating)
    if (entry->d_name[0] == '.' ||
        strncmp(entry->d_name, "w1_bus", 6) == 0 ||
        strncmp(entry->d_name, "00-", 3) == 0)
      continue;

    count++;

    // Identify the family from the prefix (first two hex chars before the dash)
    const char *family = "unknown";
    if (strncmp(entry->d_name, "28-", 3) == 0)      family = "DS18B20 (temperature)";
    else if (strncmp(entry->d_name, "10-", 3) == 0) family = "DS18S20 (temperature)";
    else if (strncmp(entry->d_name, "22-", 3) == 0) family = "DS1822  (temperature)";
    else if (strncmp(entry->d_name, "3b-", 3) == 0) family = "DS1825  (temperature)";
    else if (strncmp(entry->d_name, "42-", 3) == 0) family = "DS28EA00 (temperature)";

    printf("  %s   [%s]\n", entry->d_name, family);

    // For temperature sensors, try to read and display current value
    if (strncmp(entry->d_name, "28-", 3) == 0 ||
        strncmp(entry->d_name, "10-", 3) == 0)
    {
      w1_sensor_t s;
      char dev_path[W1_DEVICE_PATH];
      snprintf(dev_path, sizeof(dev_path), "%s/%s", W1_BASE_PATH, entry->d_name);
      w1_init_ds18b20(&s, dev_path);

      w1_reading_t r = w1_read(&s);
      if (r.status == W1_SUCCESS)
        printf("    current reading: %.3f °C  (%.3f °F)\n", r.value, temp_c_to_f(r.value));
      else
        printf("    current reading: error (%s)\n", w1_strerror(r.status));
    }
  }

  closedir(dir);

  if (count == 0)
    printf("  No 1-wire devices found.\n");

  printf("\n");
}

int w1_find_ds18b20(w1_sensor_t *sensors, int max)
{
  DIR *dir = opendir(W1_BASE_PATH);
  if (!dir)
  {
    fprintf(stderr, "w1_find_ds18b20: cannot open %s\n", W1_BASE_PATH);
    return W1_ERROR;
  }

  int count = 0;
  struct dirent *entry;

  while ((entry = readdir(dir)) != NULL && count < max)
  {
    if (strncmp(entry->d_name, W1_DS18B20_PREFIX, strlen(W1_DS18B20_PREFIX)) != 0)
      continue;

    char dev_path[W1_DEVICE_PATH];
    snprintf(dev_path, sizeof(dev_path), "%s/%s", W1_BASE_PATH, entry->d_name);
    w1_init_ds18b20(&sensors[count], dev_path);
    count++;
  }

  closedir(dir);
  return count;
}

// ─── Sensor initialisation ────────────────────────────────────────────────────

void w1_init_ds18b20(w1_sensor_t *s, const char *path)
{
    if (path && path != s->path) {   // guard against self-copy
      snprintf(s->path, sizeof(s->path), "%s", path);
    }

    populate_paths(s);

    s->type   = W1_TYPE_DS18B20;
    s->scale  = W1_DS18B20_SCALE;
    s->offset = 0.0f;

}

void w1_init_generic(w1_sensor_t *s, const char *path, float scale, float offset)
{
  if (path && path != s->path) {   // guard against self-copy
    snprintf(s->path, sizeof(s->path), "%s", path);
  }

  populate_paths(s);

  s->type   = W1_TYPE_GENERIC;
  s->scale  = scale;
  s->offset = offset;
}

#ifndef DUMMY_SENSORS

// ─── Reading ─────────────────────────────────────────────────────────────────

// Read using the simple 'temperature' sysfs file (preferred).
// The kernel w1_therm driver writes the value in millidegrees C as a plain integer.
// CRC is checked by the driver internally — if it fails the file returns an error.
w1_reading_t w1_read(const w1_sensor_t *s)
{
  w1_reading_t result = {0.0f, 0, W1_ERROR};

  FILE *fp = fopen(s->temp_path, "r");
  if (!fp)
  {
    LOG(LOG_ERR, "w1_read: cannot open %s: %s\n", s->temp_path, strerror(errno));
    result.status = W1_NOT_FOUND;
    return result;
  }

  long raw = 0;
  if (fscanf(fp, "%ld", &raw) != 1)
  {
    LOG(LOG_ERR, "w1_read: failed to parse value from %s\n", s->temp_path);
    fclose(fp);
    return result;
  }
  fclose(fp);

  result.raw    = raw;
  result.value  = (float)raw * s->scale + s->offset;
  result.status = W1_SUCCESS;
  return result;
}

#endif // ifndef DUMMY_SENSORS
#ifdef DUMMY_SENSORS
w1_reading_t w1_read(const w1_sensor_t *s)
{
  // Return between 28 and 29
  //return (w1_reading_t){ 28.5f + dummy_drift(0.5f), 0, W1_SUCCESS };

  // Return between -5 and 75
  return (w1_reading_t){ 35.0f + dummy_drift(40.0f), 0, W1_SUCCESS };

}
#endif // DUMMY_SENSORS

// Read using 'w1_slave' file with explicit CRC check in userspace.
// w1_slave format:
//   Line 1: <hex bytes> : crc=<xx> YES|NO
//   Line 2: <hex bytes> t=<millidegrees>
w1_reading_t w1_read_with_crc(const w1_sensor_t *s)
{
  w1_reading_t result = {0.0f, 0, W1_ERROR};

  FILE *fp = fopen(s->slave_path, "r");
  if (!fp)
  {
    LOG(LOG_ERR, "w1_read_with_crc: cannot open %s: %s\n", s->slave_path, strerror(errno));
    result.status = W1_NOT_FOUND;
    return result;
  }

  char line1[128] = {0};
  char line2[128] = {0};

  if (!fgets(line1, sizeof(line1), fp) || !fgets(line2, sizeof(line2), fp))
  {
    LOG(LOG_ERR, "w1_read_with_crc: short read from %s\n", s->slave_path);
    fclose(fp);
    return result;
  }
  fclose(fp);

  // Line 1 must end in "YES" to confirm CRC passed
  if (strstr(line1, "YES") == NULL)
  {
    LOG(LOG_ERR, "w1_read_with_crc: CRC check failed for %s\n", s->slave_path);
    result.status = W1_CRC_ERROR;
    return result;
  }

  // Line 2 contains "t=<value>"
  char *t_pos = strstr(line2, "t=");
  if (!t_pos)
  {
    LOG(LOG_ERR, "w1_read_with_crc: no t= field in %s\n", s->slave_path);
    return result;
  }

  long raw = atol(t_pos + 2);
  result.raw    = raw;
  result.value  = (float)raw * s->scale + s->offset;
  result.status = W1_SUCCESS;
  return result;
}

// ─── Utility ─────────────────────────────────────────────────────────────────
/*
float w1_c_to_f(float celsius)
{
  return celsius * 9.0f / 5.0f + 32.0f;
}

float w1_c_to_k(float celsius)
{
  return celsius + 273.15f;
}
*/
const char *w1_strerror(int status)
{
  switch (status)
  {
    case W1_SUCCESS:   return "success";
    case W1_ERROR:     return "read error";
    case W1_CRC_ERROR: return "CRC check failed";
    case W1_NOT_FOUND: return "sensor file not found";
    default:           return "unknown error";
  }
}
