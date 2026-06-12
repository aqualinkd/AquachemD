

#define _GNU_SOURCE  // for memmem
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <systemd/sd-journal.h>
#include <sys/time.h>

#include "json_messages.h"
#include "cJSON.h"
#include "aquachemd.h"
#include "acd_types.h"
#include "utils.h"
#include "version.h"
#include "net_services.h"
#include "acd_timer.h"
#include "sensor_stats.h"

#define MAX_JSON_BUFFER_SIZE 8192




static cJSON *devices_map = NULL;

// This buffer holds the resulting JSON string, NOT thread safe, needs to be here as it's returned.
//static char json_buffer[MAX_JSON_BUFFER_SIZE];

#define JSON_ERROR_OVERFLOW "{\"error\":\"server - buffer overflow\"}"



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
  //cJSON_AddStringToObject(device, "type", "switch");
  cJSON *attributes = cJSON_CreateArray();
  cJSON_AddItemToArray(attributes, cJSON_CreateString(acd_state_to_set_attrib(ACD_LED_OFF)));
  cJSON_AddItemToArray(attributes, cJSON_CreateString(acd_state_to_set_attrib(ACD_LED_ON)));
  cJSON_AddItemToArray(attributes, cJSON_CreateString("reset_stats"));
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
      if (curr->delay_on > 0) {
        cJSON *attributes = cJSON_CreateArray();
        cJSON_AddItemToArray(attributes, cJSON_CreateString("delay"));
        cJSON_AddItemToObject(device, "attributes", attributes);
        uint32_t remaining_sec = get_timer_left_sec(curr);
        cJSON_AddStringToObject(device, "delay_active", acd_state_to_str(remaining_sec > 0?ACD_LED_ON:ACD_LED_OFF));
        cJSON_AddNumberToObject(device, "delay_duration", remaining_sec);
      }
    } else {
      cJSON_AddStringToObject(device, "status", acd_state_to_str(curr->state));
      cJSON_AddNumberToObject(device, "int_status", curr->state);
      cJSON_AddNumberToObject(device, "value", curr->value);
      if (IS_OUTPUT(curr->type)) {
        cJSON_AddStringToObject(device, "type", "switch");
        cJSON *attributes = cJSON_CreateArray();
        cJSON_AddItemToArray(attributes, cJSON_CreateString("timer"));
        cJSON_AddItemToArray(attributes, cJSON_CreateString(acd_state_to_set_attrib(ACD_LED_ON)));
        cJSON_AddItemToArray(attributes, cJSON_CreateString(acd_state_to_set_attrib(ACD_LED_OFF)));
        cJSON_AddItemToArray(attributes, cJSON_CreateString(acd_state_to_set_attrib(ACD_LED_ENABLED)));
        
        if (isMASKSET(curr->flags, PH_PUMP) || isMASKSET(curr->flags, ORP_PUMP)) {
          cJSON_AddItemToArray(attributes, cJSON_CreateString("dosestats"));
        }
        
        uint32_t remaining_sec = get_timer_left_sec(curr);
        cJSON_AddStringToObject(device, "timer_active", acd_state_to_str(remaining_sec > 0?ACD_LED_ON:ACD_LED_OFF));
        cJSON_AddNumberToObject(device, "timer_duration", remaining_sec);
        if (isMASKSET(curr->flags, PH_PUMP)) {
          cJSON_AddNumberToObject(device, "timer_default_runtime", _acdconfig_.ph_default_dose_time);
          cJSON_AddItemToArray(attributes, cJSON_CreateString("ph_pump"));
        } else if (isMASKSET(curr->flags, ORP_PUMP)) {
          cJSON_AddNumberToObject(device, "timer_default_runtime", _acdconfig_.orp_default_dose_time);
          cJSON_AddItemToArray(attributes, cJSON_CreateString("orp_pump"));
        }
        
        cJSON_AddItemToObject(device, "attributes", attributes);
      } else {
        char buf[16];
        cJSON_AddStringToObject(device, "type", "sensor");
        if (isMASKSET(curr->flags,CALC_AVERAGE) ) {
          // statistical_sensor
          cJSON *stats = cJSON_CreateObject();
          cJSON_AddNumberToObject(stats, "avg", curr->stats.average);
          cJSON_AddNumberToObject(stats, "max", curr->stats.max);
          cJSON_AddNumberToObject(stats, "min", curr->stats.min);
          //cJSON_AddStringToObject(stats, "range", time_range_to_str(curr->flags));
          duration_seconds_to_string(curr->stats.tau_seconds, buf, sizeof(buf));
          cJSON_AddStringToObject(stats, "duration", buf);
          cJSON_AddItemToObject(device, "stats", stats);

          cJSON *attributes = cJSON_CreateArray();
          cJSON_AddItemToArray(attributes, cJSON_CreateString("stats"));
          cJSON_AddItemToObject(device, "attributes", attributes);
        } 
      }
    }

    cJSON_AddItemToObject(devices, curr->ID, device);
    
  }
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
        //cJSON_SetValuestring(cJSON_GetObjectItemCaseSensitive(item, "status"), acd_state_to_str(curr->met?ACD_LED_ON:ACD_LED_OFF));
        if (isMASKSET(curr->flags, DELAY_ACTIVE) || curr->state == ACD_LED_DELAY) {
          cJSON_SetValuestring(cJSON_GetObjectItemCaseSensitive(item, "status"), acd_state_to_str(ACD_LED_DELAY));
          cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(item, "int_status"), ACD_LED_DELAY);
        } else {
          cJSON_SetValuestring(cJSON_GetObjectItemCaseSensitive(item, "status"), acd_condition_met_to_str(curr->met));
          cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(item, "int_status"), curr->met);
        }
        
        if (curr->delay_on > 0) {
          uint32_t remaining_sec = get_timer_left_sec(curr);
          cJSON_SetValuestring(cJSON_GetObjectItemCaseSensitive(item, "delay_active"), acd_state_to_str(remaining_sec > 0?ACD_LED_ON:ACD_LED_OFF));
          cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(item, "delay_duration"), remaining_sec);
        }
      } else {
        cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(item, "value"), curr->value);
        cJSON_SetValuestring(cJSON_GetObjectItemCaseSensitive(item, "status"), acd_state_to_str(curr->state));
        cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(item, "int_status"), curr->state);
        if (IS_OUTPUT(curr->type)) {
          uint32_t remaining_sec = get_timer_left_sec(curr);
          cJSON_SetValuestring(cJSON_GetObjectItemCaseSensitive(item, "timer_active"), acd_state_to_str(remaining_sec > 0?ACD_LED_ON:ACD_LED_OFF));
          cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(item, "timer_duration"), remaining_sec);
        } else if ( IS_INPUT(curr->type) && (isMASKSET(curr->flags,CALC_AVERAGE)) ) {
          cJSON *stats = cJSON_GetObjectItemCaseSensitive(item, "stats");
          cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(stats, "avg"), curr->stats.average);
          cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(stats, "max"), curr->stats.max);
          cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(stats, "min"), curr->stats.min);
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
   /*
    item = cJSON_GetObjectItemCaseSensitive(devices_map, "AVG_1");
    cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(item, "value"), acddata->sensorMetrics.ph_daily.average);
    cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(item, "max_value"), acddata->sensorMetrics.ph_daily.max);
    cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(item, "min_value"), acddata->sensorMetrics.ph_daily.min);

    item = cJSON_GetObjectItemCaseSensitive(devices_map, "AVG_2");
    cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(item, "value"), acddata->sensorMetrics.orp_daily.average);
    cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(item, "max_value"), acddata->sensorMetrics.orp_daily.max);
    cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(item, "min_value"), acddata->sensorMetrics.orp_daily.min);
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
    snprintf(json_buffer, MAX_JSON_BUFFER_SIZE, "%s", JSON_ERROR_OVERFLOW);
  }

  cJSON_Delete(root);
  return json_buffer;

  //cJSON_PrintUnformatted(devices);
}




cJSON* get_loglevel_item(int level) {
    cJSON *item = cJSON_CreateObject();
    if (item == NULL) return NULL;

    cJSON_AddStringToObject(item, "name", log_priority_to_str(level));
    cJSON_AddNumberToObject(item, "id", level);
    cJSON_AddBoolToObject(item, "set", (get_loglevel() == level));

    return item;
}


bool build_acdmanager_json(struct aquachemdata *acddata, char *buffer, size_t buf_size)
{
  bool success = false;

  cJSON *root = cJSON_CreateObject();
  if (!root) return false;

  cJSON_AddStringToObject(root, "type", "acdmanager");
  cJSON_AddStringToObject(root, "name", AQUACHEMD_SHORT_NAME);
  cJSON_AddStringToObject(root, "fullname", AQUACHEMD_NAME);
  cJSON_AddStringToObject(root, "version", AQUACHEMD_VERSION);
  cJSON_AddBoolToObject(root, "daemonized", is_running_under_systemd());
 
  
  cJSON *levels_array = cJSON_AddArrayToObject(root, "loglevels");
  cJSON_AddItemToArray(levels_array, get_loglevel_item(LOG_DEBUG));
  cJSON_AddItemToArray(levels_array, get_loglevel_item(LOG_INFO));
  cJSON_AddItemToArray(levels_array, get_loglevel_item(LOG_NOTICE));
  cJSON_AddItemToArray(levels_array, get_loglevel_item(LOG_WARNING));
  cJSON_AddItemToArray(levels_array, get_loglevel_item(LOG_ERR));


  if (cJSON_PrintPreallocated(root, buffer, (int)buf_size, 0)) {
    success = true;
  } else {
    snprintf(buffer, buf_size, "%s", JSON_ERROR_OVERFLOW);
    success = false;
  }

  cJSON_Delete(root);
  return success;
}

/**
 * Built for speed over using cJSON, 
 * simply replace any non-printable char with a space
 */
int json_chars(char *dest, const char *src, size_t dest_len, size_t src_len) {
    if (dest_len == 0) return 0;
    
    size_t i;
    // We must leave at least 1 byte for the null terminator
    size_t limit = (src_len < dest_len - 1) ? src_len : (dest_len - 1);

    for (i = 0; i < limit; i++) {
        unsigned char c = (unsigned char)src[i];

        // Replace JSON-breaking characters and non-printables
        if (c == '"' || c == '\\' || c < 32 || c > 126) {
            dest[i] = ' ';
        } else {
            dest[i] = src[i];
        }
    }

    dest[i] = '\0'; // Guaranteed safe termination
    return (int)i;
}
/**
 * Wraps a raw log string into a JSON object for WebSocket transmission.
 * Pattern: [Buffer Pointer], [Buffer Size], [Data Inputs...]
 */
int build_logmsg_json(char *buffer, size_t buf_size, int loglevel, const char *src_msg, size_t src_len) {
    // 1. Write the JSON header
    // Use snprintf to prevent overflow; it returns the number of chars that WOULD be written
    int written = snprintf(buffer, buf_size, "{\"logmsg\":\"%-8.8s", log_priority_to_str(loglevel));
    
    if (written < 0 || (size_t)written >= buf_size) {
        return -1; // Buffer is too small even for the header
    }

    size_t current_len = (size_t)written;

    // 2. Escape and add the source message body
    // Reserve 3 bytes for: " } \0
    if (current_len + 3 < buf_size) {
        current_len += json_chars(buffer + current_len, src_msg, (buf_size - current_len - 3), src_len);
    }
    
    // 3. Close the JSON object
    int tail = snprintf(buffer + current_len, buf_size - current_len, "\"}");
    
    if (tail > 0) {
        current_len += (size_t)tail;
    }

    return (int)current_len;
}

#define MAX_PUMPS 5

struct pump_stats
{
  char pump_id[32];
  char pump_name[32];
  uint32_t total_seconds;
  float total_ml;
};

// Helper to find or create a slot in our stats array
struct pump_stats *_find_pump_stats(struct pump_stats stats[], int *count, const char *id, const char *name)
{
  for (int i = 0; i < *count; i++)
  {
    if (strcmp(stats[i].pump_id, id) == 0)
    {
      // If the existing entry is "unknown", but we just found a valid name, update it!
      if (strcmp(stats[i].pump_name, "unknown") == 0 && strcmp(name, "unknown") != 0)
      {
        strncpy(stats[i].pump_name, name, sizeof(stats[i].pump_name) - 1);
        stats[i].pump_name[sizeof(stats[i].pump_name) - 1] = '\0'; // Ensure null-termination
      }
      return &stats[i];
    }
  }
  
  // If the pump wasn't tracked yet, create a new slot
  if (*count < MAX_PUMPS)
  {
    strncpy(stats[*count].pump_id, id, sizeof(stats[*count].pump_id) - 1);
    stats[*count].pump_id[sizeof(stats[*count].pump_id) - 1] = '\0';

    strncpy(stats[*count].pump_name, name, sizeof(stats[*count].pump_name) - 1);
    stats[*count].pump_name[sizeof(stats[*count].pump_name) - 1] = '\0';

    return &stats[(*count)++];
  }
  return NULL;
}

/**
 * Retrieves pump dosing summaries and optionally a detailed history from the systemd journal.
 * @param days      Number of days to look back.
 * @param detailed  If true, includes the "history" array of individual events.
 * @param buffer    Pointer to the char array where the JSON string will be stored.
 * @param buf_size  Size of the provided buffer.
 * @return          true if successful and fit in buffer, false otherwise.
 */
bool get_pump_summaries_json(int days, bool detailed, char *buffer, size_t buf_size)
{
  sd_journal *j;
  struct pump_stats stats[MAX_PUMPS] = {0};
  int pump_count = 0;
  bool success = false;

  // 1. Initialize cJSON root
  cJSON *root = cJSON_CreateObject();
  if (!root)
    return false;

  cJSON_AddStringToObject(root, "type", "dose_history");
  cJSON *history = detailed ? cJSON_AddArrayToObject(root, "history") : NULL;

  // 2. Calculate start time in microseconds
  struct timeval tv;
  gettimeofday(&tv, NULL);
  uint64_t since_usec = ((uint64_t)tv.tv_sec * 1000000) - ((uint64_t)days * 24 * 3600 * 1000000);

  // 3. Open Journal and Filter
  if (sd_journal_open(&j, SD_JOURNAL_LOCAL_ONLY) < 0)
  {
    cJSON_Delete(root);
    return false;
  }

  // Filter by our specific 128-bit Pump Event Message ID
  sd_journal_add_match(j, "MESSAGE_ID=" SD_PUMP_EVENT_ID, 0);
  sd_journal_seek_realtime_usec(j, since_usec);

  // 4. Iterate through log entries
  SD_JOURNAL_FOREACH(j)
  {
    const char *data;
    size_t len;
    char p_id[32] = "unknown";
    char p_name[32] = "unknown";
    char p_type[32] = "unknown";
    uint32_t sec = 0;
    float ml = 0.0;
    float reading = 0.0;

    // Parse PUMP_ID
    if (sd_journal_get_data(j, "PUMP_ID", (const void **)&data, &len) >= 0)
    {
      const char *val = strchr(data, '=');
      if (val)
        snprintf(p_id, sizeof(p_id), "%s", val + 1);
    }

    // Parse PUMP_NAME
    if (sd_journal_get_data(j, "PUMP_NAME", (const void **)&data, &len) >= 0)
    {
      const char *val = strchr(data, '=');
      if (val)
        snprintf(p_name, sizeof(p_name), "%s", val + 1);
    }

    // Parse PUMP_TYPE
    if (sd_journal_get_data(j, "PUMP_TYPE", (const void **)&data, &len) >= 0)
    {
      const char *val = strchr(data, '=');
      if (val)
        snprintf(p_type, sizeof(p_type), "%s", val + 1);
    }

    // Parse SENSOR_VAL
    if (sd_journal_get_data(j, "SENSOR_VAL", (const void **)&data, &len) >= 0)
    {
      const char *val = strchr(data, '=');
      if (val)
        reading = strtof(val + 1, NULL);
    }

    // Parse RUNTIME_SEC
    if (sd_journal_get_data(j, "RUNTIME_SEC", (const void **)&data, &len) >= 0)
    {
      const char *val = strchr(data, '=');
      if (val)
        sec = (uint32_t)strtoul(val + 1, NULL, 10);
    }

    // Parse DOSE_ML
    if (sd_journal_get_data(j, "DOSE_ML", (const void **)&data, &len) >= 0)
    {
      const char *val = strchr(data, '=');
      if (val)
        ml = strtof(val + 1, NULL);
    }

    // Update Totals and History within allocation limits
    struct pump_stats *p = _find_pump_stats(stats, &pump_count, p_id, p_name);
    if (p)
    {
      p->total_seconds += sec;
      p->total_ml += ml;

      // FIX: Record individual event ONLY if the pump fits into tracked allocations
      if (detailed && history)
      {
        uint64_t timestamp;
        sd_journal_get_realtime_usec(j, &timestamp);
        time_t epoch = timestamp / 1000000;
        char time_buf[26];
        ctime_r(&epoch, time_buf);
        time_buf[24] = '\0'; // Remove trailing newline

        cJSON *event = cJSON_CreateObject();
        if (event)
        {
          cJSON_AddStringToObject(event, "timestamp", time_buf);
          cJSON_AddStringToObject(event, "pump_id", p_id);
          cJSON_AddStringToObject(event, "pump_name", p_name);
          cJSON_AddStringToObject(event, "pump_type", p_type);
          cJSON_AddNumberToObject(event, "reading", reading);
          cJSON_AddNumberToObject(event, "seconds", sec);
          cJSON_AddNumberToObject(event, "ml", ml);
          cJSON_AddItemToArray(history, event);
        }
      }
    }
  }
  sd_journal_close(j);

  // 5. Add Totals to the JSON
  cJSON *totals_arr = cJSON_AddArrayToObject(root, "totals");
  for (int i = 0; i < pump_count; i++)
  {
    cJSON *item = cJSON_CreateObject();
    if (item)
    {
      cJSON_AddStringToObject(item, "pump_id", stats[i].pump_id);
      cJSON_AddStringToObject(item, "pump_name", stats[i].pump_name);
      cJSON_AddNumberToObject(item, "sum_runtime_s", stats[i].total_seconds);
      cJSON_AddNumberToObject(item, "sum_dose_ml", stats[i].total_ml);
      cJSON_AddItemToArray(totals_arr, item);
    }
  }

  // 6. Print to the pre-allocated buffer
  if (cJSON_PrintPreallocated(root, buffer, (int)buf_size, 0))
  {
    success = true;
  }
  else
  {
    snprintf(buffer, buf_size, "%s", JSON_ERROR_OVERFLOW);
    success = false;
  }

  cJSON_Delete(root);
  return success;
}




