

#define _GNU_SOURCE  // for memmem
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include "json_messages.h"
#include "cJSON.h"
#include "aquachemd.h"
#include "acd_types.h"
#include "utils.h"
#include "version.h"
#include "net_services.h"
#include "acd_timer.h"

#define MAX_JSON_BUFFER_SIZE 8192

static cJSON *devices_map = NULL;

// This buffer holds the resulting JSON string, NOT thread safe, needs to be here as it's returned.
//static char json_buffer[MAX_JSON_BUFFER_SIZE];



/**
 * Parses a numeric value from either a JSON object ({"value": 1.5}) 
 * or a URL-encoded string (value=1.5).
 * * @param buf    Pointer to the message body buffer.
 * @param len    Length of the buffer.
 * @return float The extracted value, or UNKNOWN if not found/invalid.
 */
float parse_payload_value(const char *buf, size_t len) {
    if (!buf || len == 0) return UNKNOWN;

    LOG(LOG_DEBUG, "Parsing payload: '%.*s'\n", (int)len, buf);

    // 1. JSON Detection & Parsing
    // JSON usually starts with '{' or '[' (allowing for whitespace)
    const char *start = buf;
    while (start < buf + len && isspace((unsigned char)*start)) start++;

    if (start < buf + len && *start == '{') {
        cJSON *root = cJSON_ParseWithLength(start, len - (start - buf));
        if (root) {
            cJSON *val = cJSON_GetObjectItemCaseSensitive(root, "value");
            float result = UNKNOWN;
            
            if (cJSON_IsNumber(val)) {
                result = (float)val->valuedouble;
            } else if (cJSON_IsString(val)) {
                // strtof handles numeric strings like "1.5"
                result = strtof(val->valuestring, NULL);
            }
            
            cJSON_Delete(root);
            return result;
        }
    }

    // 2. URL-encoded Fallback (value=1.5&arg2=...)
    // Search for "value=" within the buffer
    const char *key = "value=";
    size_t key_len = strlen(key);
    
    // memmem is the safest way to search a non-null-terminated buffer
    char *pos = memmem(buf, len, key, key_len);
    if (pos) {
        char *val_start = pos + key_len;
        // strtof will stop at the first non-numeric char (like '&' or space)
        return strtof(val_start, NULL); 
    }

    return UNKNOWN;
}

/**
 * Parses URI and Value from a non-null-terminated JSON buffer.
 * @param json_str Pointer to the JSON buffer.
 * @param json_len Length of the JSON buffer.
 * @param uri_out  Pointer to a char buffer of size URI_LEN.
 * @param val_out  Pointer to a float where the value will be stored.
 * @return int     1 on success, 0 on failure.
 */
bool parse_json_uri_command(const char *json_str, size_t json_len, char *uri_out, float *val_out) {
    if (!json_str || json_len == 0 || !uri_out || !val_out) return 0;

    // Use the length-aware parser for non-null-terminated strings
    cJSON *root = cJSON_ParseWithLength(json_str, json_len);
    if (root == NULL) return 0;

    bool success = false;

    // 1. Extract URI
    cJSON *uri = cJSON_GetObjectItemCaseSensitive(root, "uri");
    if (cJSON_IsString(uri) && (uri->valuestring != NULL)) {
        // Safe copy: Fill buffer, ensure termination
        strncpy(uri_out, uri->valuestring, URI_LEN - 1);
        uri_out[URI_LEN - 1] = '\0';
        success = true;
    }

    // 2. Extract Value
    cJSON *val = cJSON_GetObjectItemCaseSensitive(root, "value");
    if (cJSON_IsNumber(val)) {
        *val_out = (float)val->valuedouble;
    } else if (cJSON_IsBool(val)) {
        *val_out = cJSON_IsTrue(val) ? 1.0f : 0.0f;
    } else if (cJSON_IsString(val)) {
        // Handle cases where numbers are sent as strings "1.5"
        *val_out = strtof(val->valuestring, NULL);
    } else {
        *val_out = UNKNOWN;
    }

    cJSON_Delete(root);
    return success;
}

void add_timedate_to_json(cJSON *root) {
    time_t rawtime;
    struct tm *timeinfo;
    char buffer[32];

    // Get current time
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    // Format: "1:17 PM 04/12/26 Sun"
    // %I = 12-hour hour, %M = minute, %p = AM/PM
    // %m = month, %d = day, %y = year (2 digits), %a = abbreviated weekday
    strftime(buffer, sizeof(buffer), "%I:%M %p", timeinfo);
    cJSON_AddStringToObject(root, "time", buffer);
    strftime(buffer, sizeof(buffer), "%m/%d/%y %a", timeinfo);
    cJSON_AddStringToObject(root, "date", buffer);
}

void populate_devices_json(struct aquachemdata *acddata, cJSON *devices)
{
  cJSON *device = cJSON_CreateObject();

  cJSON_AddStringToObject(device, "id", acddata->keys->ID);
  cJSON_AddStringToObject(device, "label", acddata->keys->label);
  cJSON_AddStringToObject(device, "status", acd_state_to_str(acddata->keys->state));
  cJSON_AddNumberToObject(device, "int_status", acddata->keys->state);
  cJSON_AddStringToObject(device, "type", "switch");
  cJSON_AddStringToObject(device, "type", "switch");
  cJSON *attributes = cJSON_CreateArray();
  cJSON_AddItemToArray(attributes, cJSON_CreateString(acd_state_to_str(ACD_LED_OFF)));
  cJSON_AddItemToArray(attributes, cJSON_CreateString(acd_state_to_str(ACD_LED_ON)));
  cJSON_AddItemToObject(device, "attributes", attributes);
  cJSON_AddItemToObject(devices, acddata->keys->ID, device);

  for (acd_key_t *curr = acddata->keys->next; curr != NULL; curr = curr->next) { 
    device = cJSON_CreateObject();
    cJSON_AddStringToObject(device, "id", curr->ID);
    cJSON_AddStringToObject(device, "label", curr->label);
    
    if (IS_CONDITION(curr->type)) {
      cJSON_AddStringToObject(device, "status", acd_state_to_str(curr->met?ACD_LED_ON:ACD_LED_OFF));
      cJSON_AddNumberToObject(device, "int_status", curr->met);
      cJSON_AddStringToObject(device, "type", "binary_sensor");
    } else {
      cJSON_AddStringToObject(device, "status", acd_state_to_str(curr->state));
      cJSON_AddNumberToObject(device, "int_status", curr->state);
      cJSON_AddNumberToObject(device, "value", curr->value);
      if (IS_OUTPUT(curr->type)) {
        cJSON_AddStringToObject(device, "type", "switch");
        cJSON *attributes = cJSON_CreateArray();
        cJSON_AddItemToArray(attributes, cJSON_CreateString("timer"));
        cJSON_AddItemToArray(attributes, cJSON_CreateString(acd_state_to_str(ACD_LED_ON)));
        cJSON_AddItemToArray(attributes, cJSON_CreateString(acd_state_to_str(ACD_LED_OFF)));
        cJSON_AddItemToArray(attributes, cJSON_CreateString(acd_state_to_str(ACD_LED_ENABLED)));
        cJSON_AddItemToObject(device, "attributes", attributes);

        uint32_t remaining_sec = get_timer_left_sec(curr);
        cJSON_AddStringToObject(device, "timer_active", acd_state_to_str(remaining_sec > 0?ACD_LED_ON:ACD_LED_OFF));
        cJSON_AddNumberToObject(device, "timer_duration", remaining_sec);
      } else {
        cJSON_AddStringToObject(device, "type", "sensor");
      }
    }

    cJSON_AddItemToObject(devices, curr->ID, device);
  }
/*
  for (acd_condition_t *curr = acddata->conditions; curr != NULL; curr = curr->next) { 
    device = cJSON_CreateObject();
    cJSON_AddStringToObject(device, "id", curr->ID);
    cJSON_AddStringToObject(device, "label", curr->label);
    cJSON_AddStringToObject(device, "status", acd_state_to_str(curr->met?ACD_LED_ON:ACD_LED_OFF));
    cJSON_AddNumberToObject(device, "int_status", curr->met);
    cJSON_AddStringToObject(device, "type", "binary_sensor");
    cJSON_AddItemToObject(devices, curr->ID, device);
  }
*/
}

const char* get_devices_json(struct aquachemdata *acddata) {
  static char json_buffer[MAX_JSON_BUFFER_SIZE];
  cJSON *root = cJSON_CreateObject();

  if (devices_map== NULL) {
    devices_map= cJSON_CreateObject();
    populate_devices_json(acddata, devices_map);
  } else {
    cJSON *item = NULL;

    item = cJSON_GetObjectItemCaseSensitive(devices_map, acddata->keys->ID);
    cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(item, "value"), acddata->keys->value);
    cJSON_SetValuestring(cJSON_GetObjectItemCaseSensitive(item, "status"), acd_state_to_str(acddata->keys->state));
    cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(item, "int_status"), acddata->keys->state);

    for (acd_key_t *curr = acddata->keys->next; curr != NULL; curr = curr->next) { 
      item = cJSON_GetObjectItemCaseSensitive(devices_map, curr->ID);

      if (IS_CONDITION(curr->type)) {
        cJSON_SetValuestring(cJSON_GetObjectItemCaseSensitive(item, "status"), acd_state_to_str(curr->met?ACD_LED_ON:ACD_LED_OFF));
        cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(item, "int_status"), curr->met);
      } else {
        cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(item, "value"), curr->value);
        cJSON_SetValuestring(cJSON_GetObjectItemCaseSensitive(item, "status"), acd_state_to_str(curr->state));
        cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(item, "int_status"), curr->state);
        if (IS_OUTPUT(curr->type)) {
          uint32_t remaining_sec = get_timer_left_sec(curr);
          cJSON_SetValuestring(cJSON_GetObjectItemCaseSensitive(item, "timer_active"), acd_state_to_str(remaining_sec > 0?ACD_LED_ON:ACD_LED_OFF));
          cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(item, "timer_duration"), remaining_sec);
        }
      } 
    }
    /*
    for (acd_condition_t *curr = acddata->conditions; curr != NULL; curr = curr->next) { 
      cJSON *item = cJSON_GetObjectItemCaseSensitive(devices_map, curr->ID);
      cJSON_SetValuestring(cJSON_GetObjectItemCaseSensitive(item, "status"), acd_state_to_str(curr->met?ACD_LED_ON:ACD_LED_OFF));
      cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(item, "int_status"), curr->met);
    }
    */
  }

  cJSON_AddStringToObject(root, AQUACHEMD_SHORT_NAME, AQUACHEMD_VERSION);
  cJSON_AddStringToObject(root, "type", "devices");
  cJSON_AddStringToObject(root, "display_message", acddata->display_message);
  
  add_timedate_to_json(root);

  // Make sure referance below so we don't delete or copy devices_map
  cJSON_AddItemReferenceToObject(root, "devices", devices_map);

  /*
  char *rendered = cJSON_Print(root);
  if (rendered) {
    printf("Current JSON State:\n%s\n", rendered);
    free(rendered);
  }*/

  int success = cJSON_PrintPreallocated(root, json_buffer, MAX_JSON_BUFFER_SIZE, 0);
  if (!success) {
    LOG(LOG_ERR, "JSON buffer overflow! MAX_JSON_BUFFER_SIZE (%d) is too small.", MAX_JSON_BUFFER_SIZE);
    snprintf(json_buffer, MAX_JSON_BUFFER_SIZE, "{\"error\": \"buffer_overflow\"}");
  }

  cJSON_Delete(root);
  return json_buffer;

  //cJSON_PrintUnformatted(devices);
}