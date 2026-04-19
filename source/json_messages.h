#ifndef JSON_MESSAGES_H_
#define JSON_MESSAGES_H_ 

#include "aquachemd.h"

float parse_payload_value(const char *buf, size_t len);
bool parse_json_uri_command(const char *json_str, size_t json_len, char *uri_out, float *val_out);

const char* get_devices_json(struct aquachemdata *acddata);

#endif