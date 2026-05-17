#ifndef ACD_TYPES_H_
#define ACD_TYPES_H_  

#include "ezo.h"
#include "1wire.h"
#include "gpio.h"



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
    ACD_TYPE_MQTT_TEMP,
    ACD_TYPE_D1W_TEMP,
    #define ACD_IN_FIRST ACD_TYPE_EZO_PH
    #define ACD_IN_LAST  ACD_TYPE_D1W_TEMP

    // Outputs (Actuators)
    ACD_TYPE_GPIO_PMP,
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
    ACD_LED_OFF = 0,
    ACD_LED_ON,
    ACD_LED_ENABLED,
    ACD_LED_DISABLED // Sensor caused button to be disabled.
} acd_state_t;


typedef struct acd_key_t {
    acd_type_t type;
    acd_state_t state;
    char *label;
    char *ID;
    uint8_t index; //values from 0 to 255
    uint8_t flags; // Any bitmasks (like timer active, or pump type)
    uint8_t err_cnt;
    
    float value; // sensor uses for current value, pump uses for value of ph/orp when turned on.
    float flow_rate; // ml per second rate for pumps

    union {
      bool met;    // For conditions, met or not.
      bool ison;   // For output pump,  
    };

    union {
        ezo_sensor_t  ezo;
        gpio_handle_t gpio;
        w1_sensor_t   w1;
        mqtt_sensor_t mqtt;
    } data;

    struct acd_key_t *next;
} acd_key_t;

typedef struct runtime_range_t{
  float threshold;
  uint32_t seconds;
} runtime_range_t;



#define MAX_DOSING_RANGES 5


#define TIMER_ACTIVE   (1 << 0) // For special_mask's
#define PH_PUMP        (1 << 1) // For special_mask's
#define ORP_PUMP       (1 << 2) // For special_mask's
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

#endif // ACD_TYPES_H_