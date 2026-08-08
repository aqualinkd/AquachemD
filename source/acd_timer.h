
#ifndef ACD_TIMER_H_
#define ACD_TIMER_H_

#include "aquachemd.h"

void start_timer(struct aquachemdata *acddata, acd_key_t *key, int duration_min, uint32_t duration_sec);
void start_delay(struct aquachemdata *acddata, acd_key_t *key, int duration_min, uint32_t duration_sec);
//void start_dose_timer_catchup(struct aquachemdata *acddata, acd_key_t *key, int duration_min, uint32_t duration_sec);

int get_timer_left(acd_key_t *key);
uint32_t get_timer_left_sec(acd_key_t *key);
void clear_timer(struct aquachemdata *acddata, acd_key_t *key);
time_t get_timer_started_at(acd_key_t *key);
void stop_all_timers();

#endif // ACD_TIMER_H_