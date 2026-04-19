#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdbool.h>
#include <stdint.h>


#include "acd_types.h"
//#include "gpio.h" //typdef struct gpio_handle_t;

// Forward Declaration: Prevents circular dependency
struct aquachemdata; 

// Forward Declaration: If needed for other prototypes
struct acd_condition;



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
  char *main_label;
  char *config_file;
  char *listen_address;
  unsigned int log_level;
  unsigned int mg_log_level;
  char *web_directory;

  //bool deamonize;

  char *mqtt_aquachemd_topic;
  char *mqtt_aqualinkd_topic;
  char *mqtt_discovery_topic;
  char *mqtt_server;
  char *mqtt_user;
  char *mqtt_passwd;
  bool mqtt_discovery_use_mac;
  
  char *gpio_chip;
  //char mqtt_ID[MQTT_ID_LEN+1];

  bool convert_mqtt_temp;
  bool mqtt_timed_update;
  
  int ph_reading_temp_max;
  int ph_reading_temp_min;

  int sensor_poll_time;

  bool post_condition; // Whether to post & display conditions that are met/unmet
  bool temp_compensated_ph;

  unsigned char test_hex;
  float test_float;
  uint16_t test_bitmask;

  acd_condition_t *conditions;
  acd_key_t *sensors;
};



typedef enum cfg_value_type{
  CFG_STRING,
  CFG_INT,
  CFG_FLOAT,
  CFG_HEX,
  CFG_BOOL,
  CFG_BITMASK,
  CFG_TXT_INT,  // Int that should be displayed as text (ie log_level)
  CFG_CUSTOM
} cfg_value_type;

typedef struct cfgParam {
  void *value_ptr;
  cfg_value_type value_type;
  uint16_t config_mask;
  char *name;
  char *metadata; // For dropdowns, etc. JSON string that the UI can parse to get options, etc. (ie for log_level: ["DEBUG_SERIAL", "DEBUG", "INFO", "NOTICE", "WARNING", "ERROR"])
  uint16_t bit_flag; // For bitmask types, the specific bit to toggle for this param
} cfgParam;

#define CFG_PERSISTANT        (1 << 0) // Don't free memory, things referance the pointer
#define CFG_GRP_ADVANCED      (1 << 1) // Show in group advanced
#define CFG_READONLY          (1 << 2) // Don't show in UI, but do write to CFG file. (Maybe display in UI but no edit)
#define CFG_HIDE              (1 << 3) // Don't show in any UI listing, don't write to CFG file.
#define CFG_PASSWD_MASK       (1 << 4) // Mask password with *****
#define CFG_FORCE_RESTART     (1 << 5) // Force aqualinkd to restart
#define CFG_ALLOW_BLANK       (1 << 6) // Allow blank entry
#define CFG_GREYED_OUT        (1 << 7) // Greyout in UI, show but not editable
#define CFG_MULTIPLE          (1 << 8) // This entry can have multiple string values, use linked list of strings

#define CFG_IS_ALLOCATED      (1 << 15)  // Largest bitmask, used internally to track if memory has been allocated for string types and needs to be freed when updated or on exit

//#define CFG_      (1 << 3)

// Text to show when CFG_PASSWD_MASK is set
#define PASSWD_MASK_TEXT "********"


#ifndef CONFIG_C
extern struct acdconfig _acdconfig_;
#else
//struct acdconfig _acdconfig_;
// Initialize config with safe defaults
struct acdconfig _acdconfig_ = {
    .log_level      = LOG_INFO,  // Start with INFO so we see boot messages
    .listen_address = "0.0.0.0:8080",
    .config_file    = "/etc/aquachemd.conf"
};
#endif


// Count entries in the config table at compile time
#define CFG_ENTRY(...) +1
// Need to use enum to create a scope for the CFG_PARAM_COUNT constant and avoid potential naming conflicts, and also to allow it to be used in array declarations
enum { 
    CFG_PARAM_COUNT = (0
#include "config_table.h"
    )
};
#undef CFG_ENTRY

#ifndef CONFIG_C
//extern cfgParam _cfgParams[CFG_PARAM_COUNT];
#else
cfgParam  _cfgParams[CFG_PARAM_COUNT];
#endif


/*

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

*/

#endif // CONFIG_H_
