


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

/* --- 4. POLLING & CONVERSION --- */
CFG_ENTRY( "sensor_poll_time",       sensor_poll_time,       60,                   CFG_INT,     0,                                  0,                NULL )
CFG_ENTRY( "mqtt_convert_to_degF",   convert_mqtt_temp,      false,                CFG_BOOL,    0,                                  0,                CFG_V_BOOL )

/* --- 5. SECURITY / TLS (Conditional) --- */
#if MG_TLS > 0
CFG_ENTRY( "cert_dir",               cert_dir,               NULL,                 CFG_STRING,  CFG_GRP_ADVANCED|CFG_FORCE_RESTART, 0,                NULL )
#endif

/* --- 6. DEVELOPMENT / TEST --- */
CFG_ENTRY( "test_hex",               test_hex,               0,                    CFG_HEX,     0,                                  0,                NULL )
CFG_ENTRY( "test_float",             test_float,             0,                    CFG_FLOAT,   0,                                  0,                NULL )
CFG_ENTRY( "test_bitmask_N1",        test_bitmask,           0,                    CFG_BITMASK, 0,                                  CFG_GRP_ADVANCED, NULL )
CFG_ENTRY( "test_bitmask_N2",        test_bitmask,           0,                    CFG_BITMASK, 0,                                  CFG_READONLY,     NULL )
CFG_ENTRY( "test_bitmask_N3",        test_bitmask,           0,                    CFG_BITMASK, 0,                                  CFG_HIDE,         NULL )






/* --- JSON Metadata --- */
#define CFG_V_log_level  "[\"DEBUG_SERIAL\", \"DEBUG\", \"INFO\", \"NOTICE\", \"WARNING\", \"ERROR\"]"
#define CFG_V_BOOL       "[\"Yes\", \"No\"]"


