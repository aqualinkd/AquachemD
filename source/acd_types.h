#ifndef ACD_TYPES_H_
#define ACD_TYPES_H_  

#include "ezo.h"
#include "1wire.h"
#include "gpio.h"


// Don't have a MQTT sensor code, so place it here.
typedef struct {
  char *topic;
} mqtt_sensor_t;

typedef struct {
  unsigned char address;
} ezo_sensor_t;


typedef enum {
    ACD_LED_UNKNOWN = -1,
    ACD_LED_OFF = 0,
    ACD_LED_ON,
    ACD_LED_ENABLED
} acd_state_t;

typedef enum {
    KEY_TYPE_NONE = 0,
    KEY_TYPE_EZO_PH,
    KEY_TYPE_EZO_ORP,
    //KEY_TYPE_EZO_PUMP,
    KEY_TYPE_EZO_TEMP,
    KEY_TYPE_MQTT_TEMP,
    KEY_TYPE_D1W_TEMP
    //KEY_TYPE_GPIO
    //KEY_TYPE_W1
} acd_sensor_type_t;

typedef struct acd_key_t {
    acd_sensor_type_t type;
    acd_state_t state;
    char *label;
    char *ID;
    uint8_t index; //values from 0 to 255

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


#define UNKNOWN -9999 
#define MASTER_ID 1

#endif // ACD_TYPES_H_