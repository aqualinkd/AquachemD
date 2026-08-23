#ifndef ACD_TYPES_H_
#define ACD_TYPES_H_  

#include <time.h>
#include <stdatomic.h>

#include "ezo.h"
#include "1wire.h"
#include "gpio.h"
#include "sysfs.h"
#include "i2c.h"
#include "uom.h"

/*
typedef enum {
  ACD_DAY,
  ACD_WEEK
} acd_period_t;   // Just used for metrics at the moment.
*/

#define ACD_MASTER_ID "AquachemD"

typedef enum {
  ACD_STARTING,
  ACD_KEEPRUNNING,
  ACD_RELOAD,
  ACD_CLEANUP,
  ACD_FINISHED,
  ACD_FAILED
} acd_runstate_t;

typedef struct {
  pthread_t parent_id;
  pthread_t id;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  //acd_runstate_t state;
  atomic_int state; 
} acd_thread_t;

/* Use as below
static acd_thread_t _my_worker = {
    .parent_id = 0,
    .id = 0,
    .mutex = PTHREAD_MUTEX_INITIALIZER, 
    .cond = PTHREAD_COND_INITIALIZER, 
    .state = ACD_STARTING
};
and
 if (atomic_load_explicit(&_my_worker.state, memory_order_relaxed) != ACD_KEEPRUNNING) {
 ..........
 }
*/


typedef enum {
    ACD_TYPE_NONE = 0,
    
    // Special
    ACD_TYPE_MASTER,

    // Conditions
    ACD_TYPE_MQTT_COND,
    ACD_TYPE_GPIO_COND,
    #define ACD_COND_FIRST ACD_TYPE_MQTT_COND
    #define ACD_COND_LAST  ACD_TYPE_GPIO_COND

    // Inputs (Sensors)
    ACD_TYPE_EZO_PH,
    ACD_TYPE_EZO_ORP,
    ACD_TYPE_EZO_TEMP,
    ACD_TYPE_EZO_PRS,
    ACD_TYPE_I2C_PRS,
    ACD_TYPE_I2C_TEMP,
    ACD_TYPE_MQTT_TEMP,
    ACD_TYPE_D1W_TEMP,
    ACD_TYPE_SYSFS_VALUE,
    ACD_TYPE_MQTT_VALUE,
    #define ACD_IN_FIRST ACD_TYPE_EZO_PH
    //#define ACD_IN_LAST  ACD_TYPE_D1W_TEMP
    #define ACD_IN_LAST  ACD_TYPE_MQTT_VALUE


    // Outputs (Actuators)
    ACD_TYPE_GPIO_PMP,
    //ACD_TYPE_GPIO_PMP_PH,
    //ACD_TYPE_GPIO_PMP_ORP,
    ACD_TYPE_EZO_PMP,
    #define ACD_OUT_FIRST ACD_TYPE_GPIO_PMP
    #define ACD_OUT_LAST  ACD_TYPE_EZO_PMP
} acd_type_t;

/* Grouping Macros */

// Returns true for MQTT and GPIO conditions, but false for MASTER
#define IS_CONDITION(t) ((t) >= ACD_COND_FIRST && (t) <= ACD_COND_LAST)

// Returns true for any input sensor (pH, Temp, etc.)
#define IS_INPUT(t)     ((t) >= ACD_IN_FIRST   && (t) <= ACD_IN_LAST)

// Returns true for any output/actuator (Pumps, etc.)
#define IS_OUTPUT(t)    ((t) >= ACD_OUT_FIRST  && (t) <= ACD_OUT_LAST)

// Don't have a MQTT sensor code, so place it here.
typedef struct {
  char *topic;
  char *target_value; 
  //char *current_value; 
} mqtt_sensor_t;

typedef struct {
  unsigned char address;
} ezo_sensor_t;

typedef enum {
    ACD_LED_UNKNOWN = -1,
    ACD_LED_OFF = 0,     // Maps to false
    ACD_LED_ON = 1,      // Maps to true
    ACD_LED_ENABLED,
    ACD_LED_DISABLED, // Sensor caused button to be disabled.
    ACD_LED_DELAY // Condition has a delay before turning on.
} acd_state_t;

// acd_scope_t is reused by two different kinds of nodes, and means something
// slightly different on each. It is set by the various "*_scope_global" config
// options (e.g. mqtt_condition_scope_global, gpio_condition_scope_global,
// ph_sensor_scope_global, orp_sensor_scope_global, prs_sensor_scope_global,
// temp_sensor_scope_global). All of these options are simple true/false, and
// map true -> ACD_SCOPE_GLOBAL, false -> ACD_SCOPE_LOCAL. ACD_SCOPE_ALLOW is
// never set by config; it only ever appears on the master key.
//
// On a CONDITION (mqtt_condition / gpio_condition, an interlock like
// "Filter Pump Running" or "Flow Switch"), scope controls how severely a
// failed/unmet condition affects the whole system:
//   scope_global = false (LOCAL)  -> Soft Limit.
//       Failing this condition disables/stops the dosing pumps (outputs)
//       only. All sensors keep sampling and reporting normally.
//       e.g. "tank running low -> pause dosing, but keep monitoring".
//   scope_global = true  (GLOBAL) -> Hard Interlock.
//       Failing this condition forces the whole system into BLOCK: any
//       running dosing pump is shut off immediately, all outputs are
//       disabled, AND any sensor that is itself scope_global=true also
//       stops being polled. Sensors marked scope_global=false keep
//       reading even through a hard interlock.
//       e.g. "no flow -> don't dose, and don't bother reading the
//       flow-dependent probes either".
// The master's scope is always the worst (highest) of all its conditions'
// scopes - a single GLOBAL condition failing overrides any LOCAL ones.
//
// On a SENSOR (ph/orp/prs/temp/etc.), scope only matters once some
// condition has already put the master into a degraded state:
//   scope_global = false (LOCAL)  -> This sensor keeps reporting values
//       no matter what, even during a hard (GLOBAL) interlock shutdown.
//       Useful for e.g. an externally-fed MQTT reading you still want
//       visible in Home Assistant while dosing is halted.
//   scope_global = true  (GLOBAL) -> This sensor is paused/disabled
//       whenever a GLOBAL condition trips the master into BLOCK.
// Sensor scope has no effect while the master is ALLOW or LIMIT - all
// sensors read normally in those states regardless of their own scope.

typedef enum {
    ACD_SCOPE_ALLOW  = 0, // Default / No restriction (ONLY FOR MASTER)
    ACD_SCOPE_LOCAL  = 1, // Acts as a Soft Limit for specific outputs
    ACD_SCOPE_GLOBAL = 2  // Acts as a Hard Interlock for the whole system
} acd_scope_t;

// Inverse the names for conditions to make code easier to read.
#define ACD_ACTION_ALLOW  ACD_SCOPE_ALLOW
#define ACD_ACTION_LIMIT  ACD_SCOPE_LOCAL
#define ACD_ACTION_BLOCK  ACD_SCOPE_GLOBAL


typedef struct {
    float average;
    time_t last_sample_time;
    float min;
    float max;
    uint32_t sample_count;   // replaces tau_seconds' role in the calc
    float tau_seconds;       // Probably need to remove in future
    pthread_mutex_t lock;
} sensor_stats_t;


typedef struct acd_key_t {
    acd_type_t type;
    acd_state_t state;
    acd_scope_t scope;
    
    volatile bool is_dirty;
    char *label;
    char *ID;
    uint8_t index; //values from 0 to 255
    uint16_t flags; // Any bitmasks (like timer active, pump type, global interlock)
    uint8_t err_cnt;
    
    float value; // sensor uses for current value, pump uses for value of ph/orp when turned on.
    acd_uom_t uom;

    union {
      float flow_rate;      // Pumps     = ml per second rate for pumps
      uint32_t delay_on;    // Condition = used for a delay before setting to on.
      sensor_stats_t stats; // Sensor    = used for statics.
    };

    union {
      bool met;    // For conditions, met or not.
      bool ison;   // For output pump,
    };

    union {
        ezo_sensor_t   ezo;
        gpio_handle_t  gpio;
        w1_sensor_t    w1;
        mqtt_sensor_t  mqtt;
        sysfs_sensor_t sysfs;  
        i2c_sensor_t   i2c;
    } data;

    struct acd_key_t *child;  // Virtual key if sensor has multiple outputs (like PTE7300 with pressure and temperature)
    struct acd_key_t *next;
} acd_key_t;

typedef struct runtime_range_t{
  float threshold;
  uint32_t seconds;
} runtime_range_t;



#define MAX_DOSING_RANGES 10

// For special_mask in acd_key
#define TIMER_ACTIVE           (1 << 0)
#define DELAY_ACTIVE           (1 << 1)
#define PH_PUMP                (1 << 2) 
#define ORP_PUMP               (1 << 3)

#define ACD_FLAG_FAULTED       (1 << 4)   
#define ACD_FLAG_ACTIVE        (1 << 5)   // not used at present

#define CALC_AVERAGE           (1 << 6)

#define CONDITION_NOTIFIED     (1 << 7)

#define ACD_FLAG_VIRTUAL       (1 << 8)

// CAN'T ADD ANY MORE wuthout changeing uint8_t to uint16_t 
//#define CONDITION_SCOPE_GLOBAL (1 << 3) // For conditions Set if global interlock, clear if local restriction
//#define CONDITION_SCOPE_LOCAL  (1 << 4) // ONLY for master key, if on and this is set, then re can read sensors but not dose.

/*
#define XXX            (1 << 3) // For special_mask's
#define XXX            (1 << 4) // For special_mask's
#define XXX            (1 << 5) // For special_mask's
#define XXX            (1 << 6) // For special_mask's
#define XXX            (1 << 7) // For special_mask's
// Change acd_key_t->flags & config.c - _staging->flags before defining more,
*/

/*
#define isMASKSET(bitmask, mask) ((bitmask & mask) == mask)
#define setMASK(bitmask, mask)    (bitmask |= mask)
*/

#define UNKNOWN -9999 
#define MASTER_ID 1



/*
   Logic for dosing events / Pull information like.
   journalctl PUMP_ID=PMP_1 --since "7 days ago" -o json | jq -r '.RUNTIME_SEC' | awk '{sum+=$1} END {print sum}'

   Hash string "ACD-PMP-Event", you get valid 32-character ID
   The 128-bit Message ID 332918807d4b46949f50e93149872583
*/
#define SD_PUMP_EVENT_ID "332918807d4b46949f50e93149872583"
#define SD_MESSAGE_STARTUP_ID "5e982c7a12344567890abcdef1234567"
#define SD_MESSAGE_UPGRADE_ID "c3b9b418e24440939b4bfae6dfbc1122"


#endif // ACD_TYPES_H_