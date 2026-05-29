
#include <math.h>
#include <time.h>
#include <pthread.h>

#include "sensor_stats.h"
#include "acd_types.h"
#include "aquachemd.h"
#include "utils.h"

pthread_mutex_t metrics_mutex = PTHREAD_MUTEX_INITIALIZER;


void _update_sensor_average(sensor_stats_t *stats, float new_value, float tau_seconds);

void update_sensor_average(acd_key_t *key) {
  if (isMASKSET(key->flags, AVG_DAILY)) {
    _update_sensor_average(&key->stats, key->value, FILTER_TAU_DAILY);
  } else if (isMASKSET(key->flags, AVG_DAILY)) {
    _update_sensor_average(&key->stats, key->value, FILTER_TAU_WEEKLY);
  }
}

/**
 * A single, unified function to update any time-based rolling average.
 *
 * @param stats       Pointer to the struct tracking this specific average
 * @param new_value   The raw sensor reading (pH, ORP, etc.)
 * @param tau_seconds The time window (e.g., FILTER_TAU_DAILY or FILTER_TAU_WEEKLY)
 */
void _update_sensor_average(sensor_stats_t *stats, float new_value, float tau_seconds) {
    time_t now = time(NULL);

    // Lock ONLY this specific sensor's tracking box
    //pthread_mutex_lock(&stats->lock);
    
    if (stats->last_sample_time == 0) {
        stats->average = new_value;
        stats->last_sample_time = now;
        stats->max = new_value;
        stats->min = new_value;
        //pthread_mutex_unlock(&stats->lock);
        return;
    }

    double delta_t = difftime(now, stats->last_sample_time);
    stats->last_sample_time = now;

    if (delta_t <= 0) {
        //pthread_mutex_unlock(&stats->lock);
        return;
    }

    float alpha = 1.0f - expf(-((float)delta_t / tau_seconds));
    stats->average = (alpha * new_value) + ((1.0f - alpha) * stats->average);

    if (new_value > stats->max) { stats->max = new_value; }
    else if (new_value < stats->min || stats->min <= 0.0) { stats->min = new_value; }
    

    //pthread_mutex_unlock(&stats->lock);
}

void reset_sensor_average(sensor_stats_t *stats) {
    //pthread_mutex_lock(&stats->lock);
    stats->average = 0.0f;
    stats->last_sample_time = 0;
    stats->max = 0.0f;
    stats->min = 0.0f;
    //pthread_mutex_unlock(&stats->lock);
}

/*
  period = AVG_DAILY or AVG_WEEKLY
*/
void reset_metrics(struct aquachemdata *acdata, uint8_t period)
{
  for (acd_key_t *key = acdata->keys->next; key != NULL; key = key->next) {
    if (isMASKSET(key->flags, period)) {
      reset_sensor_average(&key->stats);
    }
  }
}

