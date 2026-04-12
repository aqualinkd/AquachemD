
#define _POSIX_C_SOURCE 200809L // for strdup
#include <string.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
 

#define CONFIG_C
#include "config.h"
#include "utils.h"
#include "aquachemd.h"

void set_config_defaults();

#define SET_VAL_CFG_STRING(field, def)  _acdconfig_.field = (char *)(def)
#define SET_VAL_CFG_INT(field, def)     _acdconfig_.field = (int)(def)
#define SET_VAL_CFG_BOOL(field, def)    _acdconfig_.field = (bool)(def)
#define SET_VAL_CFG_FLOAT(field, def)   _acdconfig_.field = (float)(def)
#define SET_VAL_CFG_HEX(field, def)     _acdconfig_.field = (unsigned char)(def)
#define SET_VAL_CFG_BITMASK(field, def) _acdconfig_.field = (uint16_t)(def)
#define SET_VAL_CFG_TXT_INT(field, def) _acdconfig_.field = (int)(def)
// Define the initialization macro to do nothing
#define SET_VAL_CFG_CUSTOM(field, def) /* Handled by specialized logic */


// Temporary storage for conditions being parsed from the file
/*
static struct {
  char *label; 
  char *mqtt_topic;
  char *mqtt_value;
  int gpio_pin;
  bool gpio_value;
  bool has_gpio_pin;
  int type;
  char *path;
} staging = {0};
*/

static struct {
    char *label;
    char *topic;
    int   pin;
    unsigned char address; // Added for EZO hex address
    acd_sensor_type_t pending_type; // Uses KEY_TYPE_PH, KEY_TYPE_ORP, etc.
    bool  has_pin;
} staging;

void add_condition_mqtt(const char *label, const char *topic, const char *value);
void add_condition_gpio(const char *label, int pin, bool value);

void add_sensor_ezo(const char *label, acd_sensor_type_t type, unsigned char address);
void add_sensor_d1w(const char *label, acd_sensor_type_t type, const char *value);
void add_sensor_mqtt(const char *label, acd_sensor_type_t type, const char *value);

void init_cfg_parameters()
{
  LOG(LOG_DEBUG, "Initializing config with %d entries", CFG_PARAM_COUNT);

  int i = 0;

#define CFG_ENTRY(cfg_name, cfg_field, cfg_def, cfg_type, cfg_mask, cfg_flag, cfg_meta) \
  if (i < CFG_PARAM_COUNT) {                                  \
        _cfgParams[i].name        = cfg_name;                   \
        _cfgParams[i].value_ptr   = &_acdconfig_.cfg_field;     \
        _cfgParams[i].value_type  = cfg_type;                   \
        _cfgParams[i].config_mask = cfg_mask;                   \
        _cfgParams[i].bit_flag    = cfg_flag;                   \
        _cfgParams[i].metadata    = cfg_meta;                   \
                                                                \
        /* Use cfg_type here to match the argument above */     \
        SET_VAL_ ## cfg_type(cfg_field, cfg_def);               \
        i++;                                                    \
  }

#undef CONFIG_TABLE_H_ 
#include "config_table.h"

#undef CFG_ENTRY
#undef SET_VAL_CFG_STRING
#undef SET_VAL_CFG_INT
#undef SET_VAL_CFG_BOOL
#undef SET_VAL_CFG_FLOAT
#undef SET_VAL_CFG_HEX
#undef SET_VAL_CFG_BITMASK
#undef SET_VAL_CFG_TXT_INT

    //LOG(LOG_DEBUG, "Finished Initializing config with %d entries", i);
}

/*
void check_and_commit_ezo() {
    if (staging.pending_type == KEY_TYPE_PH || 
        staging.pending_type == KEY_TYPE_ORP || 
        (staging.pending_type == KEY_TYPE_TEMP && !staging.topic && !staging.path)) {
        
        const char *type_str = "temp";
        if (staging.pending_type == KEY_TYPE_PH) type_str = "ph";
        else if (staging.pending_type == KEY_TYPE_ORP) type_str = "orp";

        // Pass the staging.address to the add function
        add_sensor_ezo(staging.label, type_str, staging.address);
        
        // Reset specific EZO fields
        if (staging.label) { free(staging.label); staging.label = NULL; }
        staging.address = 0; // Reset to default (usually 0 is fine for "none")
        staging.pending_type = KEY_TYPE_NONE;
    }
}
*/

void clear_staging() {
    if (staging.label) { free(staging.label); staging.label = NULL; }
    if (staging.topic) { free(staging.topic); staging.topic = NULL; }
    staging.pin = 0;
    staging.has_pin = false;
    staging.pending_type = KEY_TYPE_NONE;
}


bool setConfigValue(struct aquachemdata *acdata, char *param, char *value) {
  bool rtn = false;
  char *tmpval;

  value = cleanwhitespace(value);
 
  LOG(LOG_DEBUG, "Handling config '%s'\n", param);

  for (int i=0; i < CFG_PARAM_COUNT; i++) {
    if (strncasecmp(param, _cfgParams[i].name, (int)strlen(_cfgParams[i].name) ) == 0) {
      rtn=true;

      // Any special 
      if ( _cfgParams[i].metadata != NULL ) {
        //printf("Checking %s in %s\n",value,_cfgParams[i].valid_values);
        if ( strncasestr(_cfgParams[i].metadata, value, STR_FULL_LENGTH) == NULL) {
          LOG(LOG_ERR, "Config entry '%s',  %s is not valid\n",param, value);
          return false;
        }
      }

      if (strlen(value) <= 0) {
        //LOG(LOG_INFO,"Set configuration option `%s` to default since value is blank\n",_cfgParams[i].name );
        //set_cfg_parm_to_default(&_cfgParams[i]);
        LOG(LOG_INFO,"Set configuration option `%s` is blank, ignoring\n",_cfgParams[i].name );
        return true;
      }

      if (isMASKSET(_cfgParams[i].config_mask, CFG_PASSWD_MASK)) {
        if (strncmp(value, PASSWD_MASK_TEXT, strlen(PASSWD_MASK_TEXT)) == 0) {
          // Don't set password when it's the mask text
          return false;
        }
      }

      // Handle standard entries
      switch (_cfgParams[i].value_type) {
        case CFG_STRING:
          if (isMASKSET(_cfgParams[i].config_mask, CFG_IS_ALLOCATED)) {
            LOG(LOG_DEBUG,"FREE Memory for config %s %s\n",_cfgParams[i].name, *(char **)_cfgParams[i].value_ptr);
            free(*(char **)_cfgParams[i].value_ptr);
            *(char **)_cfgParams[i].value_ptr = NULL;
            removeMASK(_cfgParams[i].config_mask, CFG_IS_ALLOCATED);
          }
          *(char **)_cfgParams[i].value_ptr = cleanalloc(value, STR_FULL_LENGTH);
          setMASK(_cfgParams[i].config_mask, CFG_IS_ALLOCATED);
        break;
        case CFG_INT:
          *(int *)_cfgParams[i].value_ptr = strtoul(value, NULL, 10);
        break;
        case CFG_BOOL:
          *(bool *)_cfgParams[i].value_ptr = text2bool(value);
        break;
        case CFG_HEX:
          *(unsigned char *)_cfgParams[i].value_ptr = strtoul(value, NULL, 16); 
        break;
        case CFG_FLOAT:
          tmpval = cleanalloc(value, STR_FULL_LENGTH);
          *(float *)_cfgParams[i].value_ptr = atof(tmpval);
          free(tmpval);
        break;
        case CFG_BITMASK:
          if (text2bool(value))
            *(uint16_t *)_cfgParams[i].value_ptr |= _cfgParams[i].bit_flag;
          else
            *(uint16_t *)_cfgParams[i].value_ptr &= ~_cfgParams[i].bit_flag;
        break;
        case CFG_TXT_INT:
          if (_cfgParams[i].value_ptr == &_acdconfig_.log_level) {
            *(int *)_cfgParams[i].value_ptr = log_str_to_priority(value);
          } else {
            LOG(LOG_ERR, "ADD SPECIAL CONFIG FOR '%s'\n",param);
          }
        break;
        case CFG_CUSTOM:
    // --- 1. Collection Phase (Non-terminating) ---
    if (strstr(param, "_label")) {
        if (staging.label) free(staging.label);
        staging.label = strdup(value);
    }
    else if (strstr(param, "_type")) {
        if (strcasecmp(value, "ezo") == 0) {
            if (strstr(param, "ph_sensor")) staging.pending_type = KEY_TYPE_EZO_PH;
            else if (strstr(param, "orp_sensor")) staging.pending_type = KEY_TYPE_EZO_ORP;
            else if (strstr(param, "temp_sensor")) staging.pending_type = KEY_TYPE_EZO_TEMP;
            else {
                LOG(LOG_ERR, "Unknown sensor type for parameter '%s'\n", param);
                return false;
            }
        } 
        else if (strcasecmp(value, "d1w") == 0) staging.pending_type = KEY_TYPE_D1W_TEMP;
        else if (strcasecmp(value, "mqtt") == 0) staging.pending_type = KEY_TYPE_MQTT_TEMP;
    }
    else if (strncasecmp(param, "gpio_condition_pin", 18) == 0) {
        staging.pin = (int)strtoul(value, NULL, 10);
        staging.has_pin = true;
    }
    else if (strncasecmp(param, "mqtt_condition_topic", 20) == 0) {
        if (staging.topic) free(staging.topic);
        staging.topic = strdup(value);
    }

    // EZO Address is the last line for EZO
    else if (strstr(param, "_address")) {
        unsigned char addr = (unsigned char)strtoul(value, NULL, 16);
        //LOG(LOG_DEBUG, "Committing EZO: Label=%s, Type=%d, Addr=0x%02x", staging.label, staging.pending_type, addr);
        add_sensor_ezo(staging.label, staging.pending_type, addr);
        clear_staging(); 
    }
    // Sensor Path is the last line for 1-Wire
    else if (strncasecmp(param, "temp_sensor_path", 16) == 0) {
        add_sensor_d1w(staging.label, staging.pending_type, value);
        clear_staging();
    }
    // Sensor Topic is the last line for MQTT Sensors
    else if (strncasecmp(param, "temp_sensor_topic", 17) == 0) {
        add_sensor_mqtt(staging.label, staging.pending_type, value);
        clear_staging();
    }
    // Condition Value is the last line for all logic conditions
    else if (strstr(param, "_condition_value")) {
        if (staging.has_pin) {
            add_condition_gpio(staging.label, staging.pin, text2bool(value));
        } else if (staging.topic) {
            add_condition_mqtt(staging.label, staging.topic, value);
        }
        clear_staging();
    }
    break;

        }

      return rtn;
    }
  }
/*
  if (strlen(value) <= 0) {
    LOG(LOG_WARNING,"Configuration value is blank for option `%s`, Ignoring\n",param );
    return true;
  }
*/
  return rtn;
}


void parse_config_file(struct aquachemdata *acdata)
{
  FILE * fp ;
  char bufr[MAXCFGLINE];
  char *b_ptr;

  // If -v was passed on cmd line, we need to keep logging in debug and override any config value in default value 
  int log_level = _acdconfig_.log_level; // Save log level since it may will be reset in init_cfg_parameters.

  init_cfg_parameters();

  if (log_level > 0) {
    _acdconfig_.log_level = log_level; // Restore log level since it may have been reset to default above.
  }

  LOG(LOG_DEBUG, "Reading config file");

  if( (fp = fopen(_acdconfig_.config_file, "r")) != NULL){
    while(! feof(fp)){
      if (fgets(bufr, MAXCFGLINE, fp) != NULL)
      {
        b_ptr = &bufr[0];
        char *indx;
        // Eat leading whitespace
        while(isspace(*b_ptr)) b_ptr++;
        if ( b_ptr[0] != '\0' && b_ptr[0] != '#')
        {
          indx = strchr(b_ptr, '=');  
          if ( indx != NULL) 
          {
            if ( ! setConfigValue(acdata, b_ptr, indx+1)) {
              char *end = b_ptr + strlen(b_ptr) - 1;
              while(end > b_ptr && isspace(*end)) end--;
              LOG(LOG_ERR, "Unknown config parameter '%.*s'\n",end-b_ptr+1, b_ptr);
            } else {
              // restore debug from config values overiding it if -v was used on cmd line
              if (log_level > 0 && _acdconfig_.log_level != log_level) {
                _acdconfig_.log_level = log_level; // Restore log level since it may have been reset to default above.
              }
            }
          } 
        }
      }
    }
    fclose(fp);
  } else {
    /* error processing, couldn't open file */
    LOG(LOG_ERR, "Error reading config file '%s'\n",_acdconfig_.config_file);
    //errno = EBADF;
    //displayLastSystemError("Error reading config file");
    exit (EXIT_FAILURE);
  }

  acdata->conditions = _acdconfig_.conditions; // Make conditions available in main data struct for future use.
  acdata->keys->next = _acdconfig_.sensors; // Make sensors available in main data struct keys, first key is pre-set 
  check_print_config(acdata);
}



bool write_config_file (struct aquachemdata *acdata)
{ 
  int i;
  FILE *fp;
  char *lastName = NULL;
  //bool ro_root;
  //bool created_file;
  
  //LOG(LOG_ERR, "writeCfg() not implimented\n");

  //fp = fopen(_acdconfig_.config_file, "w");
  //fp = fopen("/tmp/aqualinkd.conf", "w");

  /*
  char backup_file[256];
  sprintf(backup_file,"%s.backup",_acdconfig_.config_file);
  if (copy_file(_acdconfig_.config_file, backup_file) != true) {
    LOG(LOG_WARNING,"Couldn't make a backup `%s` of config file `%s`\n",backup_file, _acdconfig_.config_file);
  } else {
    LOG(LOG_NOTICE,"Made backup of config %s\n",backup_file);
  }
  */

  //fp = aq_open_file(_acdconfig_.config_file, &ro_root, &created_file);

  fp = fopen(_acdconfig_.config_file, "w");

  if (fp == NULL) {
    LOG(LOG_ERR, "Open config file failed '%s'\n", _acdconfig_.config_file);
    //remount_root_ro(true);
    //fprintf(stdout, "Open file failed 'sprinkler.cron'\n");
    return false;
  }
  
  // Loop over config parameters.

  for ( i=0; i < CFG_PARAM_COUNT; i++) {
    if (isMASKSET(_cfgParams[i].config_mask, CFG_HIDE) ) {
      continue;
    }

    // Group values by fist letter, if the same group together.
    if (lastName != NULL && lastName[0] != _cfgParams[i].name[0]) {
      if ( lastName != NULL && strncasestr(lastName, "device_id", STR_FULL_LENGTH) != NULL && strncasestr(_cfgParams[i].name, "device_id", STR_FULL_LENGTH) != NULL ) {

      } else {
        fprintf(fp,"\n");
      }
    }

    switch (_cfgParams[i].value_type) {
      case CFG_STRING:
        if (*(char **)_cfgParams[i].value_ptr == NULL)
          fprintf(fp, "#%s=\n", _cfgParams[i].name);
        else
          fprintf(fp, "%s=%s\n", _cfgParams[i].name, *(char **)_cfgParams[i].value_ptr);
      break;
      case CFG_INT:
        if (*(int *)_cfgParams[i].value_ptr == UNKNOWN)
          fprintf(fp, "#%s=\n", _cfgParams[i].name);
        else
          fprintf(fp, "%s=%d\n", _cfgParams[i].name, *(int *)_cfgParams[i].value_ptr);
      break;
        case CFG_BOOL:
          fprintf(fp, "%s=%s\n", _cfgParams[i].name, bool2text(*(bool *)_cfgParams[i].value_ptr));
        break;
        case CFG_HEX:
          fprintf(fp, "%s=0x%02hhx\n", _cfgParams[i].name, *(unsigned char *)_cfgParams[i].value_ptr);
        break;
         case CFG_FLOAT:
          fprintf(fp, "%s=%f\n", _cfgParams[i].name, *(float *)_cfgParams[i].value_ptr);
        break;
        case CFG_BITMASK:
          fprintf(fp, "%s=%s\n", _cfgParams[i].name, (*(uint16_t *)_cfgParams[i].value_ptr & _cfgParams[i].bit_flag) == _cfgParams[i].bit_flag? bool2text(true):bool2text(false));
        break;
        case CFG_TXT_INT:
          if (_cfgParams[i].value_ptr == &_acdconfig_.log_level) {
            fprintf(fp, "%s=%s\n", _cfgParams[i].name, log_priority_to_str(_acdconfig_.log_level));
          } else {
            fprintf(fp, "%s=NEED TO ADD CODE TO HANDLE THIS\n",_cfgParams[i].name);
          }
        break;
        case CFG_CUSTOM:
          //LOG(LOG_WARNING, "write_config_file() ADD SPECIAL CONFIG FOR '%s'\n",_cfgParams[i].name);
        break;
    }
    lastName = _cfgParams[i].name;
  }

  for (acd_condition *curr = _acdconfig_.conditions; curr != NULL; curr = curr->next) {
    if (curr->type == COND_MQTT) {
      fprintf(fp, "mqtt_condition_label=%s\n", curr->label);
      fprintf(fp, "mqtt_condition_topic=%s\n", curr->mqtt_topic);
      fprintf(fp, "mqtt_condition_value=%s\n", curr->mqtt_value);
    } else {
      fprintf(fp, "gpio_condition_label=%s\n", curr->label);
      fprintf(fp, "gpio_condition_pin=%d\n", curr->gpio_pin);
      fprintf(fp, "gpio_condition_value=%s\n", bool2text(curr->gpio_value));
    }
  }

  /* --- Sensors Section --- */
  for (acd_key_t *curr = _acdconfig_.sensors; curr != NULL; curr = curr->next) {
    fprintf(fp, "\n"); // Add a newline between sensor blocks for readability

    switch (curr->type) {
        case KEY_TYPE_EZO_PH:
            if (curr->label) fprintf(fp, "ph_sensor_label=%s\n", curr->label);
            fprintf(fp, "ph_sensor_type=ezo\n");
            // address is a uchar, use hex_to_str or %02x directly
            fprintf(fp, "ph_sensor_address=0x%02x\n", curr->data.ezo.address);
            break;

        case KEY_TYPE_EZO_ORP:
            if (curr->label) fprintf(fp, "orp_sensor_label=%s\n", curr->label);
            fprintf(fp, "orp_sensor_type=ezo\n");
            fprintf(fp, "orp_sensor_address=0x%02x\n", curr->data.ezo.address);
            break;

        case KEY_TYPE_EZO_TEMP:
            if (curr->label) fprintf(fp, "temp_sensor_label=%s\n", curr->label);
            fprintf(fp, "temp_sensor_type=ezo\n");
            fprintf(fp, "temp_sensor_address=0x%02x\n", curr->data.ezo.address);
            break;

        case KEY_TYPE_D1W_TEMP:
            if (curr->label) fprintf(fp, "temp_sensor_label=%s\n", curr->label);
            fprintf(fp, "temp_sensor_type=d1w\n");
            fprintf(fp, "temp_sensor_path=%s\n", curr->data.w1.path);
            break;

        case KEY_TYPE_MQTT_TEMP:
            if (curr->label) fprintf(fp, "temp_sensor_label=%s\n", curr->label);
            fprintf(fp, "temp_sensor_type=mqtt\n");
            fprintf(fp, "temp_sensor_topic=%s\n", curr->data.mqtt.topic);
            break;

        default:
            // Handle GPIO or other types if necessary
            break;
    }
}

  fprintf(fp,"\n");

  //aq_close_file(fp, ro_root);
  fclose(fp);

  return true;
}

#define MAX_PRINTLEN 25

void check_print_config (struct aquachemdata *acdata)
{
  int i;
  char name[MAX_PRINTLEN];
  //uint32_t errors=0;

  // Should already be set, but just in case, make conditions available.
  acdata->conditions = _acdconfig_.conditions; // Make conditions available in main data struct for future use.

  acdata->keys->next = _acdconfig_.sensors; // Make sensors available in main data struct keys, first key is pre-set 

  // Anything that's not in the config table should be added as a special case here until it's added to the table. This is for handling things that need to be displayed in a special way or that aren't actually stored in the config struct but are still important to display.
  LOG(LOG_NOTICE, "%-*s = %s\n",MAX_PRINTLEN,"Configuration file", _acdconfig_.config_file);


  for ( i=0; i < CFG_PARAM_COUNT; i++) {

    // don't print mg_log_level if it's 0 since it's only used for debugging and would just add confusion to users looking at the config
    if (_cfgParams[i].value_ptr == &_acdconfig_.mg_log_level && *(int *)_cfgParams[i].value_ptr == 0) {
      continue;
    }

    strcsub(name, MAX_PRINTLEN, _cfgParams[i].name, '_', ' ');
    switch (_cfgParams[i].value_type) {
      case CFG_STRING:
        if (*(char **)_cfgParams[i].value_ptr == NULL)
          LOG(LOG_NOTICE, "%-*s =\n", MAX_PRINTLEN, name);
        else {
          if (isMASKSET(_cfgParams[i].config_mask ,CFG_PASSWD_MASK) )
            LOG(LOG_NOTICE, "%-*s = %s\n", MAX_PRINTLEN, name, PASSWD_MASK_TEXT);
          else
            LOG(LOG_NOTICE, "%-*s = %s\n", MAX_PRINTLEN, name, *(char **)_cfgParams[i].value_ptr);
        }
      break;
      case CFG_INT:
        if (*(int *)_cfgParams[i].value_ptr == UNKNOWN)
          LOG(LOG_NOTICE, "%-*s =\n", MAX_PRINTLEN, name);
        else
          LOG(LOG_NOTICE, "%-*s = %d\n", MAX_PRINTLEN, name, *(int *)_cfgParams[i].value_ptr);
      break;
      case CFG_BOOL:
        LOG(LOG_NOTICE, "%-*s = %s\n", MAX_PRINTLEN, name, bool2text(*(bool *)_cfgParams[i].value_ptr));
      break;
      case CFG_HEX:
        LOG(LOG_NOTICE, "%-*s = 0x%02hhx\n", MAX_PRINTLEN, name, *(unsigned char *)_cfgParams[i].value_ptr);
      break;
      case CFG_FLOAT:
        LOG(LOG_NOTICE, "%-*s = %f\n", MAX_PRINTLEN, name, *(float *)_cfgParams[i].value_ptr);
      break;
      case CFG_BITMASK:
        LOG(LOG_NOTICE, "%-*s = %s\n", MAX_PRINTLEN, name, (*(uint16_t *)_cfgParams[i].value_ptr & _cfgParams[i].bit_flag) == _cfgParams[i].bit_flag?bool2text(true):bool2text(false));
      break;
      case CFG_TXT_INT:
        if (_cfgParams[i].value_ptr == &_acdconfig_.log_level) {
          LOG(LOG_NOTICE, "%-*s = %s\n", MAX_PRINTLEN, name, log_priority_to_str(_acdconfig_.log_level));
        } else {
          LOG(LOG_NOTICE, "%-*s = NEED TO ADD CODE TO HANDLE THIS\n", MAX_PRINTLEN, name);
        }
     break;
      case CFG_CUSTOM:
        //LOG(LOG_WARNING, "check_print_config() ADD SPECIAL CONFIG FOR '%s'\n",_cfgParams[i].name);
      break;
    }
  }

  // Now print the dynamic conditions list
  for (acd_condition *curr = _acdconfig_.conditions; curr != NULL; curr = curr->next) {
    if (curr->type == COND_MQTT) {
        LOG(LOG_NOTICE, "%-*s = %-15s | %s -> %s\n", MAX_PRINTLEN, "condition (MQTT)", curr->label, curr->mqtt_topic, curr->mqtt_value);
    } else {
        LOG(LOG_NOTICE, "%-*s = %-15s | Pin %d -> %s\n", MAX_PRINTLEN, "condition (GPIO)", curr->label, curr->gpio_pin, bool2text(curr->gpio_value));
    }
  }

  for (acd_key_t *curr = _acdconfig_.sensors; curr != NULL; curr = curr->next) {
    const char *type_str = "UNKNOWN";
    switch (curr->type) {
      case KEY_TYPE_EZO_TEMP: type_str = "sensor (EZO Temp)"; break;
      case KEY_TYPE_EZO_PH: type_str = "sensor (EZO pH)"; break;
      case KEY_TYPE_EZO_ORP: type_str = "sensor (EZO ORP)"; break;
      case KEY_TYPE_D1W_TEMP: type_str = "sensor (1-Wire Temp)"; break;
      case KEY_TYPE_MQTT_TEMP: type_str = "sensor (MQTT Temp)"; break;
      //case KEY_TYPE_EZO_PUMP: type_str = "sensor (EZO Pump)"; break;
      //case KEY_TYPE_GPIO: type_str = "sensor (GPIO)"; break;
      default: type_str = "sensor (UNKNOWN)"; break;
    }
    LOG(LOG_NOTICE, "%-*s = %-15s | %s\n", MAX_PRINTLEN, type_str, curr->label, curr->ID);
  }
}


typedef enum {
  ACD_LABEL_MQTT,
  ACD_LABEL_GPIO,
  ACD_LABEL_EZO,
  ACD_LABEL_D1W
} acd_label_type_t;

const char* hex_to_str(unsigned char c) {
    static char buf[6]; // Enough for "0x??\0"
    snprintf(buf, sizeof(buf), "0x%02x", c);
    return buf;
}
const char* int_to_str(int i) {
    static char buf[12]; // Sufficient for -2147483648\0
    snprintf(buf, sizeof(buf), "%d", i);
    return buf;
}

/* Helper to generate ID and set Master status */

void generate_condition_id(acd_condition *node) {
   static int count = MASTER_ID;

   char buf[32];
   
   node->index = count++;
   snprintf(buf, sizeof(buf), "CS_%d", node->index);
   node->ID = strdup(buf);
}

/* Helper to generate ID and set Master status */
void generate_sensor_id(acd_key_t *node) {
    static int count_ph = MASTER_ID;
    static int count_orp = MASTER_ID;
    static int count_temp = MASTER_ID;

    char buf[32]; 
    const char *prefix = "";

    switch (node->type) {
        case KEY_TYPE_EZO_PH:
            prefix = "PH";
            node->index = count_ph++;
            break;
        case KEY_TYPE_EZO_ORP:
            prefix = "ORP";
            node->index = count_orp++;
            break;
        case KEY_TYPE_EZO_TEMP:
        case KEY_TYPE_MQTT_TEMP:
        case KEY_TYPE_D1W_TEMP:
            prefix = "TEMP";
            node->index = count_temp++;
            break;
        default:
            prefix = "UNK";
            node->index = 0;
            break;
    }

    snprintf(buf, sizeof(buf), "%s_%d", prefix, node->index);
    node->ID = strdup(buf);
}

char	*generate_label(const char *base, acd_label_type_t type, const char *label) {

  if (label && strlen(label) > 0) {
    return strdup(label);
  }

  char buf[128];
  switch (type) {
    case ACD_LABEL_MQTT:
    case ACD_LABEL_D1W:
    {
      const char *last_slash = strrchr(base, '/');
        if (last_slash != NULL) {
          snprintf(buf, sizeof(buf), "%s", last_slash + 1);
        } else {
          snprintf(buf, sizeof(buf), "%s", base);
        }  
      }
      break;
      case ACD_LABEL_GPIO:
        snprintf(buf, sizeof(buf), "GPIO_%s", base);
      break;
      case ACD_LABEL_EZO:
        snprintf(buf, sizeof(buf), "EZO_%s", base);
      break;
      default:
        snprintf(buf, sizeof(buf), "%s_UNKNOWN", base);
  }
  return strdup(buf);
}



// Helper to handle the linked list boilerplate
void append_to_conditions_list(acd_condition *new_node) {
    new_node->next = NULL;
    
    if (_acdconfig_.conditions == NULL) {
        _acdconfig_.conditions = new_node;
    } else {
        acd_condition *curr = _acdconfig_.conditions;
        while (curr->next != NULL) {
            curr = curr->next;
        }
        curr->next = new_node;
    }
}

void append_to_sensor_list(acd_key_t *new_node) {
    new_node->next = NULL;

    bool is_temp = (new_node->type == KEY_TYPE_EZO_TEMP || 
                    new_node->type == KEY_TYPE_MQTT_TEMP || 
                    new_node->type == KEY_TYPE_D1W_TEMP);

    if (_acdconfig_.sensors == NULL) {
        _acdconfig_.sensors = new_node;
        return;
    }

    if (is_temp) {
        // If head isn't temp, new_node becomes new head
        if (!(_acdconfig_.sensors->type == KEY_TYPE_EZO_TEMP || 
              _acdconfig_.sensors->type == KEY_TYPE_MQTT_TEMP || 
              _acdconfig_.sensors->type == KEY_TYPE_D1W_TEMP)) {
            new_node->next = _acdconfig_.sensors;
            _acdconfig_.sensors = new_node;
        } else {
            // Find the last temperature node and insert after it
            acd_key_t *curr = _acdconfig_.sensors;
            while (curr->next != NULL && 
                  (curr->next->type == KEY_TYPE_EZO_TEMP || 
                   curr->next->type == KEY_TYPE_MQTT_TEMP || 
                   curr->next->type == KEY_TYPE_D1W_TEMP)) {
                curr = curr->next;
            }
            new_node->next = curr->next;
            curr->next = new_node;
        }
    } else {
        // Standard append to the very end of the whole list
        acd_key_t *curr = _acdconfig_.sensors;
        while (curr->next != NULL) {
            curr = curr->next;
        }
        curr->next = new_node;
    }
}

// Specialized function for MQTT
void add_condition_mqtt(const char *label, const char *topic, const char *value) {
    acd_condition *new_node = malloc(sizeof(acd_condition));
    if (!new_node) return;

    new_node->type = COND_MQTT;
    
    new_node->label = generate_label(topic, ACD_LABEL_MQTT, label);
    new_node->mqtt_topic = strdup(topic);
    new_node->mqtt_value = strdup(value);
    new_node->met = false; // Initial state, not met.
    generate_condition_id(new_node);

    append_to_conditions_list(new_node);
}

// Specialized function for GPIO
void add_condition_gpio(const char *label, int pin, bool value) {
    acd_condition *new_node = malloc(sizeof(acd_condition));
    if (!new_node) return;

    new_node->type = COND_GPIO;

    new_node->label = generate_label(int_to_str(pin), ACD_LABEL_GPIO, label);
    new_node->gpio_pin = pin;
    new_node->gpio_value = value;
    new_node->met = false; // Initial state, not met.
    generate_condition_id(new_node);
    
    append_to_conditions_list(new_node);
}

void add_sensor_ezo(const char *label, acd_sensor_type_t type, unsigned char address) {
  acd_key_t *new_node = malloc(sizeof(acd_key_t));
  
  //LOG(LOG_DEBUG, "Committing EZO: Label=%s, Type=%d, Addr=0x%02x ---- %s", label, type, address, hex_to_str(address));

  new_node->type = type;
  new_node->label = generate_label(hex_to_str(address), ACD_LABEL_EZO, label);
  new_node->data.ezo.address = address;
  generate_sensor_id(new_node);

  append_to_sensor_list(new_node);
}

void add_sensor_mqtt(const char *label, acd_sensor_type_t type, const char *topic) {
  acd_key_t *new_node = malloc(sizeof(acd_key_t));
  
  new_node->type = type;
  new_node->label = generate_label(topic, ACD_LABEL_MQTT, label);
  new_node->data.mqtt.topic = strdup(topic);
  generate_sensor_id(new_node);

  append_to_sensor_list(new_node);
}

void add_sensor_d1w(const char *label, acd_sensor_type_t type, const char *path) {
  acd_key_t *new_node = malloc(sizeof(acd_key_t));
  
  new_node->type = type;
  new_node->label = generate_label(path, ACD_LABEL_D1W, label);
  strcpy(new_node->data.w1.path, path);
  generate_sensor_id(new_node);

  append_to_sensor_list(new_node);
}


