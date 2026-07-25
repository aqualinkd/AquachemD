
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <math.h>


#include "sensor_stats.h"
#include "acd_types.h"
#include "aquachemd.h"
#include "utils.h"


void _update_sensor_average(sensor_stats_t *stats, float new_value/*, float tau_seconds*/);

void update_sensor_average(acd_key_t *key) {
  if (isMASKSET(key->flags, CALC_AVERAGE)) {
    _update_sensor_average(&key->stats, key->value);
  }
  /*
  if (isMASKSET(key->flags, AVG_DAILY)) {
    _update_sensor_average(&key->stats, key->value, FILTER_TAU_DAILY);
  } else if (isMASKSET(key->flags, AVG_DAILY)) {
    _update_sensor_average(&key->stats, key->value, FILTER_TAU_WEEKLY);
  }
    */
}

/**
 * A single, unified function to update any time-based rolling average.
 *
 * @param stats       Pointer to the struct tracking this specific average
 * @param new_value   The raw sensor reading (pH, ORP, etc.)
 */

 /*
void _update_sensor_average(sensor_stats_t *stats, float new_value) {
    time_t now = time(NULL);

    // Lock ONLY this specific sensor's tracking box
    pthread_mutex_lock(&stats->lock);
    
    if (stats->last_sample_time == 0) {
        stats->average = new_value;
        stats->last_sample_time = now;
        stats->max = new_value;
        stats->min = new_value;
        pthread_mutex_unlock(&stats->lock);
        return;
    }

    double delta_t = difftime(now, stats->last_sample_time);
    stats->last_sample_time = now;

    if (delta_t <= 0) {
        pthread_mutex_unlock(&stats->lock);
        return;
    }

    float alpha = 1.0f - expf(-((float)delta_t / stats->tau_seconds));
    stats->average = (alpha * new_value) + ((1.0f - alpha) * stats->average);

    if (new_value > stats->max) { stats->max = new_value; }
    else if (new_value < stats->min || stats->min <= 0.0) { stats->min = new_value; }
    

    pthread_mutex_unlock(&stats->lock);
}


void _reset_sensor_average(sensor_stats_t *stats) {
    pthread_mutex_lock(&stats->lock);
    stats->average = 0.0f;
    stats->last_sample_time = 0;
    stats->max = 0.0f;
    stats->min = 0.0f;
    pthread_mutex_unlock(&stats->lock);
}
*/

void _update_sensor_average(sensor_stats_t *stats, float new_value)
{
  pthread_mutex_lock(&stats->lock);

  stats->sample_count++;

  if (stats->sample_count == 1) {
    stats->average = new_value;
    stats->max     = new_value;
    stats->min     = new_value;
  } else {
    stats->average += (new_value - stats->average) / (float)stats->sample_count;
    if (new_value > stats->max) stats->max = new_value;
    if (new_value < stats->min) stats->min = new_value;
  }

  stats->last_sample_time = time(NULL);   // kept for display/"last updated" only, not used in the calc

  pthread_mutex_unlock(&stats->lock);
}

void _reset_sensor_average(sensor_stats_t *stats)
{
  pthread_mutex_lock(&stats->lock);
  stats->average       = 0.0f;
  stats->sample_count  = 0;
  stats->last_sample_time = 0;
  stats->max = 0.0f;
  stats->min = 0.0f;
  pthread_mutex_unlock(&stats->lock);
}

bool reset_sensor_average(acd_key_t *key) {
  if (isMASKSET(key->flags, CALC_AVERAGE)) {
    _reset_sensor_average(&key->stats);
    return true;
  }
  return false; 
}

bool reset_sensors_average_by_hours(struct aquachemdata *acdata, float hours)
{
  bool found = false;
  float tau_seconds = hours * 3600.0f;

  for (acd_key_t *key = acdata->keys->next; key != NULL; key = key->next) {
    if (isMASKSET(key->flags, CALC_AVERAGE) &&
         key->stats.tau_seconds == tau_seconds) {
      _reset_sensor_average(&key->stats);
      found = true;
      LOG(LOG_NOTICE, "Reset %s stats", key->label);
    }
  }

  if (!found) {
    LOG(LOG_ERR, "Did not find any sensor stats set to '%.0f hours' to reset", hours);
    return false; 
  }

  return true;
}

bool reset_sensors_average_by_duration(struct aquachemdata *acdata, const char *duration)
{
  LOG(LOG_DEBUG, "Attempting to reset sensor averages for duration '%s'", duration);
  bool found = false;
  float tau_seconds = parse_duration_to_seconds(duration);

  if (tau_seconds <= 0) {
    return false;
  }
  for (acd_key_t *key = acdata->keys->next; key != NULL; key = key->next) {
    if (isMASKSET(key->flags, CALC_AVERAGE) &&
         key->stats.tau_seconds == tau_seconds) {
      _reset_sensor_average(&key->stats);
      found = true;
    }
  }

  if (!found) {
    LOG(LOG_ERR, "Did not find any sensor stats set to '%s' to reset",duration);
    return false; 
  }

  return true;
}

/*
  period = AVG_DAILY or AVG_WEEKLY
*/
/*
void reset_metrics(struct aquachemdata *acdata, uint8_t period)
{
  for (acd_key_t *key = acdata->keys->next; key != NULL; key = key->next) {
    if (isMASKSET(key->flags, period)) {
      _reset_sensor_average(&key->stats);
    }
  }
}
*/

/**
 * Converts a string like "1d", "24h", "1w" into seconds as a float.
 * Returns 0.0f if the format is invalid.
 */
float parse_duration_to_seconds(const char *str) {
    if (str == NULL) return 0.0f;

    char *endptr;
    float value = strtof(str, &endptr);

    // If no number was found, return 0
    if (endptr == str) return 0.0f;

    // Get the character suffix and make it lowercase
    char unit = tolower(*endptr);

    switch (unit) {
        case 'w': // Week: 7 days
            return value * 604800.0f; 
        case 'd': // Day: 24 hours
            return value * 86400.0f;
        case 'h': // Hour: 60 minutes
            return value * 3600.0f;
        default:
            // Log an error if the suffix is unknown
            //fprintf(stderr, "Invalid duration unit: %c\n", unit);
            return 0.0f;
    }
}


/**
 * Converts a float of seconds into a human-readable duration string ("1w", "24h", "1.5d", etc.).
 * Returns true on success, false if the buffer is too small or invalid.
 */
bool duration_seconds_to_string(float seconds, char *dest, size_t dest_len) {
    if (dest == NULL || dest_len == 0) return false;

    // Handle 0 or negative values gracefully
    if (seconds <= 0.0f) {
        snprintf(dest, dest_len, "none");
        return true;
    }

    float value = 0.0f;
    char unit = '\0';

    // 1. Determine the best unit based on what it divides cleanly into
    if (fmodf(seconds, 604800.0f) == 0.0f) { // Clean weeks
        value = seconds / 604800.0f;
        unit = 'w';
    } else if (fmodf(seconds, 86400.0f) == 0.0f) { // Clean days
        value = seconds / 86400.0f;
        unit = 'd';
    } else if (seconds >= 3600.0f) { // Default fallback to hours if it's large enough
        value = seconds / 3600.0f;
        unit = 'h';
    } else {
        // Optional fallback: If it's less than an hour, you can output in hours as a decimal 
        // or add minutes ('m') / seconds ('s') here if needed.
        value = seconds / 3600.0f;
        unit = 'h';
    }

    // 2. Format the string cleanly. 
    // If it's a whole number, don't print trailing decimals (e.g., "1d" instead of "1.00d")
    int printed;
    if (fmodf(value, 1.0f) == 0.0f) {
        printed = snprintf(dest, dest_len, "%.0f%c", value, unit);
    } else {
        printed = snprintf(dest, dest_len, "%.1f%c", value, unit); // Use 1 decimal point for fractions like 1.5d
    }

    // Check if the output was truncated due to a tiny buffer
    if (printed < 0 || (size_t)printed >= dest_len) {
        return false;
    }

    return true;
}
