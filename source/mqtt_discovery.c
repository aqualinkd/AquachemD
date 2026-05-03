
#include "aquachemd.h"
#include "mqtt_discovery.h"
#include "net_services.h"
#include "utils.h"
#include "config.h"

void publish_mqtt_discovery(struct aquachemdata *acdata, struct mg_connection *nc)
{
  char topic[250];
  LOG(LOG_NOTICE, "Publishing MQTT discovery - NOT IMPLIMENTED\n");

  // First thing we will publish names/labels to aquachemd topics. (only once at startup)

  for (acd_key_t *curr = acdata->keys; curr != NULL; curr = curr->next) { 
    sprintf(topic, "%s/%s/label", _acdconfig_.mqtt_aquachemd_topic, curr->ID);
    send_mqtt(nc, topic, curr->label);
  }
}

