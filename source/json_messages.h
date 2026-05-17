#ifndef JSON_MESSAGES_H_
#define JSON_MESSAGES_H_ 

#include "aquachemd.h"

float parse_payload_value(const char *buf, size_t len);
bool parse_json_uri_command(const char *json_str, size_t json_len, char *uri_out, float *val_out);

const char* get_devices_json(struct aquachemdata *acddata);

bool get_pump_summaries_json(int days, bool detailed, char *buffer, size_t buf_size);
bool build_acdmanager_json(struct aquachemdata *acddata, char *buffer, size_t buf_size);
int build_logmsg_json(char *buffer, size_t buf_size, int loglevel, const char *src_msg, size_t src_len);

#endif