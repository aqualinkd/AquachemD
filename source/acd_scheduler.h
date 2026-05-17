#ifndef ACD_SCHEDULER_H_
#define ACD_SCHEDULER_H_

#include "config.h"

#define CRON_FILE "/etc/cron.d/aquachemd"
#define CURL "curl"

#define CV_SIZE 20

typedef struct aqs_cron
{
  int enabled;
  char minute[CV_SIZE];
  char hour[CV_SIZE];
  char daym[CV_SIZE];
  char month[CV_SIZE];
  char dayw[CV_SIZE];
  char url[CV_SIZE * 2];
  char value[CV_SIZE];
} aqs_cron;

int build_schedules_js(char* buffer, int size);
int save_schedules_js(const char* inBuf, int inSize, char* outBuf, int outSize);
void get_cron_pump_times();







#endif /* ACD_SCHEDULER_H_ */

