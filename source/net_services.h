#ifndef NET_SERVICES_H_
#define NET_SERVICES_H_ 

#include "aquachemd.h"

bool start_net_services(struct aquachemdata *acddata);

void send_mqtt(struct mg_connection *nc, const char *toppic, const char *message);

#define URI_LEN 32

#define MQTT_ON "1"
#define MQTT_OFF "0"
#define MQTT_LWM_TOPIC "Alive"

//#define MQTT_TEMP_TOPIC "temp"
//#define MQTT_PH_TOPIC "ph"
//#define MQTT_ORP_TOPIC "orp"

#define CACHE    "Cache-Control: public, max-age=604800, immutable\r\n" // 7 days
#define NO_CACHE "Cache-Control: no-cache, no-store, must-revalidate\r\nPragma: no-cache\r\nExpires: 0\r\n"
#define CONTENT_JSON NO_CACHE"Content-Type: application/json\r\n"
#define CONTENT_JS   NO_CACHE"Content-Type: text/javascript\r\n"
#define CONTENT_TEXT NO_CACHE "Content-Type: text/plain\r\n"

//#define JSON_GOOD_REPLY "{\"status\":\"success\"}"
//#define JSON_ERROR_REPLY "{\"status\":\"error\"}"
//#define JSON_ERROR_UNKNOWN_REQUEST "{\"status\":\"error\",\"message\":\"unknown request\"}"
#define JSON_ERROR_FMT "{\"status\":\"error\",\"message\":\"%s\"}"
#define JSON_GOOD_FMT "{\"status\":\"success\",\"message\":\"%s\"}"

#define GET_RTN_OK "Ok"
#define GET_RTN_UNKNOWN "Unknown command"
#define GET_RTN_NOT_CHANGED "Not Changed"
#define GET_RTN_ERROR "Error"

#endif // NET_SERVICES_H_
