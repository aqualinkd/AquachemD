#ifndef SENSOR_STATS_H_
#define SENSOR_STATS_H_

#include "acd_types.h"
#include "aquachemd.h"

// Time constants defined in seconds
//#define FILTER_TAU_DAILY   86400.0f   // 1 Day (24 hours)
//#define FILTER_TAU_WEEKLY  604800.0f  // 7 Days

void update_sensor_average(acd_key_t *key);
//void reset_sensor_average(sensor_stats_t *stats); 
//void reset_metrics(struct aquachemdata *acddata, uint8_t period);

bool reset_sensors_average_by_duration(struct aquachemdata *acddata, const char *duration);
bool reset_sensors_average_by_hours(struct aquachemdata *acdata, float hours);
bool reset_sensor_average(acd_key_t *key);
float parse_duration_to_seconds(const char *str);
bool duration_seconds_to_string(float seconds, char *dest, size_t dest_len);

#endif