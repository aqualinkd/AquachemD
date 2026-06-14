#ifndef STATE_MANAGER_H_
#define STATE_MANAGER_H_

void set_key_state(struct aquachemdata *acdata, acd_key_t *key, acd_state_t state);
bool state_change_request(struct aquachemdata *acdata, acd_key_t *key, acd_state_t state);
bool state_change_request_extended(struct aquachemdata *acdata, acd_key_t *key, acd_state_t state, uint32_t value);

void check_pump_state(struct aquachemdata *acdata, acd_key_t *key);
void sensor_read_error(struct aquachemdata *acddata, acd_key_t *key);

void devices_emergency_stop();

#endif