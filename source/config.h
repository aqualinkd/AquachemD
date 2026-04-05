#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdbool.h>
#include <stdint.h>

#include "aquachemd.h"

void parse_config_file(struct aquachemdata *acdata);
bool write_config_file (struct aquachemdata *acdata);
void check_print_config (struct aquachemdata *acdata);

#define MAXCFGLINE 256

struct acdconfig
{
#if MG_TLS > 0
  char *cert_dir;  // for future
  char *mqtt_cert_dir;
#endif
  char *config_file;
  char *listen_address;
  unsigned int log_level;
  unsigned int mg_log_level;
  char *web_directory;

  bool deamonize;

  char *mqtt_aquachemd_topic;
  char *mqtt_aqualinkd_topic;
  char *mqtt_discovery_topic;
  char *mqtt_server;
  char *mqtt_user;
  char *mqtt_passwd;
  bool mqtt_discovery_use_mac;
  
  //char mqtt_ID[MQTT_ID_LEN+1];

  bool convert_mqtt_temp;
  bool mqtt_timed_update;
  
  int sensor_poll_time;
};



typedef enum cfg_value_type{
  CFG_STRING,
  CFG_INT,
  CFG_FLOAT,
  CFG_HEX,
  CFG_BOOL,
  CFG_BITMASK,
  CFG_SPECIAL
} cfg_value_type;

typedef struct cfgParam {
  void *value_ptr;
  void *default_value;
  cfg_value_type value_type;
  uint16_t config_mask;
  char *name;
  char *valid_values;
  uint16_t mask;
} cfgParam;

#define CFG_PERSISTANT        (1 << 0) // Don't free memory, things referance the pointer
#define CFG_GRP_ADVANCED      (1 << 1) // Show in group advanced
#define CFG_READONLY          (1 << 2) // Don't show in UI, but do write to CFG file. (Maybe display in UI but no edit)
#define CFG_HIDE              (1 << 3) // Don't show in any UI listing, don't write to CFG file.
#define CFG_PASSWD_MASK       (1 << 4) // Mask password with *****
#define CFG_FORCE_RESTART     (1 << 5) // Force aqualinkd to restart
#define CFG_ALLOW_BLANK       (1 << 6) // Allow blank entry
#define CFG_GREYED_OUT        (1 << 7) // Greyout in UI, show but not editable
//#define CFG_      (1 << 3)

// Text to show when CFG_PASSWD_MASK is set
#define PASSWD_MASK_TEXT "********"


#ifndef CONFIG_C
extern struct acdconfig _acdconfig_;
#else
struct acdconfig _acdconfig_;
#endif

#ifndef CONFIG_C
extern cfgParam _cfgParams[];
extern int _numCfgParams;
#else
cfgParam _cfgParams[100];
int _numCfgParams;
#endif // CONFIG_C

#define CFG_N_listen_address "listen_address"
#define CFG_N_cert_dir "cert_dir"
#define CFG_N_serial_port "serial_port"
#define CFG_N_log_level "log_level"
#define CFG_N_MG_log_level "mg_log_level"
#define CFG_N_web_directory "web_directory"
#define CFG_N_mqtt_aquachemd_topic "mqtt_aquachemd_topic"
#define CFG_N_mqtt_aqualinkd_topic "mqtt_aqualinkd_topic" 
#define CFG_N_mqtt_discovery_topic "mqtt_discovery_topic"
#define CFG_N_mqtt_discovery_use_mac "mqtt_discovery_use_mac"
#define CFG_N_convert_mqtt_temp "mqtt_convert_to_degF"
#define CFG_N_mqtt_timed_update "mqtt_timed_update"
#define CFG_N_sensor_poll_time "sensor_poll_time"
#define CFG_N_mqtt_server "mqtt_server"
#define CFG_N_mqtt_user "mqtt_user"
#define CFG_N_mqtt_passwd "mqtt_passwd"

#define CFG_V_log_level                         "[\"DEBUG_SERIAL\", \"DEBUG\", \"INFO\", \"NOTICE\", \"WARNING\", \"ERROR\"]"
#define CFG_V_BOOL                              "[\"Yes\", \"No\"]"

#endif // CONFIG_H_
