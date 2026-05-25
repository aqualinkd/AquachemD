#ifndef AQUACHEMD_H_
#define AQUACHEMD_H_


#include <stdbool.h>
//#include "ezo.h"
//#include "1wire.h"
//#include "gpio.h"
#include "config.h"
#include "acd_types.h"


#define SET_DIRTY(flag)    ((flag) = true)
#define CLEAR_DIRTY(flag)  ((flag) = false)

#define SENSOR_FAULT_THRESHOLD 4

//void setKeyLed(struct aquachemdata *acdata, acd_key_t *key, acd_state_t state);
//bool stateChangeRequest(struct aquachemdata *acdata, acd_key_t *key, acd_state_t state);

void intHandler(int sig_num);


/**
 * SET_IF_CHANGED: Updates a variable and sets a flag if the value has changed.
 *
 * @src: The variable to be updated (can be a struct member).
 * @val: The new value.
 * @flag: A boolean flag to set to true if a change occurs.
 *
 * This macro uses GCC extensions for type safety and to prevent
 * double-evaluation of the `val` argument.
 */
#include <stdbool.h>
#include <string.h>

#define SET_IF_CHANGED(src, val, flag) \
    ({                                                           \
        __typeof__(src) __new_val = (val);                       \
        bool __changed = false;                                  \
        if ((src) != __new_val) {                                \
            (src) = __new_val;                                   \
            (flag) = true;   /* Set ONLY on change, never cleared */ \
            __changed = true;                                    \
        }                                                        \
        __changed;           /* Evaluates/returns bool to caller */ \
    })

#define SET_IF_CHANGED_STRCPY(src, val, flag)                  \
    ({                                                         \
        const char *__new_val = (val);                         \
        bool __changed = false;                                \
        if (strncmp((src), __new_val, sizeof(src)) != 0) {     \
            strncpy((src), __new_val, sizeof(src));            \
            (src)[sizeof(src) - 1] = '\0';                     \
            (flag) = true;   /* Set ONLY on change, never cleared */ \
            __changed = true;                                    \
        }                                                      \
        __changed;           /* Evaluates/returns bool to caller */ \
    })
 /*
#define SET_IF_CHANGED(src, val, flag) \
    ({                                                           \
        __typeof__(src) __new_val = (val);                       \
        if ((src) != __new_val) {                                \
            (src) = __new_val;                                   \
            (flag) = true;                                       \
        }                                                        \
    })

#define SET_IF_CHANGED_STRCPY(src, val, flag)                  \
    ({                                                         \
        const char *__new_val = (val);                         \
        if (strncmp((src), __new_val, sizeof(src)) != 0) {     \
            strncpy((src), __new_val, sizeof(src));            \
            (src)[sizeof(src) - 1] = '\0';                     \
            (flag) = true;                                     \
        }                                                      \
    })
*/

#define DISPLAY_MSG_SIZE 64

struct aquachemdata
{
  char self[64];  // Filename being executed
  volatile bool is_dirty;

  int open_websockets;
  bool acdManagerActive;

  //ph_reading_t ph_reading;
  //orp_reading_t orp_reading;
  //rtd_reading_t temp_reading;

  //acd_condition_t *conditions;
  acd_key_t *keys; // Linked list of all keys (sensors, pumps, GPIOs, etc.) for easy access and management

  char display_message[DISPLAY_MSG_SIZE];
};


#define UNKNOWN -9999 


#endif // AQUACHEMD_H_

