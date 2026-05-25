

// X-macro config table — one entry per parameter

/**
 * CFG_ENTRY( name, field, default, type, mask, group, ui_metadata )
 * 1. name        : Key in the .conf file
 * 2. field       : Member in struct acdconfig
 * 3. default     : The literal default value
 * 4. type        : Data type (used for SET_VAL_ ## type)
 * 5. mask        : Behavior flags (CFG_READONLY, etc.)
 * 6. bit_flag    : The specific bit/mask to toggle within the 'field' (only used for CFG_BITMASK type)
 * 7. ui_metadata : JSON string for UI dropdowns
 */

CFG_ENTRY( "main_label",             main_label,             "AquachemD",          CFG_STRING,  CFG_FORCE_RESTART,                  0,                NULL )

/* --- 1. GLOBAL SYSTEM & LOGGING --- */
CFG_ENTRY( "listen_address",         listen_address,         "http://0.0.0.0:80",  CFG_STRING,  CFG_GRP_ADVANCED|CFG_FORCE_RESTART, 0,                NULL )
CFG_ENTRY( "log_level",              log_level,              LOG_NOTICE,           CFG_TXT_INT, 0,                                  0,                CFG_V_log_level )
CFG_ENTRY( "mg_log_level",           mg_log_level,           0,                    CFG_INT,     CFG_READONLY|CFG_HIDE,              0,                NULL )

/* --- 2. WEB & DIRECTORIES --- */
CFG_ENTRY( "web_directory",          web_directory,          "/var/www/aquachemd", CFG_STRING,  CFG_GRP_ADVANCED|CFG_READONLY,      0,                NULL )

/* --- 3. MQTT COMMUNICATION --- */
CFG_ENTRY( "mqtt_server",            mqtt_server,            NULL,                 CFG_STRING,  CFG_FORCE_RESTART,                  0,                NULL )
CFG_ENTRY( "mqtt_user",              mqtt_user,              NULL,                 CFG_STRING,  CFG_FORCE_RESTART,                  0,                NULL )
CFG_ENTRY( "mqtt_passwd",            mqtt_passwd,            NULL,                 CFG_STRING,  CFG_FORCE_RESTART|CFG_PASSWD_MASK,  0,                NULL )
CFG_ENTRY( "mqtt_aquachemd_topic",   mqtt_aquachemd_topic,   "aquachemd",          CFG_STRING,  CFG_FORCE_RESTART,                  0,                NULL )
CFG_ENTRY( "mqtt_aqualinkd_topic",   mqtt_aqualinkd_topic,   "aqualinkd",          CFG_STRING,  CFG_FORCE_RESTART,                  0,                NULL )
CFG_ENTRY( "mqtt_discovery_topic",   mqtt_discovery_topic,   "homeassistant",      CFG_STRING,  CFG_FORCE_RESTART,                  0,                NULL )
CFG_ENTRY( "mqtt_discovery_use_mac", mqtt_discovery_use_mac, true,                 CFG_BOOL,    0,                                  0,                CFG_V_BOOL )
CFG_ENTRY( "mqtt_timed_update",      mqtt_timed_update,      true,                 CFG_BOOL,    0,                                  0,                CFG_V_BOOL )
CFG_ENTRY( "mqtt_repost_sensors",    mqtt_repost_sensors,    false,                CFG_BOOL,    0,                                  0,                CFG_V_BOOL )
CFG_ENTRY( "mqtt_discovery_strict_availability",mqtt_strict_avail,false,           CFG_BOOL,    0,                                  0,               CFG_V_BOOL )

/* --- 4. GPIO CONFIGURATION --- */
CFG_ENTRY( "gpio_chip",              gpio_chip,              "/dev/gpiochip0",     CFG_STRING,  CFG_FORCE_RESTART,                  0,                NULL )

/* --- 5. POLLING & CONVERSION --- */
CFG_ENTRY( "sensor_poll_time",       sensor_poll_time,       60,                   CFG_INT,     0,                                  0,                NULL )
CFG_ENTRY( "mqtt_convert_to_degF",   convert_mqtt_temp,      false,                CFG_BOOL,    0,                                  0,                CFG_V_BOOL )

CFG_ENTRY( "ph_reading_temp_min",    ph_reading_temp_min,    1,                    CFG_INT,     0,                                  0,                NULL )
CFG_ENTRY( "ph_reading_temp_max",    ph_reading_temp_max,    60,                   CFG_INT,     0,                                  0,                NULL )

/* --- 6. DOSING RANGES (Restored CFG_CUSTOM) --- */
CFG_ENTRY( "ph_dose_range",          ph_steps,               NULL,                 CFG_CUSTOM,  CFG_MULTIPLE,                       0,                NULL )
CFG_ENTRY( "ph_default_dose_time",   ph_default_dose_time,   20,                   CFG_INT,     0,                                  0,                NULL )

CFG_ENTRY( "orp_dose_range",         orp_steps,              NULL,                 CFG_CUSTOM,  CFG_MULTIPLE,                       0,                NULL )
CFG_ENTRY( "orp_default_dose_time",  orp_default_dose_time,  1500,                 CFG_INT,     0,                                  0,                NULL )

/* --- 7. SECURITY / TLS (Conditional) --- */
#if MG_TLS > 0
CFG_ENTRY( "cert_dir",               cert_dir,               NULL,                 CFG_STRING,  CFG_GRP_ADVANCED|CFG_FORCE_RESTART, 0,                NULL )
#endif

CFG_ENTRY( "post_condition",         post_condition,         true,                 CFG_BOOL,    0,                                  0,                CFG_V_BOOL )
CFG_ENTRY( "temp_compensated_ph",    temp_compensated_ph,    true,                 CFG_BOOL,    0,                                  0,                CFG_V_BOOL )
CFG_ENTRY( "log_zerorun_pump_events",log_zerorun_pump_events,false,                CFG_BOOL,    0,                                  0,                CFG_V_BOOL )



/* --- DYNAMIC MULTIPLE BLOCKS (Sensors, Conditions, Dosers) --- */
CFG_ENTRY( "mqtt_condition_label",         keys,             NULL,                 CFG_STRING,  CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "mqtt_condition_topic",         keys,             NULL,                 CFG_STRING,  CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "mqtt_condition_value",         keys,             NULL,                 CFG_STRING,  CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "mqtt_condition_scope_global",  keys,             NULL,                 CFG_BOOL,    CFG_MULTIPLE|CFG_HIDE,              0,                CFG_V_BOOL )

CFG_ENTRY( "gpio_condition_label",         keys,             NULL,                 CFG_STRING,  CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "gpio_condition_pin",           keys,             NULL,                 CFG_INT,     CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "gpio_condition_pin_mode",      keys,             NULL,                 CFG_STRING,  CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "gpio_condition_required_state",keys,             NULL,                 CFG_BOOL,    CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "gpio_condition_scope_global",  keys,             NULL,                 CFG_BOOL,    CFG_MULTIPLE|CFG_HIDE,              0,                NULL )

CFG_ENTRY( "ph_sensor_label",        keys,                   NULL,                 CFG_STRING,  CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "ph_sensor_type",         keys,                   NULL,                 CFG_STRING,  CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "ph_sensor_address",      keys,                   NULL,                 CFG_HEX,     CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "ph_sensor_scope_global", keys,                   NULL,                 CFG_BOOL,    CFG_MULTIPLE|CFG_HIDE,              0,                NULL )

CFG_ENTRY( "orp_sensor_label",       keys,                   NULL,                 CFG_STRING,  CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "orp_sensor_type",        keys,                   NULL,                 CFG_STRING,  CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "orp_sensor_address",     keys,                   NULL,                 CFG_HEX,     CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "orp_sensor_scope_global",keys,                   NULL,                 CFG_BOOL,    CFG_MULTIPLE|CFG_HIDE,              0,                NULL )

CFG_ENTRY( "prs_sensor_label",       keys,                   NULL,                 CFG_STRING,  CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "prs_sensor_type",        keys,                   NULL,                 CFG_STRING,  CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "prs_sensor_address",     keys,                   NULL,                 CFG_HEX,     CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "prs_sensor_scope_global",keys,                   NULL,                 CFG_BOOL,    CFG_MULTIPLE|CFG_HIDE,              0,                NULL )

CFG_ENTRY( "temp_sensor_label",      keys,                   NULL,                 CFG_STRING,  CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "temp_sensor_type",       keys,                   NULL,                 CFG_STRING,  CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "temp_sensor_address",    keys,                   NULL,                 CFG_HEX,     CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "temp_sensor_topic",      keys,                   NULL,                 CFG_STRING,  CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "temp_sensor_path",       keys,                   NULL,                 CFG_STRING,  CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "temp_sensor_offset",     keys,                   NULL,                 CFG_FLOAT,   CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "temp_sensor_scale",      keys,                   NULL,                 CFG_FLOAT,   CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "temp_sensor_scope_global",keys,                  NULL,                 CFG_BOOL,    CFG_MULTIPLE|CFG_HIDE,              0,                NULL )

CFG_ENTRY( "ph_doser_label",         keys,                   NULL,                 CFG_STRING,  CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "ph_doser_type",          keys,                   NULL,                 CFG_STRING,  CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "ph_doser_pin",           keys,                   NULL,                 CFG_INT,     CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "ph_doser_address",       keys,                   NULL,                 CFG_HEX,     CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "ph_doser_pin_mode",      keys,                   NULL,                 CFG_STRING,  CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "ph_doser_required_state",keys,                   NULL,                 CFG_BOOL,    CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "ph_doser_ml_per_second",  keys,                   NULL,                 CFG_FLOAT,   CFG_MULTIPLE|CFG_HIDE,              0,                NULL )

CFG_ENTRY( "orp_doser_label",        keys,                   NULL,                 CFG_STRING,  CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "orp_doser_type",         keys,                   NULL,                 CFG_STRING,  CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "orp_doser_pin",          keys,                   NULL,                 CFG_INT,     CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "orp_doser_address",      keys,                   NULL,                 CFG_HEX,     CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "orp_doser_pin_mode",     keys,                   NULL,                 CFG_STRING,  CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "orp_doser_required_state",keys,                  NULL,                 CFG_BOOL,    CFG_MULTIPLE|CFG_HIDE,              0,                NULL )
CFG_ENTRY( "orp_doser_ml_per_second", keys,                   NULL,                 CFG_FLOAT,   CFG_MULTIPLE|CFG_HIDE,              0,                NULL )

/* --- JSON Metadata Definitions --- */
#define CFG_V_log_level "[\"DEBUG\",\"INFO\",\"NOTICE\",\"WARNING\",\"ERROR\"]"
#define CFG_V_BOOL "[\"Yes\",\"No\"]"