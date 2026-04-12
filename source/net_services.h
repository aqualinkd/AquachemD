#ifndef NET_SERVICES_H_
#define NET_SERVICES_H_ 

#include "aquachemd.h"

bool start_net_services(struct aquachemdata *acddata);

void send_mqtt(struct mg_connection *nc, const char *toppic, const char *message);

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

#endif // NET_SERVICES_H_
