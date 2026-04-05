#ifndef AQUACHEMD_H_
#define AQUACHEMD_H_


#include <stdbool.h>
#include "ezo.h"
#include "1wire.h"
#include "gpio.h"


void intHandler(int sig_num);

#define SET_DIRTY(flag)    ((flag) = true)
#define CLEAR_DIRTY(flag)  ((flag) = false)

struct aquachemdata
{
  volatile bool is_dirty;

  int open_websockets;
  bool acdManagerActive;

  ph_reading_t ph_reading;
  orp_reading_t orp_reading;
  rtd_reading_t temp_reading;
};


#define UNKNOWN -9999 


#endif // AQUACHEMD_H_

