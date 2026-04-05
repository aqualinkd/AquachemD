#ifndef HASSIO_H_
#define HASSIO_H_

#include "aquachemd.h"
#include "mongoose.h"

void publish_mqtt_discovery(struct aquachemdata *acdata, struct mg_connection *nc);

#endif // HASSIO_H_