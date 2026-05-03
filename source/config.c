
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

static struct {
    char *label;
    char *topic_path;
    char *char_value;
    float value;
    float value2;
    int   pin;
    bool  pin_mode;
    bool  pin_state;
    unsigned char address; // Added for EZO hex address
    acd_type_t pending_type; // Uses ACD_TYPE_PH, ACD_TYPE_ORP, etc.
    //bool  has_pin;
} _staging;

void add_condition_mqtt(const char *label, const char *topic, const char *value);
void add_condition_gpio(const char *label, int pin, gpio_active_t pin_mode, gpio_req_t pin_state);

void add_sensor_gpio(const char *label, acd_type_t type, int pin, gpio_active_t pin_mode, gpio_req_t pin_state, int runtime);
void add_sensor_ezo(const char *label, acd_type_t type, unsigned char address);
void add_sensor_d1w(const char *label, acd_type_t type, const char *path, float offset, float scale);
void add_sensor_mqtt(const char *label, acd_type_t type, const char *topic);


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


void clear_staging() {
    if (_staging.label) { free(_staging.label); _staging.label = NULL; }
    if (_staging.topic_path) { free(_staging.topic_path); _staging.topic_path = NULL; }
    if (_staging.char_value) { free(_staging.char_value); _staging.char_value = NULL; }
    _staging.pin = 0;
    _staging.pin_mode = 0;
    _staging.pin_state = 0;
    _staging.value = 0;
    _staging.value2 = 0;
    _staging.pending_type = ACD_TYPE_MASTER; // Using Master as not set
}

void action_staging() {

  if (!_staging.label) {
    return;
  }

  switch ( _staging.pending_type) {
    case ACD_TYPE_EZO_PH:
    case ACD_TYPE_EZO_ORP:
    case ACD_TYPE_EZO_TEMP:
    case ACD_TYPE_EZO_PMP:
      add_sensor_ezo(_staging.label, _staging.pending_type, _staging.address);
      break;
    case ACD_TYPE_MQTT_TEMP:
      add_sensor_mqtt(_staging.label, _staging.pending_type, _staging.topic_path);
      break;
    case ACD_TYPE_D1W_TEMP:
      add_sensor_d1w(_staging.label, _staging.pending_type, _staging.topic_path, _staging.value, _staging.value2);
      break;
    case ACD_TYPE_GPIO_PMP:
      add_sensor_gpio(_staging.label, _staging.pending_type, _staging.pin, _staging.pin_mode, _staging.pin_state, (int)_staging.value);
      break;
    case ACD_TYPE_MQTT_COND:
      add_condition_mqtt(_staging.label, _staging.topic_path, _staging.char_value);
      break;
    case ACD_TYPE_GPIO_COND:
      add_condition_gpio(_staging.label, _staging.pin, _staging.pin_mode, _staging.pin_state);
      break;
    case ACD_TYPE_NONE: // No need anymore, fall through to default.
      /*
      if (_staging.topic_path != NULL) {
      // If we have a topic, it MUST be MQTT
        add_condition_mqtt(_staging.label, _staging.topic_path, _staging.char_value);
      } else if (_staging.pin > 0) {
        // If no topic but we have a pin, it's GPIO
        add_condition_gpio(_staging.label, _staging.pin, _staging.pin_mode, _staging.pin_state);
      }
      break;*/
    case ACD_TYPE_MASTER:
    default:
      LOG(LOG_ERR, "Did't create config entry for %s",_staging.label);
      break;
  }

  clear_staging();
}


bool setConfigValue(struct aquachemdata *acdata, char *param, char *value) {
  bool rtn = false;
  //char *tmpval;

  value = cleanwhitespace(value);
 
  //LOG(LOG_DEBUG, "Handling config '%s'\n", param);

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
            //LOG(LOG_DEBUG,"FREE Memory for config %s %s\n",_cfgParams[i].name, *(char **)_cfgParams[i].value_ptr);
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
          *(float *)_cfgParams[i].value_ptr =  strtof(value, NULL);
          /*
          tmpval = cleanalloc(value, STR_FULL_LENGTH);
          *(float *)_cfgParams[i].value_ptr = atof(tmpval);
          free(tmpval);
          */
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
          if (strstr(param, "_label") != NULL) {
            action_staging();
            clear_staging();
            _staging.label = strdup(value);
          }
          else if (strstr(param, "_type")) {
            if (strcasecmp(value, "ezo") == 0) {
              if (strstr(param, "ph_sensor")) _staging.pending_type = ACD_TYPE_EZO_PH;
              else if (strstr(param, "orp_sensor")) _staging.pending_type = ACD_TYPE_EZO_ORP;
              else if (strstr(param, "temp_sensor")) _staging.pending_type = ACD_TYPE_EZO_TEMP;
              else if (strstr(param, "doser")) _staging.pending_type = ACD_TYPE_EZO_PMP;
              else {
                LOG(LOG_ERR, "Unknown sensor type for parameter '%s'\n", param);
                return false;
              }
            } 
            else if (strcasecmp(value, "d1w") == 0) _staging.pending_type = ACD_TYPE_D1W_TEMP;
            else if (strcasecmp(value, "mqtt") == 0)_staging.pending_type = ACD_TYPE_MQTT_TEMP;
            else if (strcasecmp(value, "gpio") == 0){
              if (strstr(param, "doser")) {
                _staging.pending_type = ACD_TYPE_GPIO_PMP;
              } else {
                LOG(LOG_ERR, "Unknown sensor type for parameter '%s'\n", param);
                return false;
              }
            }
          }
          else if (strncasecmp(param, "mqtt_condition_topic", 20) == 0) {
            _staging.pending_type = ACD_TYPE_MQTT_COND;
            if (_staging.topic_path) free(_staging.topic_path);
            _staging.topic_path = strdup(value);
          }
          // EZO Address is the last line for EZO
          else if (strstr(param, "_address")) {
            _staging.address = (unsigned char)strtoul(value, NULL, 16);
          }
          // Sensor Path is the last line for 1-Wire
          else if (strncasecmp(param, "temp_sensor_path", 16) == 0) {
            if (_staging.topic_path) free(_staging.topic_path);
            _staging.topic_path = strdup(value);
          }
          // Sensor Topic is the last line for MQTT Sensors
          else if (strncasecmp(param, "temp_sensor_topic", 17) == 0) {
            if (_staging.topic_path) free(_staging.topic_path);
            _staging.topic_path = strdup(value);
          }
          else if (strncasecmp(param, "temp_sensor_offset", 18) == 0) {
            _staging.value = strtof(value, NULL);
          }
          else if (strncasecmp(param, "temp_sensor_scale", 17) == 0) {
            _staging.value2 = strtof(value, NULL);
          }
          else if (strncasecmp(param, "gpio_condition_pin_mode", 23) == 0) {
            _staging.pin_mode = text2gpioactive(value);
            _staging.pending_type = ACD_TYPE_GPIO_COND;
          }
          else if (strncasecmp(param, "gpio_condition_required_state", 29) == 0) {
            _staging.pin_state = text2bool(value);
            _staging.pending_type = ACD_TYPE_GPIO_COND;
          }
          else if (strncasecmp(param, "doser_pin_mode", 14) == 0) {
            _staging.pin_mode = text2gpioactive(value);
          }
          else if (strncasecmp(param, "doser_runtime", 13) == 0) {
            _staging.value = (int)strtoul(value, NULL, 10);
          }
          else if (strncasecmp(param, "doser_required_state", 20) == 0) {
            _staging.pin_state = text2bool(value);
          }
          else if (strncasecmp(param, "gpio_condition_pin", 18) == 0 || strncasecmp(param, "doser_pin", 9) == 0) {
            _staging.pin = (int)strtoul(value, NULL, 10);
            if (strncasecmp(param, "gpio_condition", 14) == 0) {
              _staging.pending_type = ACD_TYPE_GPIO_COND;
            }
          }
          else if (strncasecmp(param, "mqtt_condition_value", 20) == 0) {
            if (_staging.char_value) free(_staging.char_value);
            _staging.char_value = strdup(value);
            _staging.pending_type = ACD_TYPE_MQTT_COND; 
          }
          
        break;
      } // Switch to Handle standard entries
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
  bool log_debug = (_acdconfig_.log_level >= LOG_DEBUG);

  init_cfg_parameters();

  if (log_debug) {
    _acdconfig_.log_level = LOG_DEBUG; // Restore log level since it may have been reset to default above.
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
              if (log_debug) {
                _acdconfig_.log_level = LOG_DEBUG; // Restore log level since it may have been reset to default above.
              }
            }
          } 
        }
      }
    }
    action_staging(); // Add any last dangleing sensors or conditions.
    fclose(fp);
  } else {
    /* error processing, couldn't open file */
    LOG(LOG_ERR, "Error reading config file '%s'\n",_acdconfig_.config_file);
    //errno = EBADF;
    //displayLastSystemError("Error reading config file");
    exit (EXIT_FAILURE);
  }

  //acdata->conditions = _acdconfig_.conditions; // Make conditions available in main data struct for future use.
  acdata->keys->next = _acdconfig_.keys; // Make sensors available in main data struct keys, first key is pre-set 
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
/*
  for (acd_condition_t *curr = _acdconfig_.conditions; curr != NULL; curr = curr->next) {
    if (curr->type == COND_MQTT) {
      fprintf(fp, "mqtt_condition_label=%s\n", curr->label);
      fprintf(fp, "mqtt_condition_topic=%s\n", curr->data.mqtt.topic);
      fprintf(fp, "mqtt_condition_value=%s\n", curr->data.mqtt.target_value);
    } else {
      fprintf(fp, "gpio_condition_label=%s\n", curr->label);
      fprintf(fp, "gpio_condition_pin=%d\n", curr->data.gpio.pin);
      fprintf(fp, "gpio_condition_value=%d\n", curr->data.gpio.active);
    }
  }
*/
  /* --- Sensors Section --- */
  for (acd_key_t *curr = _acdconfig_.keys; curr != NULL; curr = curr->next) {
    fprintf(fp, "\n"); // Add a newline between sensor blocks for readability

    switch (curr->type) {
        case ACD_TYPE_MQTT_COND:
          fprintf(fp, "mqtt_condition_label=%s\n", curr->label);
          fprintf(fp, "mqtt_condition_topic=%s\n", curr->data.mqtt.topic);
          fprintf(fp, "mqtt_condition_value=%s\n", curr->data.mqtt.target_value);
          break;
        
        case ACD_TYPE_GPIO_COND:
          fprintf(fp, "gpio_condition_label=%s\n", curr->label);
          fprintf(fp, "gpio_condition_pin=%d\n", curr->data.gpio.pin);
          fprintf(fp, "gpio_condition_value=%d\n", curr->data.gpio.active);
          break;

        case ACD_TYPE_EZO_PH:
            if (curr->label) fprintf(fp, "ph_sensor_label=%s\n", curr->label);
            fprintf(fp, "ph_sensor_type=ezo\n");
            // address is a uchar, use hex_to_str or %02x directly
            fprintf(fp, "ph_sensor_address=0x%02x\n", curr->data.ezo.address);
            break;

        case ACD_TYPE_EZO_ORP:
            if (curr->label) fprintf(fp, "orp_sensor_label=%s\n", curr->label);
            fprintf(fp, "orp_sensor_type=ezo\n");
            fprintf(fp, "orp_sensor_address=0x%02x\n", curr->data.ezo.address);
            break;

        case ACD_TYPE_EZO_TEMP:
            if (curr->label) fprintf(fp, "temp_sensor_label=%s\n", curr->label);
            fprintf(fp, "temp_sensor_type=ezo\n");
            fprintf(fp, "temp_sensor_address=0x%02x\n", curr->data.ezo.address);
            break;

        case ACD_TYPE_D1W_TEMP:
            if (curr->label) fprintf(fp, "temp_sensor_label=%s\n", curr->label);
            fprintf(fp, "temp_sensor_type=d1w\n");
            fprintf(fp, "temp_sensor_path=%s\n", curr->data.w1.path);
            break;

        case ACD_TYPE_MQTT_TEMP:
            if (curr->label) fprintf(fp, "temp_sensor_label=%s\n", curr->label);
            fprintf(fp, "temp_sensor_type=mqtt\n");
            fprintf(fp, "temp_sensor_topic=%s\n", curr->data.mqtt.topic);
            break;

        case ACD_TYPE_GPIO_PMP:
            if (curr->label) fprintf(fp, "doser_label=%s\n", curr->label);
            fprintf(fp, "doser_type=gpio\n");
            fprintf(fp, "doser_pin=%d\n", curr->data.gpio.pin);
            fprintf(fp, "doser_value=%d\n", curr->data.gpio.active);
            break;

        case ACD_TYPE_EZO_PMP:
            if (curr->label) fprintf(fp, "doser_label=%s\n", curr->label);
            fprintf(fp, "doser_type=ezo\n");
            fprintf(fp, "doser_address=0x%02x\n", curr->data.ezo.address);
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
  //acdata->conditions = _acdconfig_.conditions; // Make conditions available in main data struct for future use.

  acdata->keys->next = _acdconfig_.keys; // Make sensors available in main data struct keys, first key is pre-set 

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
/*
  // Now print the dynamic conditions list
  for (acd_condition_t *curr = _acdconfig_.conditions; curr != NULL; curr = curr->next) {
    if (curr->type == COND_MQTT) {
        LOG(LOG_NOTICE, "%-*s = %-15s | %s -> %s\n", MAX_PRINTLEN, "condition (MQTT)", curr->label, curr->data.mqtt.topic, curr->data.mqtt.target_value);
    } else {
        LOG(LOG_NOTICE, "%-*s = %-15s | Pin %d -> %d\n", MAX_PRINTLEN, "condition (GPIO)", curr->label, curr->data.gpio.pin, curr->data.gpio.active);
    }
  }
*/
  for (acd_key_t *curr = _acdconfig_.keys; curr != NULL; curr = curr->next) {
    const char *type_str = "UNKNOWN";
    switch (curr->type) {
      case ACD_TYPE_MQTT_COND: type_str = "condition (MQTT)"; break;
      case ACD_TYPE_GPIO_COND: type_str = "condition (GPIO)"; break;
      case ACD_TYPE_EZO_TEMP: type_str = "sensor (EZO Temp)"; break;
      case ACD_TYPE_EZO_PH: type_str = "sensor (EZO pH)"; break;
      case ACD_TYPE_EZO_ORP: type_str = "sensor (EZO ORP)"; break;
      case ACD_TYPE_D1W_TEMP: type_str = "sensor (1-Wire Temp)"; break;
      case ACD_TYPE_MQTT_TEMP: type_str = "sensor (MQTT Temp)"; break;
      case ACD_TYPE_GPIO_PMP: type_str = "pump (GPIO)"; break;
      case ACD_TYPE_EZO_PMP: type_str = "pump (EZO PMP)"; break;
      //case ACD_TYPE_EZO_PUMP: type_str = "sensor (EZO Pump)"; break;
      //case ACD_TYPE_GPIO: type_str = "sensor (GPIO)"; break;
      default: type_str = "sensor (UNKNOWN)"; break;
    }
    LOG(LOG_NOTICE, "%-*s = %-8.8s| %s\n", MAX_PRINTLEN, type_str, curr->ID, curr->label );
  }
}


typedef enum {
  ACD_LABEL_MQTT,
  ACD_LABEL_GPIO,
  ACD_LABEL_EZO,
  ACD_LABEL_D1W,
  ACD_LABEL_PMP  // Doser
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

//void generate_condition_id(acd_condition_t *node) {
void generate_condition_id(acd_key_t *node) {
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
    static int count_pmp = MASTER_ID;

    char buf[32]; 
    const char *prefix = "";

    switch (node->type) {
        case ACD_TYPE_EZO_PH:
            prefix = "PH";
            node->index = count_ph++;
            break;
        case ACD_TYPE_EZO_ORP:
            prefix = "ORP";
            node->index = count_orp++;
            break;
        case ACD_TYPE_EZO_TEMP:
        case ACD_TYPE_MQTT_TEMP:
        case ACD_TYPE_D1W_TEMP:
            prefix = "TEMP";
            node->index = count_temp++;
            break;
        case ACD_TYPE_GPIO_PMP:
        case ACD_TYPE_EZO_PMP:
            prefix = "PMP";
            node->index = count_pmp++;
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
    case ACD_LABEL_PMP:
      snprintf(buf, sizeof(buf), "PMP_%s", base);
      break;
    default:
      snprintf(buf, sizeof(buf), "%s_UNKNOWN", base);
      break;
  }
  return strdup(buf);
}




// Priority groups — lower number = closer to head
static int node_priority(acd_type_t type) {
    if (type == ACD_TYPE_MASTER)                return 0;
    if (IS_CONDITION(type))                     return 1;
    if (type == ACD_TYPE_EZO_TEMP  ||
        type == ACD_TYPE_MQTT_TEMP ||
        type == ACD_TYPE_D1W_TEMP)              return 2;
    if (IS_INPUT(type))                         return 3;  // PH, ORP, etc.
    if (IS_OUTPUT(type))                        return 4;
    return 5;                                              // unknown/safety
}

void append_to_key_list(acd_key_t *new_node) {
    new_node->next = NULL;
    int new_prio = node_priority(new_node->type);

    // Insert at head if list is empty or new node beats the head
    if (_acdconfig_.keys == NULL ||
        node_priority(_acdconfig_.keys->type) > new_prio) {
        new_node->next = _acdconfig_.keys;
        _acdconfig_.keys = new_node;
        return;
    }

    // Walk until the next node has a strictly higher priority
    acd_key_t *curr = _acdconfig_.keys;
    while (curr->next != NULL && node_priority(curr->next->type) <= new_prio) {
        curr = curr->next;
    }
    new_node->next = curr->next;
    curr->next = new_node;
}

// Specialized function for MQTT
void add_condition_mqtt(const char *label, const char *topic, const char *value) {
    acd_key_t *new_node = malloc(sizeof(acd_key_t));
    if (!new_node) return;

    new_node->type = ACD_TYPE_MQTT_COND;
    
    new_node->label = generate_label(topic, ACD_LABEL_MQTT, label);
    new_node->data.mqtt.topic = strdup(topic);
    new_node->data.mqtt.target_value = strdup(value);
    new_node->met = false; // Initial state, not met.
    generate_condition_id(new_node);

    //new_node->target_value = UNKNOWN;

    append_to_key_list(new_node);
}

// Specialized function for GPIO
void add_condition_gpio(const char *label, int pin, gpio_active_t pin_mode, gpio_req_t pin_state) {
    acd_key_t *new_node = malloc(sizeof(acd_key_t));
    if (!new_node) return;

    new_node->type = ACD_TYPE_GPIO_COND;

    new_node->label = generate_label(int_to_str(pin), ACD_LABEL_GPIO, label);
    new_node->data.gpio.pin = pin;
    new_node->data.gpio.active = pin_mode;
    new_node->data.gpio.required = pin_state;
    new_node->met = false; // Initial state, not met.
    generate_condition_id(new_node);
    
    append_to_key_list(new_node);

    //LOG(LOG_ERR, "GPIO %s Pin %d, active %d, state %d",new_node->label, new_node->data.gpio.pin, new_node->data.gpio.active, pin_state);
}

void add_sensor_ezo(const char *label, acd_type_t type, unsigned char address) {
  acd_key_t *new_node = malloc(sizeof(acd_key_t));
  
  //LOG(LOG_DEBUG, "Committing EZO: Label=%s, Type=%d, Addr=0x%02x ---- %s", label, type, address, hex_to_str(address));

  new_node->type = type;
  new_node->label = generate_label(hex_to_str(address), ACD_LABEL_EZO, label);
  new_node->data.ezo.address = address;
  generate_sensor_id(new_node);

  append_to_key_list(new_node);
}

void add_sensor_mqtt(const char *label, acd_type_t type, const char *topic) {
  acd_key_t *new_node = malloc(sizeof(acd_key_t));
  
  new_node->type = type;
  new_node->label = generate_label(topic, ACD_LABEL_MQTT, label);
  new_node->data.mqtt.topic = strdup(topic);
  generate_sensor_id(new_node);

  append_to_key_list(new_node);
}

void add_sensor_d1w(const char *label, acd_type_t type, const char *path, float offset, float scale) {
  acd_key_t *new_node = malloc(sizeof(acd_key_t));
  
  new_node->type = type;
  new_node->label = generate_label(path, ACD_LABEL_D1W, label);
  strcpy(new_node->data.w1.path, path);
  new_node->data.w1.offset = offset;
  new_node->data.w1.scale = scale;
  generate_sensor_id(new_node);

  append_to_key_list(new_node);
}

void add_sensor_gpio(const char *label, acd_type_t type, int pin, gpio_active_t pin_mode, gpio_req_t pin_state, int runtime) {
  acd_key_t *new_node = malloc(sizeof(acd_key_t));
  
  new_node->type = type;
  new_node->label = generate_label(int_to_str(pin), ACD_LABEL_PMP, label);
  new_node->data.gpio.pin = pin;
  new_node->data.gpio.active = pin_mode;
  new_node->data.gpio.required = pin_state;

  if (runtime <= 0) {
    new_node->runtime = _acdconfig_.default_dose_time;
  } else {
    new_node->runtime = runtime;
  }

  generate_sensor_id(new_node);

  append_to_key_list(new_node);

  //LOG(LOG_ERR, "GPIO %s Pin %d, active %d, state %d",new_node->label, new_node->data.gpio.pin, new_node->data.gpio.active, pin_state);
}


