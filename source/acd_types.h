#ifndef ACD_TYPES_H_
#define ACD_TYPES_H_  

#include "ezo.h"
#include "1wire.h"
#include "gpio.h"


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

typedef enum {
    KEY_TYPE_NONE = 0,
    KEY_TYPE_MASTER,
    KEY_TYPE_EZO_PH,
    KEY_TYPE_EZO_ORP,
    //KEY_TYPE_EZO_PUMP,
    KEY_TYPE_EZO_TEMP,
    KEY_TYPE_MQTT_TEMP,
    KEY_TYPE_D1W_TEMP,
    KEY_TYPE_GPIO_DOSER,
    KEY_TYPE_EZO_DOSER
    //KEY_TYPE_GPIO
    //KEY_TYPE_W1
} acd_sensor_type_t;

#define TIMER_ACTIVE   (1 << 0) 


typedef struct acd_key_t {
    acd_sensor_type_t type;
    acd_state_t state;
    //acd_state_t last_state; // Used to store the last state 
    char *label;
    char *ID;
    uint8_t index; //values from 0 to 255
    uint8_t special_mask; // Any special masks (lite timer active)

    float value; // Maybe needs to be moved into sensor typedef's?????

    union {
        //ph_reading_t   ph;
        //orp_reading_t  orp;
        //rtd_reading_t  rtd;
        //pump_status_t  pump;
        ezo_sensor_t  ezo;
        gpio_handle_t  gpio;
        w1_sensor_t    w1;
        mqtt_sensor_t mqtt;
    } data;

    struct acd_key_t *next;
} acd_key_t;


typedef enum {
  COND_MQTT,
  COND_GPIO
} acd_condition_type_t;

typedef struct acd_condition_t {
  acd_condition_type_t type;
  char *label;
  char *ID;
  uint8_t index;
  bool met; 
  //int target_value; // -9999/UNKNOWN for mqtt means use mqtt.target_value which is a string

  union {
    mqtt_sensor_t mqtt;
    gpio_handle_t gpio;  
  } data;
  
  struct acd_condition_t *next;
} acd_condition_t;

/*
typedef struct acd_condition_t {
  acd_condition_type_t type;
  char *label;
  char *ID;
  uint8_t index; //values from 0 to 255
  bool met; // Whether the condition is currently met or not.

  union {
    struct {
      char *mqtt_topic;
      char *mqtt_value; // Good value for the topic to be considered "met"
      char *mqtt_current_value; // Store the current value of the topic so we can check if it has changed in the main loop
    };
    struct {
      int gpio_pin;
      bool gpio_value; // Good value for the topic to be considered "met"
      bool gpio_current_value; // Store the current value of the GPIO pin so we can check if it has changed in the main loop
      gpio_handle_t *gpio_handle; // Store the GPIO handle for this condition so we can read it in the main loop
    };
  };
  
  struct acd_condition_t *next;
} acd_condition_t;
*/

#define IS_DOSER_TYPE(t) ((t) == KEY_TYPE_GPIO_DOSER || (t) == KEY_TYPE_EZO_DOSER)

#define UNKNOWN -9999 
#define MASTER_ID 1

#endif // ACD_TYPES_H_