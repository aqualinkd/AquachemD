#ifndef STATE_MANAGER_H_
#define STATE_MANAGER_H_

void set_key_state(struct aquachemdata *acdata, acd_key_t *key, acd_state_t state);
bool state_change_request(struct aquachemdata *acdata, acd_key_t *key, acd_state_t state);


void check_pump_state(struct aquachemdata *acdata, acd_key_t *key);

#endif