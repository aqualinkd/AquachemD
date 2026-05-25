
#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

#define CONFIG_C
#include "config.h"
#include "utils.h"
#include "aquachemd.h"
#include "cJSON.h"

void set_config_defaults();

#define SET_VAL_CFG_STRING(field, def)  _acdconfig_.field = def
#define SET_VAL_CFG_INT(field, def)     _acdconfig_.field = def
#define SET_VAL_CFG_BOOL(field, def)    _acdconfig_.field = def
#define SET_VAL_CFG_FLOAT(field, def)   _acdconfig_.field = def
#define SET_VAL_CFG_HEX(field, def)     _acdconfig_.field = def
#define SET_VAL_CFG_BITMASK(field, def) _acdconfig_.field = def
#define SET_VAL_CFG_TXT_INT(field, def) _acdconfig_.field = def
// Define the initialization macro to do nothing
#define SET_VAL_CFG_CUSTOM(field, def)  /* Handled by specialized logic */

/*
#define SET_VAL_CFG_STRING(field, def)  do { char *__v = (char *)(def); memcpy(&(_acdconfig_.field), &__v, sizeof(__v)); } while(0)
#define SET_VAL_CFG_INT(field, def)     do { int __v = (int)(def); memcpy(&(_acdconfig_.field), &__v, sizeof(__v)); } while(0)
#define SET_VAL_CFG_BOOL(field, def)    do { bool __v = (bool)(def); memcpy(&(_acdconfig_.field), &__v, sizeof(__v)); } while(0)
#define SET_VAL_CFG_FLOAT(field, def)   do { float __v = (float)(def); memcpy(&(_acdconfig_.field), &__v, sizeof(__v)); } while(0)
#define SET_VAL_CFG_HEX(field, def)     do { unsigned char __v = (unsigned char)(def); memcpy(&(_acdconfig_.field), &__v, sizeof(__v)); } while(0)
#define SET_VAL_CFG_BITMASK(field, def) do { uint16_t __v = (uint16_t)(def); memcpy(&(_acdconfig_.field), &__v, sizeof(__v)); } while(0)
#define SET_VAL_CFG_TXT_INT(field, def) do { int __v = (int)(def); memcpy(&(_acdconfig_.field), &__v, sizeof(__v)); } while(0)
#define SET_VAL_CFG_CUSTOM(field, def)  // Bypassed for custom range step arrays 
*/
/*
* Below is better but needs -Wstrict-aliasing turned off to stop compiller warnings
*/
/* 
#define SET_VAL_CFG_STRING(field, def)  (*(char **)&(_acdconfig_.field) = (char *)(def))
#define SET_VAL_CFG_INT(field, def)     (*(int *)&(_acdconfig_.field) = (int)(def))
#define SET_VAL_CFG_BOOL(field, def)    (*(bool *)&(_acdconfig_.field) = (bool)(def))
#define SET_VAL_CFG_FLOAT(field, def)   (*(float *)&(_acdconfig_.field) = (float)(def))
#define SET_VAL_CFG_HEX(field, def)     (*(unsigned char *)&(_acdconfig_.field) = (unsigned char)(def))
#define SET_VAL_CFG_BITMASK(field, def) (*(uint16_t *)&(_acdconfig_.field) = (uint16_t)(def))
#define SET_VAL_CFG_TXT_INT(field, def) (*(int *)&(_acdconfig_.field) = (int)(def))
#define SET_VAL_CFG_CUSTOM(field, def)  // Bypassed for custom range step arrays 
*/


// Helper to stop cJSON to handle a float correctly.
static void cJSON_AddFloat(cJSON *object, const char *name, float value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.6g", value); // %g automatically drops trailing zeros
    cJSON_AddRawToObject(object, name, buf);
}


static struct {
    char *label;
    char *topic_path;
    char *char_value;
    float value;
    float value2;
    int   pin;
    bool  pin_mode;
    bool  pin_state;
    bool  is_global;
    unsigned char address;
    acd_type_t pending_type;
    uint8_t flags;
} _staging;

void add_condition_mqtt(const char *label, const char *topic, const char *value, bool is_global);
void add_condition_gpio(const char *label, int pin, gpio_active_t pin_mode, gpio_req_t pin_state, bool is_global);
void add_output_gpio(const char *label, acd_type_t type, int pin, gpio_active_t pin_mode, gpio_req_t pin_state, float ml_ps, uint32_t flags);
void add_sensor_ezo(const char *label, acd_type_t type, unsigned char address, bool is_global);
void add_sensor_d1w(const char *label, acd_type_t type, const char *path, float offset, float scale, bool is_global);
void add_sensor_mqtt(const char *label, acd_type_t type, const char *topic, bool is_global);

void init_cfg_parameters() {
    LOG(LOG_DEBUG, "Initializing config with %d entries", CFG_PARAM_COUNT);
    int i = 0;

#define CFG_ENTRY(cfg_name, cfg_field, cfg_def, cfg_type, cfg_mask, cfg_flag, cfg_meta) \
    if (i < CFG_PARAM_COUNT) {                                  \
        _cfgParams[i].name        = cfg_name;                   \
        _cfgParams[i].value_ptr   = (((cfg_mask) & CFG_MULTIPLE) && (cfg_type != CFG_CUSTOM)) ? NULL : &_acdconfig_.cfg_field; \
        _cfgParams[i].value_type  = cfg_type;                   \
        _cfgParams[i].config_mask = cfg_mask;                   \
        _cfgParams[i].bit_flag    = cfg_flag;                   \
        _cfgParams[i].metadata    = cfg_meta;                   \
                                                                \
        if (!((cfg_mask) & CFG_MULTIPLE)) {                     \
            SET_VAL_ ## cfg_type(cfg_field, cfg_def);           \
        }                                                       \
        i++;                                                    \
    }

#undef CONFIG_TABLE_H_ 
#include "config_table.h"
#undef CFG_ENTRY
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
    _staging.pending_type = ACD_TYPE_MASTER;
    _staging.flags = 0;
    _staging.is_global = true;
}

void action_staging() {
    if (!_staging.label) return;

    switch (_staging.pending_type) {
        case ACD_TYPE_EZO_PH:
        case ACD_TYPE_EZO_ORP:
        case ACD_TYPE_EZO_TEMP:
        case ACD_TYPE_EZO_PMP:
        case ACD_TYPE_EZO_PRS:
            add_sensor_ezo(_staging.label, _staging.pending_type, _staging.address, _staging.is_global);
            break;
        case ACD_TYPE_MQTT_TEMP:
            add_sensor_mqtt(_staging.label, _staging.pending_type, _staging.topic_path, _staging.is_global);
            break;
        case ACD_TYPE_D1W_TEMP:
            add_sensor_d1w(_staging.label, _staging.pending_type, _staging.topic_path, _staging.value, _staging.value2, _staging.is_global);
            break;
        case ACD_TYPE_GPIO_PMP:
            add_output_gpio(_staging.label, _staging.pending_type, _staging.pin, _staging.pin_mode, _staging.pin_state, _staging.value, _staging.flags);
            break;
        case ACD_TYPE_MQTT_COND:
            add_condition_mqtt(_staging.label, _staging.topic_path, _staging.char_value, _staging.is_global);
            break;
        case ACD_TYPE_GPIO_COND:
            add_condition_gpio(_staging.label, _staging.pin, _staging.pin_mode, _staging.pin_state, _staging.is_global);
            break;
        default:
            LOG(LOG_ERR, "Didn't create config entry for %s", _staging.label);
            break;
    }
    clear_staging();
}

bool parse_dose_range(runtime_range_t *steps, uint8_t *count, const char *value, bool sort_descending) {
    if (*count >= MAX_DOSING_RANGES) return false;
    float threshold;
    uint32_t seconds;
    if (sscanf(value, "%f:%u", &threshold, &seconds) != 2) return false;

    int idx = *count;
    steps[idx].threshold = threshold;
    steps[idx].seconds = seconds;
    (*count)++;

    for (int j = idx; j > 0; j--) {
        bool swap = (sort_descending) ? (steps[j].threshold > steps[j - 1].threshold) : (steps[j].threshold < steps[j - 1].threshold);
        if (swap) {
            runtime_range_t temp = steps[j];
            steps[j] = steps[j - 1];
            steps[j - 1] = temp;
        } else break;
    }
    return true;
}

bool setConfigValue(struct aquachemdata *acdata, char *param, char *value) {
    value = cleanwhitespace(value);

    for (int i = 0; i < CFG_PARAM_COUNT; i++) {
        if (strncasecmp(param, _cfgParams[i].name, strlen(_cfgParams[i].name)) == 0) {
            if (_cfgParams[i].metadata != NULL && strncasestr(_cfgParams[i].metadata, value, STR_FULL_LENGTH) == NULL) {
                return false;
            }
            if (strlen(value) <= 0) return true;

            // --- INTERCEPT ALL RANGE ARRAYS AND DYNAMIC ENTRY PARSERS ---
            if (_cfgParams[i].config_mask & CFG_MULTIPLE) {
                if (_cfgParams[i].value_type == CFG_CUSTOM) {
                    if (strncasecmp(param, "ph_dose_range", 13) == 0) {
                        return parse_dose_range(_acdconfig_.ph_steps, &_acdconfig_.ph_step_count, value, true);
                    } 
                    if (strncasecmp(param, "orp_dose_range", 14) == 0) {
                        return parse_dose_range(_acdconfig_.orp_steps, &_acdconfig_.orp_step_count, value, false);
                    }
                    return false;
                }

                // Dynamic Linked List Staging Parser
                if (strstr(param, "_label") != NULL) {
                    action_staging();
                    clear_staging();
                    _staging.label = strdup(value);
                }
                else if (strstr(param, "_type")) {
                    if (strcasecmp(value, "ezo") == 0) {
                        if (strstr(param, "ph_sensor"))       {_staging.pending_type = ACD_TYPE_EZO_PH;   _staging.address = EZO_PH_ADDR;}
                        else if (strstr(param, "orp_sensor")) {_staging.pending_type = ACD_TYPE_EZO_ORP;  _staging.address = EZO_ORP_ADDR;}
                        else if (strstr(param, "temp_sensor")){_staging.pending_type = ACD_TYPE_EZO_TEMP; _staging.address = EZO_RTD_ADDR;}
                        else if (strstr(param, "prs_sensor")) {_staging.pending_type = ACD_TYPE_EZO_PRS;  _staging.address = EZO_PRS_ADDR;}
                        else if (strstr(param, "doser")) {
                            _staging.pending_type = ACD_TYPE_EZO_PMP;
                            _staging.address = EZO_PMP_ADDR;
                            if (strncasecmp(param, "ph", 2) == 0) setMASK(_staging.flags, PH_PUMP);
                            else if (strncasecmp(param, "orp", 2) == 0) setMASK(_staging.flags, ORP_PUMP);
                        }
                    } 
                    else if (strcasecmp(value, "d1w") == 0) _staging.pending_type = ACD_TYPE_D1W_TEMP;
                    else if (strcasecmp(value, "mqtt") == 0) _staging.pending_type = ACD_TYPE_MQTT_TEMP;
                    else if (strcasecmp(value, "gpio") == 0) {
                        if (strstr(param, "doser")) {
                            _staging.pending_type = ACD_TYPE_GPIO_PMP;
                            if (strncasecmp(param, "ph", 2) == 0) setMASK(_staging.flags, PH_PUMP);
                            else if (strncasecmp(param, "orp", 2) == 0) setMASK(_staging.flags, ORP_PUMP);
                        }
                    }
                }
                else if (strncasecmp(param, "mqtt_condition_topic", 20) == 0) {
                    _staging.pending_type = ACD_TYPE_MQTT_COND;
                    if (_staging.topic_path) free(_staging.topic_path);
                    _staging.topic_path = strdup(value);
                }
                else if (strstr(param, "_address")) {
                    _staging.address = (unsigned char)strtoul(value, NULL, 16);
                }
                else if (strncasecmp(param, "temp_sensor_path", 16) == 0) {
                    if (_staging.topic_path) free(_staging.topic_path);
                    _staging.topic_path = strdup(value);
                }
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
                else if (strstr(param, "_condition_scope_global") ||
                         strstr(param, "_sensor_scope_global")) {
                    _staging.is_global = parse_bool(value);
                }
                else if (strncasecmp(param, "gpio_condition_pin_mode", 23) == 0) {
                    _staging.pin_mode = parse_gpio_active(value);
                    _staging.pending_type = ACD_TYPE_GPIO_COND;
                }
                else if (strncasecmp(param, "gpio_condition_required_state", 29) == 0) {
                    _staging.pin_state = parse_gpio_req(value);
                    _staging.pending_type = ACD_TYPE_GPIO_COND;
                }
                else if (strstr(param, "doser_pin_mode")) {
                    _staging.pin_mode = parse_gpio_active(value);
                }
                else if (strstr(param, "doser_required_state")) {
                    _staging.pin_state = parse_gpio_req(value);
                }
                else if (strstr(param, "doser_ml_per_second")) {
                    _staging.value = strtof(value, NULL);
                }
                else if (strncasecmp(param, "gpio_condition_pin", 18) == 0 || strstr(param, "doser_pin")) {
                    _staging.pin = (int)strtoul(value, NULL, 10);
                    if (strncasecmp(param, "gpio_condition", 14) == 0) _staging.pending_type = ACD_TYPE_GPIO_COND;
                }
                else if (strncasecmp(param, "mqtt_condition_value", 20) == 0) {
                    if (_staging.char_value) free(_staging.char_value);
                    _staging.char_value = strdup(value);
                    _staging.pending_type = ACD_TYPE_MQTT_COND;
                }
                return true;
            }

            // Standard Entries
            switch (_cfgParams[i].value_type) {
                case CFG_STRING:
                    if (_cfgParams[i].config_mask & CFG_IS_ALLOCATED) {
                        free(*(char **)_cfgParams[i].value_ptr);
                        *(char **)_cfgParams[i].value_ptr = NULL;
                    }
                    *(char **)_cfgParams[i].value_ptr = cleanalloc(value, STR_FULL_LENGTH);
                    _cfgParams[i].config_mask |= CFG_IS_ALLOCATED;
                    break;
                case CFG_INT:
                    *(int *)_cfgParams[i].value_ptr = strtoul(value, NULL, 10);
                    break;
                case CFG_BOOL:
                    *(bool *)_cfgParams[i].value_ptr = parse_bool(value);
                    break;
                case CFG_HEX:
                    *(unsigned char *)_cfgParams[i].value_ptr = strtoul(value, NULL, 16);
                    break;
                case CFG_FLOAT:
                    *(float *)_cfgParams[i].value_ptr = strtof(value, NULL);
                    break;
                case CFG_TXT_INT:
                    if (_cfgParams[i].value_ptr == &_acdconfig_.log_level) {
                        *(int *)_cfgParams[i].value_ptr = log_str_to_priority(value);
                    }
                    break;
                default: break;
            }
            return true;
        }
    }
    return false;
}

void parse_config_file(struct aquachemdata *acdata) {
    FILE *fp;
    char bufr[MAXCFGLINE];
    char *b_ptr;
    bool log_debug = (_acdconfig_.log_level >= LOG_DEBUG);

    init_cfg_parameters();
    if (log_debug) _acdconfig_.log_level = LOG_DEBUG;

    if ((fp = fopen(_acdconfig_.config_file, "r")) != NULL) {
        while (!feof(fp)) {
            if (fgets(bufr, MAXCFGLINE, fp) != NULL) {
                b_ptr = &bufr[0];
                char *indx;
                while (isspace(*b_ptr)) b_ptr++;
                if (b_ptr[0] != '\0' && b_ptr[0] != '#') {
                    indx = strchr(b_ptr, '=');
                    if (indx != NULL) {
                        if (!setConfigValue(acdata, b_ptr, indx + 1)) {
                            char *end = b_ptr + strlen(b_ptr) - 1;
                            while (end > b_ptr && isspace(*end)) end--;
                            LOG(LOG_ERR, "Unknown config parameter '%.*s'\n", (int)(end - b_ptr + 1), b_ptr);
                        } else if (log_debug) {
                            _acdconfig_.log_level = LOG_DEBUG;
                        }
                    }
                }
            }
        }
        action_staging();
        fclose(fp);
    } else {
        exit(EXIT_FAILURE);
    }
    acdata->keys->next = _acdconfig_.keys;
    check_print_config(acdata);
}


/*

THIS WILL CORE DUMP / FAIL.   Need to check 
if (_cfgParams[i].config_mask & CFG_MULTIPLE) {
  continue / break / don;t evaluate
}

*/
bool write_config_file(struct aquachemdata *acdata) {
    FILE *fp = fopen(_acdconfig_.config_file, "w");
    if (fp == NULL) return false;

    char *lastName = NULL;
    for (int i = 0; i < CFG_PARAM_COUNT; i++) {
        if (_cfgParams[i].config_mask & (CFG_HIDE | CFG_MULTIPLE)) continue;

        if (lastName != NULL && lastName[0] != _cfgParams[i].name[0]) {
            fprintf(fp, "\n");
        }

        switch (_cfgParams[i].value_type) {
            case CFG_STRING:
                if (*(char **)_cfgParams[i].value_ptr == NULL) fprintf(fp, "#%s=\n", _cfgParams[i].name);
                else fprintf(fp, "%s=%s\n", _cfgParams[i].name, *(char **)_cfgParams[i].value_ptr);
                break;
            case CFG_INT:
                if (*(int *)_cfgParams[i].value_ptr == UNKNOWN) fprintf(fp, "#%s=\n", _cfgParams[i].name);
                else fprintf(fp, "%s=%d\n", _cfgParams[i].name, *(int *)_cfgParams[i].value_ptr);
                break;
            case CFG_BOOL:
                fprintf(fp, "%s=%s\n", _cfgParams[i].name, bool_to_str(*(bool *)_cfgParams[i].value_ptr));
                break;
            case CFG_HEX:
                fprintf(fp, "%s=0x%02hhx\n", _cfgParams[i].name, *(unsigned char *)_cfgParams[i].value_ptr);
                break;
            case CFG_FLOAT:
                fprintf(fp, "%s=%f\n", _cfgParams[i].name, *(float *)_cfgParams[i].value_ptr);
                break;
            case CFG_TXT_INT:
                if (_cfgParams[i].value_ptr == &_acdconfig_.log_level) {
                    fprintf(fp, "%s=%s\n", _cfgParams[i].name, log_priority_to_str(_acdconfig_.log_level));
                }
                break;
            default: break;
        }
        lastName = _cfgParams[i].name;
    }

    if (_acdconfig_.ph_step_count > 0) {
        fprintf(fp, "\n# pH Dosing Ranges\n");
        for (int s = 0; s < _acdconfig_.ph_step_count; s++) {
            fprintf(fp, "ph_dose_range=%.1f:%d\n", _acdconfig_.ph_steps[s].threshold, _acdconfig_.ph_steps[s].seconds);
        }
    }
    if (_acdconfig_.orp_step_count > 0) {
        fprintf(fp, "\n# ORP Dosing Ranges\n");
        for (int s = 0; s < _acdconfig_.orp_step_count; s++) {
            fprintf(fp, "orp_dose_range=%.1f:%d\n", _acdconfig_.orp_steps[s].threshold, _acdconfig_.orp_steps[s].seconds);
        }
    }

    /* --- Sensors Section --- */
    for (acd_key_t *curr = _acdconfig_.keys; curr != NULL; curr = curr->next) {
        fprintf(fp, "\n");
        switch (curr->type) {
            case ACD_TYPE_MQTT_COND:
                fprintf(fp, "mqtt_condition_label=%s\n", curr->label);
                fprintf(fp, "mqtt_condition_topic=%s\n", curr->data.mqtt.topic);
                fprintf(fp, "mqtt_condition_value=%s\n", curr->data.mqtt.target_value);
                break;
            case ACD_TYPE_GPIO_COND:
                fprintf(fp, "gpio_condition_label=%s\n", curr->label);
                fprintf(fp, "gpio_condition_pin=%d\n", curr->data.gpio.pin);
                fprintf(fp, "gpio_condition_pin_mode=%s\n", gpio_active_to_str(curr->data.gpio.active));
                fprintf(fp, "gpio_condition_required_state=%s\n", gpio_req_to_str(curr->data.gpio.required));
                break;
            case ACD_TYPE_EZO_PH:
                fprintf(fp, "ph_sensor_label=%s\nph_sensor_type=ezo\nph_sensor_address=0x%02x\n", curr->label, curr->data.ezo.address);
                break;
            case ACD_TYPE_EZO_ORP:
                fprintf(fp, "orp_sensor_label=%s\norp_sensor_type=ezo\norp_sensor_address=0x%02x\n", curr->label, curr->data.ezo.address);
                break;
            case ACD_TYPE_EZO_TEMP:
                fprintf(fp, "temp_sensor_label=%s\ntemp_sensor_type=ezo\ntemp_sensor_address=0x%02x\n", curr->label, curr->data.ezo.address);
                break;
            case ACD_TYPE_MQTT_TEMP:
                fprintf(fp, "temp_sensor_label=%s\ntemp_sensor_type=mqtt\ntemp_sensor_topic=%s\n", curr->label, curr->data.mqtt.topic);
                break;
            case ACD_TYPE_D1W_TEMP:
                fprintf(fp, "temp_sensor_label=%s\ntemp_sensor_type=d1w\ntemp_sensor_path=%s\n", curr->label, curr->data.w1.path);
                fprintf(fp, "temp_sensor_offset=%f\ntemp_sensor_scale=%.4f\n", curr->data.w1.offset, curr->data.w1.scale);
                break;
            case ACD_TYPE_GPIO_PMP:
                if (curr->flags & PH_PUMP) {
                    fprintf(fp, "ph_doser_label=%s\nph_doser_type=gpio\nph_doser_pin=%d\n", curr->label, curr->data.gpio.pin);
                    fprintf(fp, "ph_doser_pin_mode=%s\nph_doser_required_state=%s\n", gpio_active_to_str(curr->data.gpio.active), gpio_req_to_str(curr->data.gpio.required));
                    fprintf(fp, "ph_doser_ml_per_second=%f\n", curr->flow_rate);
                } else {
                    fprintf(fp, "orp_doser_label=%s\norp_doser_type=gpio\norp_doser_pin=%d\n", curr->label, curr->data.gpio.pin);
                    fprintf(fp, "orp_doser_pin_mode=%s\norp_doser_required_state=%s\n", gpio_active_to_str(curr->data.gpio.active), gpio_req_to_str(curr->data.gpio.required));
                    fprintf(fp, "orp_doser_ml_per_second=%f\n", curr->flow_rate);
                }
                break;
            default: break;
        }
    }
    fclose(fp);
    return true;
}





bool build_aquachem_config_json(char *buffer, size_t buf_size) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return false;

    cJSON_AddStringToObject(root, "type", "config_schema");
    cJSON *fields = cJSON_AddArrayToObject(root, "fields");

    // 1. GLOBAL SYSTEM PARAMETERS
    for (int i = 0; i < CFG_PARAM_COUNT; i++) {
        if (_cfgParams[i].config_mask & (CFG_HIDE | CFG_MULTIPLE)){
          // We will allow ph_steps and orm_steps which are CFG_MULTIPLE to be handled in this block.
          if ( _cfgParams[i].value_ptr != &_acdconfig_.ph_steps &&
               _cfgParams[i].value_ptr != &_acdconfig_.orp_steps) {
            continue;
          }
        } 

        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "key", _cfgParams[i].name);
        cJSON_AddBoolToObject(item, "readonly", (_cfgParams[i].config_mask & CFG_READONLY) ? true : false);

        if (_cfgParams[i].metadata) {
            cJSON *opts = cJSON_Parse(_cfgParams[i].metadata);
            if (opts) cJSON_AddItemToObject(item, "options", opts);
        }

        switch (_cfgParams[i].value_type) {
            case CFG_STRING:
                cJSON_AddStringToObject(item, "type", "text");
                cJSON_AddStringToObject(item, "value", *(char **)_cfgParams[i].value_ptr ? *(char **)_cfgParams[i].value_ptr : "");
                break;
            case CFG_INT:
                cJSON_AddStringToObject(item, "type", "number");
                cJSON_AddNumberToObject(item, "value", *(int *)_cfgParams[i].value_ptr);
                break;
            case CFG_BOOL:
                cJSON_AddStringToObject(item, "type", "boolean");
                cJSON_AddBoolToObject(item, "value", *(bool *)_cfgParams[i].value_ptr);
                break;
            case CFG_FLOAT:
                cJSON_AddStringToObject(item, "type", "number");
                cJSON_AddFloat(item, "value", *(float *)_cfgParams[i].value_ptr);
                break;
            case CFG_TXT_INT:
                cJSON_AddStringToObject(item, "type", "select");
                if (_cfgParams[i].value_ptr == &_acdconfig_.log_level) {
                    cJSON_AddStringToObject(item, "value", log_priority_to_str(_acdconfig_.log_level));
                }
                break;
            case CFG_CUSTOM:
              if (_cfgParams[i].value_ptr == &_acdconfig_.ph_steps || _cfgParams[i].value_ptr == &_acdconfig_.orp_steps) {
                uint8_t step_cnt;
                runtime_range_t *steps;
                int decimal = 0;
                char range[12];
                if (_cfgParams[i].value_ptr == &_acdconfig_.ph_steps) {
                  step_cnt = _acdconfig_.ph_step_count;
                  steps = _acdconfig_.ph_steps;
                  decimal = 1;
                } else {
                  step_cnt = _acdconfig_.orp_step_count;
                  steps = _acdconfig_.orp_steps;
                  decimal = 0;
                }
                cJSON_AddStringToObject(item, "type", "array_text");
                cJSON *val_array = cJSON_CreateArray();
                for (int s = 0; s < step_cnt; s++) {
                  snprintf( range, sizeof(range), "%.*f:%d",decimal,steps[s].threshold, steps[s].seconds);
                  cJSON_AddItemToArray(val_array, cJSON_CreateString(range));
                }
                cJSON_AddItemToObject(item, "value", val_array);
              }
              break;
            default:
                cJSON_AddStringToObject(item, "type", "text");
                break;
        }
        cJSON_AddItemToArray(fields, item);
    }

    // 2. DYNAMIC CUSTOM HARDWARE BLOCKS (With Detailed Input Schemas)
    cJSON *custom_blocks = cJSON_AddArrayToObject(root, "custom_blocks");
    acd_key_t *curr = _acdconfig_.keys;
    
    while (curr != NULL) {
        cJSON *block = cJSON_CreateObject();
        cJSON_AddStringToObject(block, "label", curr->label ? curr->label : "");
        
        // Add the primary identifiers the UI needs to group/categorize the block
        cJSON_AddNumberToObject(block, "block_type_id", (double)curr->type);
        
        // Every block gets an internal array describing its configurable fields
        cJSON *block_fields = cJSON_AddArrayToObject(block, "fields");
        cJSON *f_item; // Temporary object pointer for building field definitions

        switch (curr->type) {
            case ACD_TYPE_MQTT_COND:
                cJSON_AddStringToObject(block, "driver_type", "mqtt_condition");
                
                f_item = cJSON_CreateObject();
                cJSON_AddStringToObject(f_item, "key", "mqtt_condition_topic");
                cJSON_AddStringToObject(f_item, "type", "text");
                cJSON_AddBoolToObject(f_item, "readonly", false);
                cJSON_AddStringToObject(f_item, "value", curr->data.mqtt.topic ? curr->data.mqtt.topic : "");
                cJSON_AddItemToArray(block_fields, f_item);
                
                f_item = cJSON_CreateObject();
                cJSON_AddStringToObject(f_item, "key", "mqtt_condition_value");
                cJSON_AddStringToObject(f_item, "type", "text");
                cJSON_AddBoolToObject(f_item, "readonly", false);
                cJSON_AddStringToObject(f_item, "value", curr->data.mqtt.target_value ? curr->data.mqtt.target_value : "");
                cJSON_AddItemToArray(block_fields, f_item);
                
                f_item = cJSON_CreateObject();
                cJSON_AddStringToObject(f_item, "key", "mqtt_condition_scope_global");
                cJSON_AddStringToObject(f_item, "type", "boolean");
                cJSON_AddBoolToObject(f_item, "readonly", false);
                cJSON_AddItemToObject(f_item, "options", cJSON_Parse(CFG_V_BOOL));
                cJSON_AddBoolToObject(f_item, "value", curr->scope==ACD_ACTION_BLOCK?true:false);
                
                cJSON_AddItemToArray(block_fields, f_item);
                break;

            case ACD_TYPE_GPIO_COND:
                cJSON_AddStringToObject(block, "driver_type", "gpio_condition");
                
                f_item = cJSON_CreateObject();
                cJSON_AddStringToObject(f_item, "key", "gpio_condition_pin");
                cJSON_AddStringToObject(f_item, "type", "number");
                cJSON_AddBoolToObject(f_item, "readonly", false);
                cJSON_AddNumberToObject(f_item, "value", curr->data.gpio.pin);
                cJSON_AddItemToArray(block_fields, f_item);

                f_item = cJSON_CreateObject();
                cJSON_AddStringToObject(f_item, "key", "gpio_condition_pin_mode");
                cJSON_AddStringToObject(f_item, "type", "select");
                cJSON_AddBoolToObject(f_item, "readonly", false);
                cJSON_AddStringToObject(f_item, "value", gpio_active_to_str(curr->data.gpio.active));
                cJSON_AddItemToObject(f_item, "options", cJSON_Parse("[\"Active High\",\"Active Low\"]"));
                cJSON_AddItemToArray(block_fields, f_item);

                f_item = cJSON_CreateObject();
                cJSON_AddStringToObject(f_item, "key", "gpio_condition_required_state");
                cJSON_AddStringToObject(f_item, "type", "select");
                cJSON_AddBoolToObject(f_item, "readonly", false);
                cJSON_AddStringToObject(f_item, "value", gpio_req_to_str(curr->data.gpio.required));
                cJSON_AddItemToObject(f_item, "options", cJSON_Parse("[\"on\",\"off\"]"));
                cJSON_AddItemToArray(block_fields, f_item);

                f_item = cJSON_CreateObject();
                cJSON_AddStringToObject(f_item, "key", "gpio_condition_scope_global");
                cJSON_AddStringToObject(f_item, "type", "boolean");
                cJSON_AddBoolToObject(f_item, "readonly", false);
                cJSON_AddItemToObject(f_item, "options", cJSON_Parse(CFG_V_BOOL));
                cJSON_AddBoolToObject(f_item, "value", curr->scope==ACD_ACTION_BLOCK?true:false);
                cJSON_AddItemToArray(block_fields, f_item);
                break;

            case ACD_TYPE_EZO_TEMP:
            case ACD_TYPE_EZO_PH:
            case ACD_TYPE_EZO_ORP:
            case ACD_TYPE_EZO_PRS:
                cJSON_AddStringToObject(block, "driver_type", "ezo_sensor");
                
                f_item = cJSON_CreateObject();
                // Dynamically match key prefix based on sensor assignment type
                cJSON_AddStringToObject(f_item, "key", (curr->type == ACD_TYPE_EZO_PH) ? "ph_sensor_address" : 
                                                       (curr->type == ACD_TYPE_EZO_ORP) ? "orp_sensor_address" : "temp_sensor_address");
                cJSON_AddStringToObject(f_item, "type", "text"); // hex addresses treated as bounded text strings
                cJSON_AddBoolToObject(f_item, "readonly", false);
                {
                    char addr[10];
                    snprintf(addr, sizeof(addr), "0x%02x", curr->data.ezo.address);
                    cJSON_AddStringToObject(f_item, "value", addr);
                }
                cJSON_AddItemToArray(block_fields, f_item);

                f_item = cJSON_CreateObject();
                if (curr->type == ACD_TYPE_EZO_PH) {cJSON_AddStringToObject(f_item, "key", "ph_sensor_scope_global");}
                else if (curr->type == ACD_TYPE_EZO_ORP) {cJSON_AddStringToObject(f_item, "key", "orp_sensor_scope_global");}
                else if (curr->type == ACD_TYPE_EZO_PRS) {cJSON_AddStringToObject(f_item, "key", "prs_sensor_scope_global");}
                else {cJSON_AddStringToObject(f_item, "key", "temp_sensor_scope_global");}

                cJSON_AddStringToObject(f_item, "type", "boolean");
                cJSON_AddBoolToObject(f_item, "readonly", false);
                cJSON_AddItemToObject(f_item, "options", cJSON_Parse(CFG_V_BOOL));
                cJSON_AddBoolToObject(f_item, "value", curr->scope==ACD_SCOPE_GLOBAL?true:false);

                break;

            case ACD_TYPE_MQTT_TEMP:
                cJSON_AddStringToObject(block, "driver_type", "mqtt_sensor");
                
                f_item = cJSON_CreateObject();
                cJSON_AddStringToObject(f_item, "key", "temp_sensor_topic");
                cJSON_AddStringToObject(f_item, "type", "text");
                cJSON_AddBoolToObject(f_item, "readonly", false);
                cJSON_AddStringToObject(f_item, "value", curr->data.mqtt.topic ? curr->data.mqtt.topic : "");
                cJSON_AddItemToArray(block_fields, f_item);
                break;

            case ACD_TYPE_D1W_TEMP:
                cJSON_AddStringToObject(block, "driver_type", "d1w_sensor");
                
                f_item = cJSON_CreateObject();
                cJSON_AddStringToObject(f_item, "key", "temp_sensor_path");
                cJSON_AddStringToObject(f_item, "type", "text");
                cJSON_AddBoolToObject(f_item, "readonly", false);
                cJSON_AddStringToObject(f_item, "value", (curr->data.w1.path[0] != '\0') ? curr->data.w1.path : "");
                cJSON_AddItemToArray(block_fields, f_item);

                f_item = cJSON_CreateObject();
                cJSON_AddStringToObject(f_item, "key", "temp_sensor_offset");
                cJSON_AddStringToObject(f_item, "type", "number");
                cJSON_AddBoolToObject(f_item, "readonly", false);
                cJSON_AddFloat(f_item, "value", curr->data.w1.offset);
                cJSON_AddItemToArray(block_fields, f_item);

                f_item = cJSON_CreateObject();
                cJSON_AddStringToObject(f_item, "key", "temp_sensor_scale");
                cJSON_AddStringToObject(f_item, "type", "number");
                cJSON_AddBoolToObject(f_item, "readonly", false);
                cJSON_AddFloat(f_item, "value", curr->data.w1.scale);
                cJSON_AddItemToArray(block_fields, f_item);
                break;

            case ACD_TYPE_GPIO_PMP:
                cJSON_AddStringToObject(block, "driver_type", "gpio_doser");
                
                // Determine whether this is a pH or ORP tracking pump instance
                bool is_ph = (curr->flags & PH_PUMP);
                
                f_item = cJSON_CreateObject();
                cJSON_AddStringToObject(f_item, "key", is_ph ? "ph_doser_pin" : "orp_doser_pin");
                cJSON_AddStringToObject(f_item, "type", "number");
                cJSON_AddBoolToObject(f_item, "readonly", false);
                cJSON_AddNumberToObject(f_item, "value", curr->data.gpio.pin);
                cJSON_AddItemToArray(block_fields, f_item);

                f_item = cJSON_CreateObject();
                cJSON_AddStringToObject(f_item, "key", is_ph ? "ph_doser_pin_mode" : "orp_doser_pin_mode");
                cJSON_AddStringToObject(f_item, "type", "select");
                cJSON_AddBoolToObject(f_item, "readonly", false);
                cJSON_AddStringToObject(f_item, "value", gpio_active_to_str(curr->data.gpio.active));
                cJSON_AddItemToObject(f_item, "options", cJSON_Parse("[\"Active High\",\"Active Low\"]"));
                cJSON_AddItemToArray(block_fields, f_item);

                f_item = cJSON_CreateObject();
                cJSON_AddStringToObject(f_item, "key", is_ph ? "ph_doser_required_state" : "orp_doser_required_state");
                cJSON_AddStringToObject(f_item, "type", "select");
                cJSON_AddBoolToObject(f_item, "readonly", false);
                cJSON_AddStringToObject(f_item, "value", gpio_req_to_str(curr->data.gpio.required));
                printf("***** %s required_state %s\n",curr->label, gpio_req_to_str(curr->data.gpio.required));
                cJSON_AddItemToObject(f_item, "options", cJSON_Parse("[\"on\",\"off\"]"));
                cJSON_AddItemToArray(block_fields, f_item);

                f_item = cJSON_CreateObject();
                cJSON_AddStringToObject(f_item, "key", is_ph ? "ph_doser_ml_per_second" : "orp_doser_ml_per_second");
                cJSON_AddStringToObject(f_item, "type", "number");
                cJSON_AddBoolToObject(f_item, "readonly", false);
                cJSON_AddFloat(f_item, "value", curr->flow_rate);
                cJSON_AddItemToArray(block_fields, f_item);
                break;

            case ACD_TYPE_EZO_PMP: // not implimented yet
            case ACD_TYPE_NONE:
            case ACD_TYPE_MASTER:
                break;
        }
        
        cJSON_AddItemToArray(custom_blocks, block);
        curr = curr->next;
    }

    // 3. MASTER DEFINITIONS FOR CREATING NEW BLOCKS
    cJSON *available_drivers = cJSON_AddArrayToObject(root, "available_drivers");
    cJSON *drv, *df_arr, *df_item;

    // Define: MQTT Condition Template
    drv = cJSON_CreateObject();
    cJSON_AddNumberToObject(drv, "block_type_id", 2);
    cJSON_AddStringToObject(drv, "driver_type", "mqtt_condition");
    cJSON_AddStringToObject(drv, "default_label", "New MQTT Condition");
    df_arr = cJSON_AddArrayToObject(drv, "fields");
    
    df_item = cJSON_CreateObject();
    cJSON_AddStringToObject(df_item, "key", "mqtt_condition_topic");
    cJSON_AddStringToObject(df_item, "type", "text");
    cJSON_AddStringToObject(df_item, "value", "");
    cJSON_AddItemToArray(df_arr, df_item);
    cJSON_AddItemToArray(available_drivers, drv);

    // Define: GPIO Condition Template
    drv = cJSON_CreateObject();
    cJSON_AddNumberToObject(drv, "block_type_id", 3);
    cJSON_AddStringToObject(drv, "driver_type", "gpio_condition");
    cJSON_AddStringToObject(drv, "default_label", "New Flow/Level Sensor");
    df_arr = cJSON_AddArrayToObject(drv, "fields");

    df_item = cJSON_CreateObject();
    cJSON_AddStringToObject(df_item, "key", "gpio_condition_pin");
    cJSON_AddStringToObject(df_item, "type", "number");
    cJSON_AddNumberToObject(df_item, "value", 0);
    cJSON_AddItemToArray(df_arr, df_item);

    df_item = cJSON_CreateObject();
    cJSON_AddStringToObject(df_item, "key", "gpio_condition_pin_mode");
    cJSON_AddStringToObject(df_item, "type", "select");
    cJSON_AddStringToObject(df_item, "value", "Active High");
    cJSON_AddItemToObject(df_item, "options", cJSON_Parse("[\"Active High\",\"Active Low\"]"));
    cJSON_AddItemToArray(df_arr, df_item);

    df_item = cJSON_CreateObject();
    cJSON_AddStringToObject(df_item, "key", "gpio_condition_required_state");
    cJSON_AddStringToObject(df_item, "type", "select");
    cJSON_AddStringToObject(df_item, "value", "off");
    cJSON_AddItemToObject(df_item, "options", cJSON_Parse("[\"on\",\"off\"]"));
    cJSON_AddItemToArray(df_arr, df_item);
    cJSON_AddItemToArray(available_drivers, drv);

    // Define: 1-Wire Temperature Template
    drv = cJSON_CreateObject();
    cJSON_AddNumberToObject(drv, "block_type_id", 8);
    cJSON_AddStringToObject(drv, "driver_type", "d1w_sensor");
    cJSON_AddStringToObject(drv, "default_label", "New 1-Wire Sensor");
    df_arr = cJSON_AddArrayToObject(drv, "fields");

    df_item = cJSON_CreateObject();
    cJSON_AddStringToObject(df_item, "key", "temp_sensor_path");
    cJSON_AddStringToObject(df_item, "type", "text");
    cJSON_AddStringToObject(df_item, "value", "/sys/bus/w1/devices/");
    cJSON_AddItemToArray(df_arr, df_item);

    df_item = cJSON_CreateObject();
    cJSON_AddStringToObject(df_item, "key", "temp_sensor_offset");
    cJSON_AddStringToObject(df_item, "type", "number");
    cJSON_AddNumberToObject(df_item, "value", 0);
    cJSON_AddItemToArray(df_arr, df_item);

    df_item = cJSON_CreateObject();
    cJSON_AddStringToObject(df_item, "key", "temp_sensor_scale");
    cJSON_AddStringToObject(df_item, "type", "number");
    cJSON_AddFloat(df_item, "value", 1.0);
    cJSON_AddItemToArray(df_arr, df_item);
    cJSON_AddItemToArray(available_drivers, drv);

    // Define: GPIO Doser Template
    drv = cJSON_CreateObject();
    cJSON_AddNumberToObject(drv, "block_type_id", 9);
    cJSON_AddStringToObject(drv, "driver_type", "gpio_doser");
    cJSON_AddStringToObject(drv, "default_label", "New Chemical Doser");
    df_arr = cJSON_AddArrayToObject(drv, "fields");

    df_item = cJSON_CreateObject();
    cJSON_AddStringToObject(df_item, "key", "doser_pin");
    cJSON_AddStringToObject(df_item, "type", "number");
    cJSON_AddNumberToObject(df_item, "value", 0);
    cJSON_AddItemToArray(df_arr, df_item);

    df_item = cJSON_CreateObject();
    cJSON_AddStringToObject(df_item, "key", "doser_pin_mode");
    cJSON_AddStringToObject(df_item, "type", "select");
    cJSON_AddStringToObject(df_item, "value", "Active Low");
    cJSON_AddItemToObject(df_item, "options", cJSON_Parse("[\"Active High\",\"Active Low\"]"));
    cJSON_AddItemToArray(df_arr, df_item);

    df_item = cJSON_CreateObject();
    cJSON_AddStringToObject(df_item, "key", "doser_ml_per_second");
    cJSON_AddStringToObject(df_item, "type", "number");
    cJSON_AddFloat(df_item, "value", 1.0);
    cJSON_AddItemToArray(df_arr, df_item);
    cJSON_AddItemToArray(available_drivers, drv);

    // 3. RENDER TO MEMORY BUFFER
    bool success = cJSON_PrintPreallocated(root, buffer, (int)buf_size, 0);
    if (!success) {
        snprintf(buffer, buf_size, "{\"error\":\"buffer_overflow\"}");
    }

    cJSON_Delete(root);
    return success;
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

    // Print these after.
    if (_cfgParams[i].config_mask & CFG_MULTIPLE) {
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
        LOG(LOG_NOTICE, "%-*s = %s\n", MAX_PRINTLEN, name, bool_to_str(*(bool *)_cfgParams[i].value_ptr));
      break;
      case CFG_HEX:
        LOG(LOG_NOTICE, "%-*s = 0x%02hhx\n", MAX_PRINTLEN, name, *(unsigned char *)_cfgParams[i].value_ptr);
      break;
      case CFG_FLOAT:
        LOG(LOG_NOTICE, "%-*s = %f\n", MAX_PRINTLEN, name, *(float *)_cfgParams[i].value_ptr);
      break;
      case CFG_BITMASK:
        LOG(LOG_NOTICE, "%-*s = %s\n", MAX_PRINTLEN, name, (*(uint16_t *)_cfgParams[i].value_ptr & _cfgParams[i].bit_flag) == _cfgParams[i].bit_flag?bool_to_str(true):bool_to_str(false));
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


  if (_acdconfig_.ph_step_count > 0) {
    for (int s = 0; s < _acdconfig_.ph_step_count; s++) {
      LOG(LOG_NOTICE, "%-*s = >= %.1f (%ds)\n",MAX_PRINTLEN, "pH Dosing Range", _acdconfig_.ph_steps[s].threshold, _acdconfig_.ph_steps[s].seconds);
    }
  }
  if (_acdconfig_.orp_step_count > 0) {
    for (int s = 0; s < _acdconfig_.orp_step_count; s++) {
      LOG(LOG_NOTICE, "%-*s = <= %.1f (%ds)\n",MAX_PRINTLEN, "ORP Dosing Range", _acdconfig_.orp_steps[s].threshold, _acdconfig_.orp_steps[s].seconds);
    }
  }


  for (acd_key_t *curr = _acdconfig_.keys; curr != NULL; curr = curr->next) {
    const char *type_str = "UNKNOWN";
    char buffer[64]; // Temporary storage for combined strings
    const char *role = (curr->flags & PH_PUMP)  ? "pH" : 
                       (curr->flags & ORP_PUMP) ? "ORP" : "undefined";
    buffer[0] = '\0';

    switch (curr->type) {
      case ACD_TYPE_MQTT_COND: type_str = "condition (MQTT)"; break;
      case ACD_TYPE_GPIO_COND: type_str = "condition (GPIO)"; break;
      case ACD_TYPE_EZO_TEMP:  type_str = "sensor (EZO Temp)"; break;
      case ACD_TYPE_EZO_PH:    type_str = "sensor (EZO pH)"; break;
      case ACD_TYPE_EZO_ORP:   type_str = "sensor (EZO ORP)"; break;
      case ACD_TYPE_EZO_PRS:   type_str = "sensor (EZO PMP)"; break;
      case ACD_TYPE_D1W_TEMP:  type_str = "sensor (1-Wire Temp)"; break;
      case ACD_TYPE_MQTT_TEMP: type_str = "sensor (MQTT Temp)"; break;
      case ACD_TYPE_GPIO_PMP:
        type_str = "pump (GPIO)";
        snprintf(buffer, sizeof(buffer), "(%s)", role);
        break;
      case ACD_TYPE_EZO_PMP:
        type_str = "pump (EZO PMP)";
        snprintf(buffer, sizeof(buffer), "(%s)", role);
        break;
      case ACD_TYPE_MASTER:
        break;
      case ACD_TYPE_NONE:
        default: type_str = "sensor (UNKNOWN)"; 
        break;
    }

    if (curr->type == ACD_TYPE_GPIO_PMP || curr->type == ACD_TYPE_GPIO_COND) {
      int len = strlen(buffer);
      snprintf(&buffer[len], sizeof(buffer)-len, "(pin=%d, %s, required state=%s)", 
                curr->data.gpio.pin, 
                gpio_active_to_str(curr->data.gpio.active), 
                gpio_req_to_str(curr->data.gpio.required));
    }

    LOG(LOG_NOTICE, "%-*s = %-8.8s| %s %s\n", MAX_PRINTLEN, type_str, curr->ID, curr->label,buffer);
  }

}









typedef enum {
  ACD_LABEL_MQTT,
  ACD_LABEL_GPIO,
  ACD_LABEL_EZO,
  ACD_LABEL_D1W,
  ACD_LABEL_PMP,  // Doser
  ACD_LABEL_PRS
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
    static int count_prs = MASTER_ID;

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
        case ACD_TYPE_EZO_PRS:
            prefix = "PRS";
            node->index = count_prs++;
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
    case ACD_LABEL_PRS:
      snprintf(buf, sizeof(buf), "PRS_%s", base);
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
void add_condition_mqtt(const char *label, const char *topic, const char *value, bool is_global) {
    
    acd_key_t *new_node = malloc(sizeof(acd_key_t));
    if (!new_node) return;

    new_node->type = ACD_TYPE_MQTT_COND;
    
    new_node->label = generate_label(topic, ACD_LABEL_MQTT, label);
    new_node->data.mqtt.topic = strdup(topic);
    new_node->data.mqtt.target_value = strdup(value);
    new_node->met = false; // Initial state, not met.
    new_node->scope = is_global?ACD_ACTION_BLOCK:ACD_ACTION_LIMIT;
    
    generate_condition_id(new_node);

    //new_node->target_value = UNKNOWN;

    append_to_key_list(new_node);
}

// Specialized function for GPIO
void add_condition_gpio(const char *label, int pin, gpio_active_t pin_mode, gpio_req_t pin_state, bool is_global) {
    acd_key_t *new_node = malloc(sizeof(acd_key_t));
    if (!new_node) return;

    new_node->type = ACD_TYPE_GPIO_COND;

    new_node->label = generate_label(int_to_str(pin), ACD_LABEL_GPIO, label);
    new_node->data.gpio.pin = pin;
    new_node->data.gpio.active = pin_mode;
    new_node->data.gpio.required = pin_state;
    new_node->met = false; // Initial state, not met.
    new_node->scope = is_global?ACD_ACTION_BLOCK:ACD_ACTION_LIMIT;
    
    generate_condition_id(new_node);
    
    append_to_key_list(new_node);

    //LOG(LOG_ERR, "GPIO %s Pin %d, active %d, state %d",new_node->label, new_node->data.gpio.pin, new_node->data.gpio.active, pin_state);
}

void add_sensor_ezo(const char *label, acd_type_t type, unsigned char address, bool is_global) {
  acd_key_t *new_node = malloc(sizeof(acd_key_t));
  
  //LOG(LOG_DEBUG, "Committing EZO: Label=%s, Type=%d, Addr=0x%02x ---- %s", label, type, address, hex_to_str(address));

  new_node->type = type;
  new_node->label = generate_label(hex_to_str(address), ACD_LABEL_EZO, label);
  new_node->data.ezo.address = address;
  new_node->scope = is_global?ACD_SCOPE_GLOBAL:ACD_SCOPE_LOCAL;
  generate_sensor_id(new_node);

  append_to_key_list(new_node);
}

void add_sensor_mqtt(const char *label, acd_type_t type, const char *topic, bool is_global) {
  acd_key_t *new_node = malloc(sizeof(acd_key_t));
  
  new_node->type = type;
  new_node->label = generate_label(topic, ACD_LABEL_MQTT, label);
  new_node->data.mqtt.topic = strdup(topic);
  new_node->scope = is_global?ACD_SCOPE_GLOBAL:ACD_SCOPE_LOCAL;
  generate_sensor_id(new_node);

  append_to_key_list(new_node);
}

void add_sensor_d1w(const char *label, acd_type_t type, const char *path, float offset, float scale, bool is_global) {
  acd_key_t *new_node = malloc(sizeof(acd_key_t));
  
  new_node->type = type;
  new_node->label = generate_label(path, ACD_LABEL_D1W, label);
  strcpy(new_node->data.w1.path, path);
  new_node->data.w1.offset = offset;
  new_node->data.w1.scale = scale;
  new_node->scope = is_global?ACD_SCOPE_GLOBAL:ACD_SCOPE_LOCAL;
  generate_sensor_id(new_node);

  append_to_key_list(new_node);
}

void add_output_gpio(const char *label, acd_type_t type, int pin, gpio_active_t pin_mode, gpio_req_t pin_state, float ml_per_sec, uint32_t flags) {
  acd_key_t *new_node = malloc(sizeof(acd_key_t));
  
  new_node->type = type;
  new_node->label = generate_label(int_to_str(pin), ACD_LABEL_PMP, label);
  new_node->data.gpio.pin = pin;
  new_node->data.gpio.active = pin_mode;
  new_node->data.gpio.required = pin_state;
  new_node->flow_rate = ml_per_sec;
  
  //printf("***** %s required_state %s\n",new_node->label, gpio_req_to_str(new_node->data.gpio.required));

  if (flags != 0) {
    new_node->flags = flags;
  }
  /*
  if (runtime <= 0) {
    new_node->runtime = _acdconfig_.ph_default_dose_time;
  } else {
    new_node->runtime = runtime;
  }
  */
  generate_sensor_id(new_node);

  append_to_key_list(new_node);

  //LOG(LOG_ERR, "GPIO %s Pin %d, active %d, state %d",new_node->label, new_node->data.gpio.pin, new_node->data.gpio.active, pin_state);
}