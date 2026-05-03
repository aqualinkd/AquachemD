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
    uint8_t special_mask; // Any special masks (like timer active)

    union {
      float value; // Maybe needs to be moved into sensor typedef's?????
      int runtime; // default runtime for pumps
    };

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


#define TIMER_ACTIVE   (1 << 0) // For special_mask's

#define UNKNOWN -9999 
#define MASTER_ID 1


#endif // ACD_TYPES_H_