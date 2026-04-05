

#include <string.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_C
#include "config.h"
#include "utils.h"
#include "aquachemd.h"

void set_config_defaults();


const bool          _dcfg_false            = false;
const bool          _dcfg_true             = true;
const int           _dcfg_loglevel         = LOG_NOTICE;
const char         *_dcfg_web_port         = "http://0.0.0.0:80";
const char         *_dcfg_web_root         = "/var/www/aquachemd";
const char         *mqtt_aquachemd_topic   = "aquachemd";
const int           _dcfg_zero             = 0;
const int           _dcfg_sensor_poll_time = 60;


void init_cfg_parameters()
{
  _numCfgParams = 0;

  _cfgParams[_numCfgParams].value_ptr = &_acdconfig_.listen_address;
  _cfgParams[_numCfgParams].value_type = CFG_STRING;
  _cfgParams[_numCfgParams].name = CFG_N_listen_address;
  _cfgParams[_numCfgParams].config_mask |= CFG_GRP_ADVANCED;
  _cfgParams[_numCfgParams].config_mask |= CFG_FORCE_RESTART;
  _cfgParams[_numCfgParams].default_value = (void *)_dcfg_web_port;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_acdconfig_.web_directory;
  _cfgParams[_numCfgParams].value_type = CFG_STRING;
  _cfgParams[_numCfgParams].name = CFG_N_web_directory;
  _cfgParams[_numCfgParams].config_mask |= CFG_GRP_ADVANCED;
  _cfgParams[_numCfgParams].config_mask |= CFG_READONLY;
  _cfgParams[_numCfgParams].config_mask |= CFG_FORCE_RESTART;
  _cfgParams[_numCfgParams].default_value = (void *)_dcfg_web_root;


#if MG_TLS > 0
  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_acdconfig_.cert_dir;
  _cfgParams[_numCfgParams].value_type = CFG_STRING;
  _cfgParams[_numCfgParams].name = CFG_N_cert_dir;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_null;
  _cfgParams[_numCfgParams].config_mask |= CFG_ALLOW_BLANK;
  _cfgParams[_numCfgParams].config_mask |= CFG_GRP_ADVANCED;
  _cfgParams[_numCfgParams].config_mask |= CFG_FORCE_RESTART;
#endif 

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_acdconfig_.log_level;
  _cfgParams[_numCfgParams].value_type = CFG_SPECIAL;
  _cfgParams[_numCfgParams].name = CFG_N_log_level;
  _cfgParams[_numCfgParams].valid_values = CFG_V_log_level;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_loglevel;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_acdconfig_.mg_log_level;
  _cfgParams[_numCfgParams].value_type = CFG_INT;
  _cfgParams[_numCfgParams].name = CFG_N_MG_log_level;
  _cfgParams[_numCfgParams].config_mask |= CFG_READONLY;
  _cfgParams[_numCfgParams].config_mask |= CFG_HIDE;
  _cfgParams[_numCfgParams].default_value = (void *) &_dcfg_zero;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_acdconfig_.mqtt_server;
  _cfgParams[_numCfgParams].value_type = CFG_STRING;
  _cfgParams[_numCfgParams].name = CFG_N_mqtt_server;
  _cfgParams[_numCfgParams].config_mask |= CFG_FORCE_RESTART;
  _cfgParams[_numCfgParams].default_value = NULL;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_acdconfig_.mqtt_user;
  _cfgParams[_numCfgParams].value_type = CFG_STRING;
  _cfgParams[_numCfgParams].name = CFG_N_mqtt_user;
  _cfgParams[_numCfgParams].config_mask |= CFG_FORCE_RESTART;
  _cfgParams[_numCfgParams].default_value = NULL;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_acdconfig_.mqtt_passwd;
  _cfgParams[_numCfgParams].value_type = CFG_STRING;
  _cfgParams[_numCfgParams].name = CFG_N_mqtt_passwd;
  _cfgParams[_numCfgParams].config_mask |= CFG_FORCE_RESTART;
  _cfgParams[_numCfgParams].config_mask |= CFG_PASSWD_MASK;
  _cfgParams[_numCfgParams].default_value = NULL;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_acdconfig_.mqtt_aquachemd_topic;
  _cfgParams[_numCfgParams].value_type = CFG_STRING;
  _cfgParams[_numCfgParams].name = CFG_N_mqtt_aquachemd_topic;
  _cfgParams[_numCfgParams].config_mask |= CFG_FORCE_RESTART;
  _cfgParams[_numCfgParams].default_value = (void *)&mqtt_aquachemd_topic;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_acdconfig_.mqtt_aqualinkd_topic;
  _cfgParams[_numCfgParams].value_type = CFG_STRING;
  _cfgParams[_numCfgParams].name = CFG_N_mqtt_aqualinkd_topic;
  _cfgParams[_numCfgParams].config_mask |= CFG_FORCE_RESTART;
  _cfgParams[_numCfgParams].default_value = NULL;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_acdconfig_.mqtt_discovery_topic;
  _cfgParams[_numCfgParams].value_type = CFG_STRING;
  _cfgParams[_numCfgParams].name = CFG_N_mqtt_discovery_topic;
  _cfgParams[_numCfgParams].config_mask |= CFG_FORCE_RESTART;
  _cfgParams[_numCfgParams].default_value = NULL;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_acdconfig_.mqtt_discovery_use_mac;
  _cfgParams[_numCfgParams].value_type = CFG_BOOL;
  _cfgParams[_numCfgParams].name = CFG_N_mqtt_discovery_use_mac;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_true;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_acdconfig_.convert_mqtt_temp;
  _cfgParams[_numCfgParams].value_type = CFG_BOOL;
  _cfgParams[_numCfgParams].name = CFG_N_convert_mqtt_temp;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_false;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_acdconfig_.mqtt_timed_update;
  _cfgParams[_numCfgParams].value_type = CFG_BOOL;
  _cfgParams[_numCfgParams].name = CFG_N_mqtt_timed_update;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_true;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_acdconfig_.sensor_poll_time;
  _cfgParams[_numCfgParams].value_type = CFG_INT;
  _cfgParams[_numCfgParams].name = CFG_N_sensor_poll_time;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_sensor_poll_time;

  set_config_defaults();
}

void set_cfg_parm_to_default(cfgParam *parm)
{
  switch (parm->value_type)
  {
  case CFG_STRING:
    *(char **)parm->value_ptr = (char *)parm->default_value;
    break;
  case CFG_INT:
    *(int *)parm->value_ptr = *(int *)parm->default_value;
    break;
  case CFG_BOOL:
    *(bool *)parm->value_ptr = *(bool *)parm->default_value;
    break;
  case CFG_HEX:
    *(unsigned char *)parm->value_ptr = *(unsigned char *)parm->default_value;
    break;
  case CFG_FLOAT:
    *(float *)parm->value_ptr = *(float *)parm->default_value;
    break;
  case CFG_BITMASK:
    if (*(bool *)parm->default_value == true)
    {
      *(uint16_t *)parm->value_ptr |= parm->mask;
    }
    else
    {
      *(uint16_t *)parm->value_ptr &= ~parm->mask;
    }
    break;
  case CFG_SPECIAL:
    if (strncasecmp(parm->name, CFG_N_log_level, strlen(CFG_N_log_level)) == 0)
    {
      *(int *)parm->value_ptr = *(int *)parm->default_value;
    }
    else
    {
      LOG(LOG_ERR, "ADD CONFIG DEFAULT FOR %s\n", parm->name);
    }
    break;
  }
}

void set_config_defaults()
{
  for (int i=0; i <= _numCfgParams; i++) { 
    set_cfg_parm_to_default(&_cfgParams[i]);
  }
}



bool setConfigValue(struct aquachemdata *acdata, char *param, char *value) {
  bool rtn = false;
  char *tmpval;

  value = cleanwhitespace(value);
 
  for (int i=0; i <= _numCfgParams; i++) {
    if (strncasecmp(param, _cfgParams[i].name, (int)strlen(_cfgParams[i].name) ) == 0) {
      rtn=true;

      // Any special 
      if ( _cfgParams[i].valid_values != NULL ) {
        //printf("Checking %s in %s\n",value,_cfgParams[i].valid_values);
        if ( strncasestr(_cfgParams[i].valid_values, value, STR_FULL_LENGTH) == NULL) {
          LOG(LOG_ERR, "Config entry '%s',  %s is not valid\n",param, value);
          return false;
        }
      }

      if (strlen(value) <= 0) {
        LOG(LOG_INFO,"Set configuration option `%s` to default since value is blank\n",_cfgParams[i].name );
        set_cfg_parm_to_default(&_cfgParams[i]);
        return true;
      }

      if (isMASKSET(_cfgParams[i].config_mask, CFG_PASSWD_MASK)) {
        if (strncmp(value, PASSWD_MASK_TEXT, strlen(PASSWD_MASK_TEXT)) == 0) {
          // Don't set password when it's the mask text
          return false;
        }
      }

      switch (_cfgParams[i].value_type) {
        case CFG_STRING:
          if (_cfgParams[i].value_ptr != NULL && *(char **)_cfgParams[i].value_ptr != _cfgParams[i].default_value) {
            LOG(LOG_DEBUG,"FREE Memory for config %s %s\n",_cfgParams[i].name, *(char **)_cfgParams[i].value_ptr);
            free(*(char **)_cfgParams[i].value_ptr);
            *(char **)_cfgParams[i].value_ptr = NULL;
          }
          *(char **)_cfgParams[i].value_ptr = cleanalloc(value, STR_FULL_LENGTH);
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
            *(uint16_t *)_cfgParams[i].value_ptr |= _cfgParams[i].mask;
          else
            *(uint16_t *)_cfgParams[i].value_ptr &= ~_cfgParams[i].mask;
        break;
        case CFG_SPECIAL:
          if (strncasecmp(param, CFG_N_log_level, strlen(CFG_N_log_level)) == 0) {
            *(int *)_cfgParams[i].value_ptr = log_str_to_priority(value);
          //} else if (strncasecmp(param, CFG_N_panel_type, strlen(CFG_N_panel_type)) == 0) {
          //  setPanelByName(acdata, value); 
          //} else {
            LOG(LOG_ERR, "ADD SPECIAL CONFIG FOR '%s'\n",param);
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

  LOG(LOG_DEBUG, "reading config file");

  init_cfg_parameters();

  //_acdconfig_.config_file = cleanalloc(cfgFile, STR_FULL_LENGTH);
  /*
  if (!(_acdconfig_.config_file = cleanalloc(cfgFile, STR_FULL_LENGTH))) {
    LOG(LOG_ERR, "Error reading config file");
    return;
  }
  */
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

  for ( i=0; i <= _numCfgParams; i++) {
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
          fprintf(fp, "%s=%s\n", _cfgParams[i].name, (*(uint16_t *)_cfgParams[i].value_ptr & _cfgParams[i].mask) == _cfgParams[i].mask? bool2text(true):bool2text(false));
        break;
        case CFG_SPECIAL:
          if (strncasecmp(_cfgParams[i].name, CFG_N_log_level, strlen(CFG_N_log_level)) == 0) {
            fprintf(fp, "%s=%s\n", _cfgParams[i].name, log_priority_to_str(_acdconfig_.log_level));
          //} else if (strncasecmp(_cfgParams[i].name, CFG_N_panel_type, strlen(CFG_N_panel_type)) == 0) {
          //  fprintf(fp, "%s=%s\n", _cfgParams[i].name, getPanelString());
          } else {
            fprintf(fp, "%s=NEED TO ADD CODE TO HANDLE THIS\n",_cfgParams[i].name);
          }
        break;
    }
    lastName = _cfgParams[i].name;
  }

  fprintf(fp,"\n");

  //aq_close_file(fp, ro_root);
  fclose(fp);

  return true;
}

#define MAX_PRINTLEN 35

void check_print_config (struct aquachemdata *acdata)
{
  int i;
  char name[MAX_PRINTLEN];
  //uint32_t errors=0;

  for ( i=0; i <= _numCfgParams; i++) {

    // don't print mg_log_level if it's 0 since it's only used for debugging and would just add confusion to users looking at the config
    if (_cfgParams[i].value_ptr == &_acdconfig_.mg_log_level && *(int *)_cfgParams[i].value_ptr == 0) {
      continue;
    }

    strcsub(name, MAX_PRINTLEN, _cfgParams[i].name, '_', ' ');
    switch (_cfgParams[i].value_type) {
      case CFG_STRING:
        if (*(char **)_cfgParams[i].value_ptr == NULL)
          LOG(LOG_NOTICE, "%-35s =\n", name);
        else {
          if (isMASKSET(_cfgParams[i].config_mask ,CFG_PASSWD_MASK) )
            LOG(LOG_NOTICE, "%-35s = %s\n",name, PASSWD_MASK_TEXT);
          else
            LOG(LOG_NOTICE, "%-35s = %s\n",name, *(char **)_cfgParams[i].value_ptr);
        }
      break;
      case CFG_INT:
        if (*(int *)_cfgParams[i].value_ptr == UNKNOWN)
          LOG(LOG_NOTICE, "%-35s =\n", name);
        else
          LOG(LOG_NOTICE, "%-35s = %d\n", name, *(int *)_cfgParams[i].value_ptr);
      break;
      case CFG_BOOL:
        LOG(LOG_NOTICE, "%-35s = %s\n", name, bool2text(*(bool *)_cfgParams[i].value_ptr));
      break;
      case CFG_HEX:
        LOG(LOG_NOTICE, "%-35s = 0x%02hhx\n", name, *(unsigned char *)_cfgParams[i].value_ptr);
      break;
      case CFG_FLOAT:
        LOG(LOG_NOTICE, "%-35s = %f\n", name, *(float *)_cfgParams[i].value_ptr);
      break;
      case CFG_BITMASK:
        LOG(LOG_NOTICE, "%-35s = %s\n", name, (*(uint16_t *)_cfgParams[i].value_ptr & _cfgParams[i].mask) == _cfgParams[i].mask?bool2text(true):bool2text(false));
      break;
      case CFG_SPECIAL:
        if (strncasecmp(_cfgParams[i].name, CFG_N_log_level, strlen(CFG_N_log_level)) == 0) {
          LOG(LOG_NOTICE, "%-35s = %s\n", name, log_priority_to_str(_acdconfig_.log_level));
        } else {
          LOG(LOG_NOTICE, "%-35s = NEED TO ADD CODE TO HANDLE THIS\n",name);
        }
      break;
    }
  }



}
