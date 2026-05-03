
#define _POSIX_C_SOURCE 200809L // clock_gettime()

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#include "aquachemd.h"
#include "utils.h"
#include "acd_timer.h"
#include "state_manager.h"

struct timerthread {
  pthread_t thread_id;
  pthread_mutex_t thread_mutex;
  pthread_cond_t thread_cond;
  acd_key_t *key;
  struct aquachemdata *acddata;
  int duration_min;
  uint32_t duration_sec;
  struct timespec timeout;
  time_t started_at;
  struct timerthread *next;
  struct timerthread *prev;
};

// Global lock to protect the linked list
static pthread_mutex_t _ll_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct timerthread *_timerthread_ll = NULL;

void *timer_worker( void *ptr );

// NOTE: Caller MUST hold _ll_mutex before calling this function
struct timerthread *find_timerthread(acd_key_t *key)
{
  struct timerthread *t_ptr;
  for (t_ptr = _timerthread_ll; t_ptr != NULL; t_ptr = t_ptr->next) {
    if (t_ptr->key == key) {
      return t_ptr;
    }
  }
  return NULL;
}

int get_timer_left(acd_key_t *key)
{
  int remaining_mins = 0;
  
  pthread_mutex_lock(&_ll_mutex);
  struct timerthread *t_ptr = find_timerthread(key);

  if (t_ptr != NULL) {
    time_t now = time(NULL);
    long total_duration_sec = (t_ptr->duration_min * 60) + t_ptr->duration_sec;
    long elapsed_sec = (long)difftime(now, t_ptr->started_at);
    
    if (elapsed_sec < total_duration_sec) {
      // Add 0.5 for rounding to nearest minute
      remaining_mins = (int) (((total_duration_sec - elapsed_sec) / 60.0) + 0.5);
    }
  }
  pthread_mutex_unlock(&_ll_mutex);

  return remaining_mins;
}

uint32_t get_timer_left_sec(acd_key_t *key)
{
  uint32_t remaining_sec = 0;

  pthread_mutex_lock(&_ll_mutex);
  struct timerthread *t_ptr = find_timerthread(key);

  if (t_ptr != NULL) {
    time_t now = time(NULL);
    long total_duration_sec = (t_ptr->duration_min * 60) + t_ptr->duration_sec;
    long elapsed_sec = (long)difftime(now, t_ptr->started_at);
    
    if (elapsed_sec < total_duration_sec) {
      remaining_sec = (uint32_t)(total_duration_sec - elapsed_sec);
    }
  }
  pthread_mutex_unlock(&_ll_mutex);

  return remaining_sec;
}

void clear_timer(struct aquachemdata *acddata, acd_key_t *key)
{
  pthread_mutex_lock(&_ll_mutex);
  struct timerthread *t_ptr = find_timerthread(key);

  if (t_ptr != NULL) {
    LOG(LOG_INFO, "Clearing timer for '%s'\n", t_ptr->key->label);
    
    // Lock the thread's specific mutex before changing its variables
    pthread_mutex_lock(&t_ptr->thread_mutex);
    t_ptr->duration_min = 0;
    t_ptr->duration_sec = 0;
    pthread_cond_broadcast(&t_ptr->thread_cond);
    pthread_mutex_unlock(&t_ptr->thread_mutex);
  }
  pthread_mutex_unlock(&_ll_mutex);
}

void start_timer(struct aquachemdata *acddata, acd_key_t *key, int duration_min, uint32_t duration_sec)
{
  pthread_mutex_lock(&_ll_mutex);
  struct timerthread *t_ptr = find_timerthread(key);

  if (t_ptr != NULL) {
    LOG(LOG_INFO, "Timer already active for '%s', resetting\n", t_ptr->key->label);
    
    pthread_mutex_lock(&t_ptr->thread_mutex);
    t_ptr->duration_min = duration_min;
    t_ptr->duration_sec = duration_sec;
    pthread_cond_broadcast(&t_ptr->thread_cond);
    pthread_mutex_unlock(&t_ptr->thread_mutex);
    
    pthread_mutex_unlock(&_ll_mutex);
    return;
  }

  // Create new timer
  struct timerthread *tmthread = calloc(1, sizeof(struct timerthread));
  tmthread->acddata = acddata;
  tmthread->key = key;
  tmthread->duration_min = duration_min;
  tmthread->duration_sec = duration_sec;
  tmthread->started_at = time(NULL); 
  
  // CRITICAL: Initialize threading primitives
  pthread_mutex_init(&tmthread->thread_mutex, NULL);
  pthread_cond_init(&tmthread->thread_cond, NULL);

  // Add to Linked List (Safely inside _ll_mutex lock)
  if (_timerthread_ll == NULL) {
    _timerthread_ll = tmthread;
  } else {
    for (t_ptr = _timerthread_ll; t_ptr->next != NULL; t_ptr = t_ptr->next) {}
    t_ptr->next = tmthread;
    tmthread->prev = t_ptr;
  }

  // Start Thread
  if(pthread_create(&tmthread->thread_id, NULL, timer_worker, (void*)tmthread) < 0) {
    LOG(LOG_ERR, "could not create timer thread for key '%s'\n", key->label);
    // Cleanup if creation fails
    if (tmthread->prev) tmthread->prev->next = NULL;
    else _timerthread_ll = NULL;
    pthread_mutex_destroy(&tmthread->thread_mutex);
    pthread_cond_destroy(&tmthread->thread_cond);
    free(tmthread);
    pthread_mutex_unlock(&_ll_mutex);
    return;
  }

  pthread_detach(tmthread->thread_id);
  pthread_mutex_unlock(&_ll_mutex);
}

#define WAIT_TIME_BEFORE_ON_CHECK 1000

void *timer_worker(void *ptr)
{
  struct timerthread *tmthread = (struct timerthread *) ptr;
  int retval = 0;
  int cnt = 0;

  LOG(LOG_NOTICE, "Start timer for '%s'\n", tmthread->key->label);
  tmthread->key->special_mask |= TIMER_ACTIVE;

  // Wait for device to turn on
  while (tmthread->key->state == ACD_LED_OFF) {
    LOG(LOG_DEBUG, "waiting for key state '%s' to change\n", tmthread->key->label);
    precise_delay(WAIT_TIME_BEFORE_ON_CHECK);
    if (cnt++ == 5) {
       LOG(LOG_NOTICE, "turning on '%s'\n", tmthread->key->label);
       state_change_request(tmthread->acddata, tmthread->key, ACD_LED_OFF);
    } else if (cnt == 10) {
       LOG(LOG_ERR, "key state never turned on '%s'\n", tmthread->key->label);
       break;
    }
  }

  pthread_mutex_lock(&tmthread->thread_mutex);

  struct timespec end_time;
  clock_gettime(CLOCK_REALTIME, &end_time);
  end_time.tv_sec += (tmthread->duration_min * 60) + tmthread->duration_sec;
  tmthread->started_at = time(NULL);
  
  LOG(LOG_INFO, "Timer started for '%s': %d:%02d total duration\n", tmthread->key->label, tmthread->duration_min, tmthread->duration_sec);

  while (1) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    long remaining_sec = end_time.tv_sec - now.tv_sec;

    if (remaining_sec <= 0) break;

    SET_DIRTY(tmthread->acddata->is_dirty);
    
    if (remaining_sec >= 60) {
      LOG(LOG_INFO, "Time left for '%s': %ldm %lds\n", tmthread->key->label, remaining_sec / 60, remaining_sec % 60);
    } else {
      LOG(LOG_INFO, "Time left for '%s': %ld seconds\n", tmthread->key->label, remaining_sec);
    }

    tmthread->timeout = now;
    //tmthread->timeout.tv_sec += (remaining_sec > 60) ? 60 : remaining_sec; // Wakes exactly when done if < 60s
    //tmthread->timeout.tv_sec += (remaining_sec >= 60) ? 60 : 1; // Wake ever second if under 1 minute left on timer
    tmthread->timeout.tv_sec += (remaining_sec > 60) ? (remaining_sec - 60) : 1; // Wake ever second if under 1 minute left on timer

    retval = pthread_cond_timedwait(&tmthread->thread_cond, &tmthread->thread_mutex, &tmthread->timeout);

    if (retval == 0) {
      if (tmthread->duration_min <= 0 && tmthread->duration_sec <= 0) break;

      LOG(LOG_INFO, "Timer update received for '%s'. Recalculating...\n", tmthread->key->label);
      clock_gettime(CLOCK_REALTIME, &end_time);
      end_time.tv_sec += (tmthread->duration_min * 60) + tmthread->duration_sec;
      tmthread->started_at = time(NULL); 
    } else if (retval != ETIMEDOUT) {
        LOG(LOG_ERR, "pthread_cond_timedwait error: %d\n", retval);
        break;
    }
  }

  pthread_mutex_unlock(&tmthread->thread_mutex);
  LOG(LOG_NOTICE, "End timer for '%s'\n", tmthread->key->label);

  // Determine if we timed out naturally or were cancelled
  if ((tmthread->duration_min != 0 || tmthread->duration_sec != 0) && tmthread->key->state != ACD_LED_OFF && tmthread->key->state != ACD_LED_ENABLED) {
    LOG(LOG_INFO, "Timer waking turning '%s' off\n", tmthread->key->label);
    state_change_request(tmthread->acddata, tmthread->key, ACD_LED_OFF);
  } else if (tmthread->key->state != ACD_LED_ON) {
    LOG(LOG_INFO, "Timer waking '%s' is already off\n", tmthread->key->label);
  }

  tmthread->key->special_mask &= ~TIMER_ACTIVE;

  // --- CRITICAL FIX: Lock Linked List before modifying and freeing ---
  pthread_mutex_lock(&_ll_mutex);
  
  if (tmthread->next != NULL && tmthread->prev != NULL) {
    tmthread->next->prev = tmthread->prev;
    tmthread->prev->next = tmthread->next;
  } else if (tmthread->next == NULL && tmthread->prev != NULL) {
    tmthread->prev->next = NULL;
  } else if (tmthread->next != NULL && tmthread->prev == NULL) {
    _timerthread_ll = tmthread->next;
    _timerthread_ll->prev = NULL;
  } else if (tmthread->next == NULL && tmthread->prev == NULL) {
    _timerthread_ll = NULL;
  }

  // Destroy primitives before freeing memory
  pthread_mutex_destroy(&tmthread->thread_mutex);
  pthread_cond_destroy(&tmthread->thread_cond);
  
  free(tmthread);
  pthread_mutex_unlock(&_ll_mutex);
  
  pthread_exit(0);
  return NULL;
}





#ifdef DO_NOT_COMPILE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#include "aquachemd.h"
#include "utils.h"
#include "acd_timer.h"


struct timerthread {
  pthread_t thread_id;
  pthread_mutex_t thread_mutex;
  pthread_cond_t thread_cond;
  acd_key_t *key;
  int deviceIndex;
  struct aquachemdata *acddata;
  int duration_min;
  u_int32_t duration_sec;
  struct timespec timeout;
  time_t started_at;
  struct timerthread *next;
  struct timerthread *prev;
};

/*volatile*/ static struct timerthread *_timerthread_ll = NULL;

void *timer_worker( void *ptr );

struct timerthread *find_timerthread(acd_key_t *key)
{
  struct timerthread *t_ptr;

  if (_timerthread_ll != NULL) {
    for (t_ptr = _timerthread_ll; t_ptr != NULL; t_ptr = t_ptr->next) {
      if (t_ptr->key == key) {
        return t_ptr;
      }
    }
  }

  return NULL;
}

int get_timer_left(acd_key_t *key)
{
  struct timerthread *t_ptr = find_timerthread(key);

  if (t_ptr != NULL) {
    time_t now = time(0);
    double seconds = difftime(now, t_ptr->started_at);
    return (int) ((t_ptr->duration_min - (seconds / 60)) +0.5) ;
  }

  return 0;
}

uint32_t get_timer_left_sec(acd_key_t *key)
{
  struct timerthread *t_ptr = find_timerthread(key);

  if (t_ptr != NULL) {
    time_t now = time(0);
    long total_duration_sec = (t_ptr->duration_min * 60) + t_ptr->duration_sec;
    long elapsed_sec = (long)difftime(now, t_ptr->started_at);
    if (elapsed_sec >= total_duration_sec) {
        return 0;
    }
    
    return (uint32_t)(total_duration_sec - elapsed_sec);
  }

  return 0;
}

void clear_timer(struct aquachemdata *acddata, int deviceIndex)
{
  //struct timerthread *t_ptr = find_timerthread(key);
  struct timerthread *t_ptr = find_timerthread(&acddata->aqkeys[deviceIndex]);

  if (t_ptr != NULL) {
    LOG( LOG_INFO, "Clearing timer for '%s'\n",t_ptr->key->label);
    t_ptr->duration_min = 0;
    t_ptr->duration_sec = 0;
    pthread_cond_broadcast(&t_ptr->thread_cond);
  }
}


void start_timer(struct aquachemdata *acddata, int deviceIndex, int duration_min, u_int32_t duration_sec)
{
  acd_key_t *key = &acddata->aqkeys[deviceIndex];
  struct timerthread *t_ptr = find_timerthread(key);

  if (t_ptr != NULL) {
    LOG( LOG_INFO, "Timer already active for '%s', resetting\n",t_ptr->key->label);
    t_ptr->duration_min = duration_min;
    t_ptr->duration_sec = duration_sec;
    pthread_cond_broadcast(&t_ptr->thread_cond);
    return;
  }

  struct timerthread *tmthread = calloc(1, sizeof(struct timerthread));
  tmthread->acddata = acddata;
  tmthread->key = key;
  tmthread->deviceIndex = deviceIndex;
  tmthread->thread_id = 0;
  tmthread->duration_min = duration_min;
  tmthread->duration_sec = duration_sec;
  tmthread->next = NULL;
  tmthread->started_at = time(0); // This will get reset once we actually start. But need it here incase someone calls get_timer_left() before we start

  if( pthread_create( &tmthread->thread_id , NULL ,  timer_worker, (void*)tmthread) < 0) {
    LOG( LOG_ERR, "could not create timer thread for key '%s'\n",key->label);
    free(tmthread);
    return;
  }

  if (_timerthread_ll == NULL) {
    _timerthread_ll = tmthread;
    _timerthread_ll->prev = NULL;
    //LOG( LOG_NOTICE, "Added Timer '%s' at beginning LL\n",_timerthread_ll->key->label);
  }
  else
  {
    for (t_ptr = _timerthread_ll; t_ptr->next != NULL; t_ptr = t_ptr->next) {} // Simply run to the end of the list
    t_ptr->next = tmthread;
    tmthread->prev = t_ptr;
    //LOG( LOG_NOTICE, "Added Timer '%s' at end LL \n",tmthread->key->label);
  }
  
  if ( tmthread->thread_id != 0 ) {
    pthread_detach(tmthread->thread_id);
  }
}

#define WAIT_TIME_BEFORE_ON_CHECK 1000
//#define WAIT_TIME_BEFORE_ON_CHECK 1000000 // 1 second

void *timer_worker( void *ptr )
{
  struct timerthread *tmthread;
  tmthread = (struct timerthread *) ptr;
  int retval = 0;
  int cnt=0;

  LOG( LOG_NOTICE, "Start timer for '%s'\n",tmthread->key->label);

  // Add mask so we know timer is active
  tmthread->key->special_mask |= TIMER_ACTIVE;

  while (tmthread->key->led->state == OFF) {
    LOG( LOG_DEBUG, "waiting for key state '%s' to change\n",tmthread->key->label);
    delay(WAIT_TIME_BEFORE_ON_CHECK);
    if (cnt++ == 5 && !isPDA_PANEL) {
       LOG( LOG_NOTICE, "turning on '%s'\n",tmthread->key->label);
       panel_device_request(tmthread->acddata, ON_OFF, tmthread->deviceIndex, true, NET_TIMER);
    } else if (cnt == 10) {
       LOG( LOG_ERR, "key state never turned on'%s'\n",tmthread->key->label);
       break;
    }
  }

  // Waake every minute (or second) and set the dirty flag.
  pthread_mutex_lock(&tmthread->thread_mutex);

  // Calculate the absolute end time once
  struct timespec end_time;
  clock_gettime(CLOCK_REALTIME, &end_time);
  end_time.tv_sec += (tmthread->duration_min * 60) + tmthread->duration_sec;

  tmthread->started_at = time(0);
  LOG( LOG_INFO, "Timer started for '%s': %d:%02d total duration\n", tmthread->key->label, tmthread->duration_min, tmthread->duration_sec);

  while (1) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    // Calculate remaining time
    long remaining_sec = end_time.tv_sec - now.tv_sec;

    if (remaining_sec <= 0) {
        break; // Timer finished
    }

    SET_DIRTY(tmthread->acddata->is_dirty);
    // 3. Print time left
    if (remaining_sec >= 60) {
      LOG( LOG_INFO, "Time left for '%s': %ldm %lds\n", tmthread->key->label, remaining_sec / 60, remaining_sec % 60);
    } else {
      LOG( LOG_INFO, "Time left for '%s': %ld seconds\n", tmthread->key->label, remaining_sec);
    }

    // Set next wake time
    tmthread->timeout = now;
    tmthread->timeout.tv_sec += (remaining_sec > 60) ? 60 : 1;

    // Wait for timeout or signal
    retval = pthread_cond_timedwait(&tmthread->thread_cond, &tmthread->thread_mutex, &tmthread->timeout);

    if (retval == 0) {
      // We were signaled! Someone changed tmthread->duration_min or duration_sec
      if (tmthread->duration_min <= 0 && tmthread->duration_sec <= 0) {
        break; // Cancelled
      }

      LOG( LOG_INFO, "Timer update received for '%s'. Recalculating...\n", tmthread->key->label);
        
      // Update the end_time based on the NEW duration
      clock_gettime(CLOCK_REALTIME, &end_time);
      end_time.tv_sec += (tmthread->duration_min * 60) + tmthread->duration_sec;
      // Also update started_at so get_timer_left_sec() remains accurate
      tmthread->started_at = time(0); 
    } 
    else if (retval != ETIMEDOUT) {
        LOG( LOG_ERR, "pthread_cond_timedwait error: %d\n", retval);
        break;
    }
  }

  pthread_mutex_unlock(&tmthread->thread_mutex);
  LOG( LOG_NOTICE, "End timer for '%s'\n", tmthread->key->label);

  // We need to detect if we ended on time or were killed.  
  // If killed the device is probable off (or being set to off), so we should probably poll a few times before turning off.
  // Either that of change ap_panel to not turn off device if timer is set.

  //LOG( LOG_NOTICE, "End timer duration '%d'\n",tmthread->duration_min); 

  // if duration_min is 0 we were killed, if not we got here on timeout, so turn off device.

  if ( (tmthread->duration_min != 0 || tmthread->duration_sec != 0) && tmthread->key->led->state != OFF) {
    LOG( LOG_INFO, "Timer waking turning '%s' off\n",tmthread->key->label);
    panel_device_request(tmthread->acddata, ON_OFF, tmthread->deviceIndex, false, NET_TIMER);
  } else if (tmthread->key->led->state == OFF) {
    LOG( LOG_INFO, "Timer waking '%s' is already off\n",tmthread->key->label);
  }
  
  if (tmthread->key->led->state != OFF) {
    // Need to wait
  }

  // remove mask so we know timer is dead
  tmthread->key->special_mask &= ~ TIMER_ACTIVE;

  if (tmthread->next != NULL && tmthread->prev != NULL){
    // Middle of linked list
    tmthread->next->prev = tmthread->prev;
    tmthread->prev->next = tmthread->next;
    //LOG( LOG_NOTICE, "Removed Timer '%s' from middle LL\n",tmthread->key->label);
  } else if (tmthread->next == NULL && tmthread->prev != NULL){
    // end of linked list
    tmthread->prev->next = NULL;
    //LOG( LOG_NOTICE, "Removed Timer '%s' from end LL\n",tmthread->key->label);
  } else if (tmthread->next != NULL && tmthread->prev == NULL){
    // beginning of linked list
    _timerthread_ll = tmthread->next;
    _timerthread_ll->prev = NULL;
    //LOG( LOG_NOTICE, "Removed Timer '%s' from beginning LL\n",tmthread->key->label);
  } else if (tmthread->next == NULL && tmthread->prev == NULL){
    // only item in list
    _timerthread_ll = NULL;
    //LOG( LOG_NOTICE, "Removed Timer '%s' last LL\n",tmthread->key->label);
  }

  free(tmthread);
  pthread_exit(0);

  return ptr;
}

#endif