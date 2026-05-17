



#include "aquachemd.h"
#include "utils.h"
#include "acd_timer.h"
#include "net_services.h"

bool set_cond_state(struct aquachemdata *acdata, acd_key_t *cond, acd_state_t state);
void set_key_state(struct aquachemdata *acdata, acd_key_t *key, acd_state_t state);

void check_master(struct aquachemdata *acdata);


float get_sensor_value(struct aquachemdata *acdata, acd_type_t type)
{
// ACD_TYPE_EZO_ORP
// ACD_TYPE_EZO_PH
  for (acd_key_t *key = acdata->keys->next; key != NULL; key = key->next) {
    if( key->type == type) {
      return key->value;
    }
  }
  return 0;
}

uint32_t get_ph_dose_time(float current_ph) {
  if (_acdconfig_.ph_step_count <= 0) {
    return _acdconfig_.ph_default_dose_time;
  }
  // Iterate through the ranges defined in config
  for (int i = 0; i < _acdconfig_.ph_step_count; i++) {
    if (current_ph >= _acdconfig_.ph_steps[i].threshold) {
      return _acdconfig_.ph_steps[i].seconds;
    }
  }

  //return 0; // Default if under minimum
  // If we reach here, the pH is lower than the lowest threshold.
  // Return the seconds from the very last entry in the list (the minimum dose).
  return _acdconfig_.ph_steps[_acdconfig_.ph_step_count - 1].seconds;
}


uint32_t get_orp_dose_time(float current_orp) { // Changed name to current_orp for clarity
  if (_acdconfig_.orp_step_count <= 0) {
    return _acdconfig_.orp_default_dose_time;
  }

  // Iterate through ranges (Sorted Low to High: e.g., 600, 650, 700)
  for (int i = 0; i < _acdconfig_.orp_step_count; i++) {
    // If current ORP is BELOW or AT the threshold, we need that dose
    if (current_orp <= _acdconfig_.orp_steps[i].threshold) {
      return _acdconfig_.orp_steps[i].seconds;
    }
  }

  // If we are ABOVE the highest threshold (pool is clean), return the last/lowest dose
  return _acdconfig_.orp_steps[_acdconfig_.orp_step_count - 1].seconds;
}

uint32_t get_pump_runtime(struct aquachemdata *acdata, acd_key_t *key) {
  // Find latest ph and orp values.
  uint32_t runtime;

  if (isMASKSET(key->flags, PH_PUMP)) {
    float current_ph = get_sensor_value(acdata, ACD_TYPE_EZO_PH);
    runtime = get_ph_dose_time(current_ph);
    LOG(LOG_NOTICE, "Dosing time calculated as %s for %d(s) (pH %.1f)",key->label, runtime, current_ph);
    return runtime;
  } else if (isMASKSET(key->flags, ORP_PUMP)) {
    float current_orp = get_sensor_value(acdata, ACD_TYPE_EZO_ORP);
    runtime = get_orp_dose_time(current_orp);
    LOG(LOG_NOTICE, "Dosing time calculated as %s for %d(s) (ORP %.1f)",key->label, runtime, current_orp);
    return runtime;
  }

  LOG(LOG_ERR,"Unable to get pump runtime for %s", key->label);

  return 0;
}

// This is a request from MQTT/WebSocket/Web, NOT for a system change


void turn_pump_on(struct aquachemdata *acdata, acd_key_t *key, uint32_t duration_sec) {
  
  int runtime = duration_sec<=0?get_pump_runtime(acdata,key):duration_sec;

  if (runtime <= 0) {
    LOG(LOG_NOTICE, "Pump %s runtime is %d, not turning on", key->label, runtime);
    if (_acdconfig_.log_zerorun_pump_events) {
      LOG_PUMP_EVENT(key, 0, 0, 0);
      post_dosing_event(key, 0, 0);
    }
    return;
  }

  if (key->type == ACD_TYPE_GPIO_PMP) {
    relay_on(&key->data.gpio);
    key->ison = pump_is_on(&key->data.gpio);
  } else {
    LOG(LOG_ERR, "Add Code in state_manage.c - turn_pump_on()");
  }
  SET_IF_CHANGED(key->state , ACD_LED_ON, acdata->is_dirty);
  
  if (isMASKSET(key->flags, PH_PUMP)) {
    key->value = get_sensor_value(acdata, ACD_TYPE_EZO_PH);
  } else if (isMASKSET(key->flags, ORP_PUMP)) {
    key->value = get_sensor_value(acdata, ACD_TYPE_EZO_ORP);
  }

  start_timer(acdata, key, 0, duration_sec<=0?runtime:duration_sec);
}

void turn_pump_off(struct aquachemdata *acdata, acd_key_t *key) {
  time_t start = get_timer_started_at(key);
  time_t now = time(NULL);

  if (key->type == ACD_TYPE_GPIO_PMP) {
    relay_off(&key->data.gpio);
    key->ison = pump_is_on(&key->data.gpio);
  } else {
    LOG(LOG_ERR, "Add Code in state_manage.c - turn_pump_off()");
  }

  // Calculate actual runtime and log the event
  if (start > 0) {
    uint32_t actual_runtime = (uint32_t)(now - start);
    float dose_ml = actual_runtime * key->flow_rate;
    LOG_PUMP_EVENT(key, actual_runtime, dose_ml, dose_ml);
    post_dosing_event(key, actual_runtime, dose_ml);
  }
  
  SET_IF_CHANGED(key->state, ACD_LED_ENABLED, acdata->is_dirty);
  clear_timer(acdata, key);
  key->value = 0;
}
/*
void turn_pump_off(struct aquachemdata *acdata, acd_key_t *key) {

  time_t start = get_timer_started_at(key);

  if (key->type == ACD_TYPE_GPIO_PMP) {
    relay_off(&key->data.gpio);
    key->ison = pump_is_on(&key->data.gpio);
  } else {
    LOG(LOG_ERR, "Add Code in state_manage.c - turn_pump_off()");
  }

  if (start != NULL) {

  }

  SET_IF_CHANGED(key->state , ACD_LED_ENABLED, acdata->is_dirty);
  clear_timer(acdata, key);
}
*/
void check_pump_state(struct aquachemdata *acdata, acd_key_t *key) {
  /*
  LOG(LOG_NOTICE, "Output %s, GPIO %d is in %d/%s state",key->label,key->data.gpio.pin,pump_is_on(&key->data.gpio),(pump_is_on(&key->data.gpio)?"ON":"OFF"));
  LOG(LOG_NOTICE, "Output %s, GPIO %d cache %d/%s state",key->label,key->data.gpio.pin,key->ison,key->ison?"ON":"OFF");
  LOG(LOG_NOTICE, "Output %s, GPIO %d LED   %d/%s state",key->label,key->data.gpio.pin,key->state,acd_state_to_str(key->state));
  */
  int current = pump_is_on(&key->data.gpio);
  if (current >= 0 && current != key->ison) {
    LOG(LOG_WARNING, "Pump %s changed externally\n", key->label);
    key->ison = current;
    set_key_state(acdata, key, key->ison ? ACD_LED_ON : ACD_LED_ENABLED);
    LOG(LOG_NOTICE, "Output %s, GPIO %d is now %s/%s/%s",key->label,key->data.gpio.pin,(pump_is_on(&key->data.gpio)?"ON":"OFF"),acd_state_to_str(key->state),key->ison?"ON":"OFF");
  }
}

bool _state_change_request(struct aquachemdata *acdata, acd_key_t *key, acd_state_t state, uint32_t value);

bool state_change_request(struct aquachemdata *acdata, acd_key_t *key, acd_state_t state)
{
  return _state_change_request(acdata, key, state, 0);
}

bool state_change_request_extended(struct aquachemdata *acdata, acd_key_t *key, acd_state_t state, uint32_t value)
{
  return _state_change_request(acdata, key, state, value);
}

bool _state_change_request(struct aquachemdata *acdata, acd_key_t *key, acd_state_t state, uint32_t value)
{
  // Remember.
  //  Master         off / on / enabled ----> Means a condition is not met.
  //  sensors        on / disabled
  //  conditions     on / off
  //  output/doser   on / off / enabled / disabled ----> off, system will ignore on messages. (user turns off)
  //
  // condition turned !met ( Update master to disabled and turn everything off/enabled.)
  // last condition met ( Update master to on and turn everything to enabled, only if master is enabled)
  //  master off (turn sensors to disabled)



  // Bunch of logic for different key states.
  switch(key->type) {
    case ACD_TYPE_MASTER:
      if (key->state == ACD_LED_OFF && state == ACD_LED_ON) { //if we are off, use the on state as enabled.
        SET_IF_CHANGED(key->state , ACD_LED_ENABLED, acdata->is_dirty);
        check_master(acdata); // Set things to enabled
      } else if (key->state == ACD_LED_DISABLED && state == ACD_LED_ON) { // if disabled, can't turn on
        LOG(LOG_WARNING, "%s is %s, can't turn %s", key->label, acd_state_to_str(key->state), acd_state_to_str(state));
        return false;
      } else if (state == ACD_LED_OFF) { // Turn sensors off.
        SET_IF_CHANGED(key->state , state, acdata->is_dirty);
        check_master(acdata); // Set things to disabled
      } else {
        SET_IF_CHANGED(key->state , state, acdata->is_dirty);
      }
      break;
    case ACD_TYPE_GPIO_PMP:
    case ACD_TYPE_EZO_PMP:
      // If we are in an off state, don't disable.  This is when pump is off and condition is not met, leave pump on off state.
      if (key->state == ACD_LED_OFF && (state == ACD_LED_DISABLED )) {
        LOG(LOG_WARNING, "%s is %s, can't turn %s", key->label, acd_state_to_str(key->state), acd_state_to_str(state));
        return false;
      }
      //if we are off, use the on state as enabled.
      if (key->state == ACD_LED_OFF && (state == ACD_LED_ON || state == ACD_LED_ENABLED)) {
        if (acdata->keys->state == ACD_LED_OFF) {
          SET_IF_CHANGED(key->state , ACD_LED_DISABLED, acdata->is_dirty); // Master is off, can only set to disabled.
        } else {
          SET_IF_CHANGED(key->state , ACD_LED_ENABLED, acdata->is_dirty); // Master is on, can only set enabled.
        }
      } else if (state == ACD_LED_ON) {
        if (key->state == ACD_LED_ENABLED ) {
          turn_pump_on(acdata, key, value<=0?0:value);
        } else {
          LOG(LOG_WARNING, "%s is %s, can't turn %s", key->label, acd_state_to_str(key->state), acd_state_to_str(state));
          return false;
        }
      } else if (key->state == ACD_LED_ON && state == ACD_LED_OFF) {
        turn_pump_off(acdata, key);
      } else if (key->state == ACD_LED_ENABLED && state == ACD_LED_OFF) {
        SET_IF_CHANGED(key->state , ACD_LED_OFF, acdata->is_dirty);
       } else if (key->state == ACD_LED_DISABLED && state == ACD_LED_OFF) {
        SET_IF_CHANGED(key->state , ACD_LED_OFF, acdata->is_dirty);
      } else {
        //SET_IF_CHANGED(key->state , state, acdata->is_dirty);
        LOG(LOG_WARNING, "%s is %s, can't turn %s", key->label, acd_state_to_str(key->state), acd_state_to_str(state));
        return false;
      }
      break;
    default:
      //SET_IF_CHANGED(key->state , state, acdata->is_dirty);
      LOG(LOG_WARNING, "%s is %s, setting to %s not supported", key->label, acd_state_to_str(key->state), acd_state_to_str(state));
      return false;
      break;
  }

  LOG(LOG_DEBUG, "%s set to %s", key->label, acd_state_to_str(key->state));

  return true;
}


void check_master(struct aquachemdata *acdata) {
  acd_key_t *failed_condition = NULL;
//printf("***** check_master()\n");
  if ( acdata->keys->state == ACD_LED_OFF ) {
    for (acd_key_t *curr = acdata->keys->next; curr != NULL; curr = curr->next) {
      if (!IS_CONDITION(curr->type) && curr->state != ACD_LED_OFF)
        set_key_state(acdata, curr, ACD_LED_DISABLED);
    }
    return;
  }

  for (acd_key_t *curr = acdata->keys->next; curr != NULL; curr = curr->next) {
    if (IS_CONDITION(curr->type) && curr->met == false) {
      failed_condition = curr;
      //printf("***** check_master() - failed condition %s\n",failed_condition->label);
      //break;
    }
  }

  if (failed_condition == NULL) {
    //printf("***** check_master() - set ON\n");
    SET_IF_CHANGED(acdata->keys->state, ACD_LED_ON, acdata->is_dirty);
  } else {
    //printf("***** check_master() - set ENABLED\n");
    SET_IF_CHANGED(acdata->keys->state, ACD_LED_ENABLED, acdata->is_dirty);
  }

  for (acd_key_t *curr = acdata->keys->next; curr != NULL; curr = curr->next) {
    if (IS_OUTPUT(curr->type)) {
      //printf("***** check_master() - set output %s\n",curr->label);
      if (failed_condition != NULL && curr->state == ACD_LED_ON) {
        // NEED TO TURN OFF DOSER
        LOG(LOG_NOTICE, "Condition %s failed, %s is on turning off", failed_condition->label, curr->label);
        LOG(LOG_ERR, "ADD CODE TO HANDLE THIS");
        turn_pump_off(acdata, curr);
      }

      if (failed_condition == NULL && curr->state != ACD_LED_OFF && curr->state != ACD_LED_ON) {
        SET_IF_CHANGED(curr->state, ACD_LED_ENABLED, acdata->is_dirty);
      } else if (failed_condition != NULL && curr->state != ACD_LED_OFF) {
        SET_IF_CHANGED(curr->state, ACD_LED_DISABLED, acdata->is_dirty);
      }
    } else if (IS_INPUT(curr->type)) {
      if (failed_condition == NULL)
        SET_IF_CHANGED(curr->state, ACD_LED_ON, acdata->is_dirty);
      else
        SET_IF_CHANGED(curr->state, ACD_LED_DISABLED, acdata->is_dirty);
    }
  }
}

/*
void master_state_change(struct aquachemdata *acdata, acd_state_t new_state) {

  SET_IF_CHANGED(acdata->keys->state, new_state, acdata->is_dirty);

  acd_state_t new_sensor_state = ACD_LED_ENABLED;

  if (acdata->keys->state == ACD_LED_OFF) {
    new_sensor_state = ACD_LED_DISABLED;
  } else if (acdata->keys->state == ACD_LED_ON) {
    new_sensor_state = ACD_LED_ENABLED;
  } else if (acdata->keys->state == ACD_LED_ENABLED) {
    new_sensor_state = ACD_LED_DISABLED;
  }

  LOG(LOG_DEBUG,"AquachemD is %s, turning sensors to %s\n",acd_state_to_str(acdata->keys->state), acd_state_to_str(new_sensor_state));
  for (acd_key_t *curr = acdata->keys->next; curr != NULL; curr = curr->next) {
    if (!IS_CONDITION(curr->type)) {
      set_key_state(acdata, curr, new_sensor_state);
    }
  }
}
*/
void set_key_state(struct aquachemdata *acdata, acd_key_t *key, acd_state_t state)
{
  //  Master         off / on / enabled ----> Means a condition is not met.
  //  sensors        on / disabled
  //  doser (sensor) on / off / disabled / enabled

  bool goodState = true;

  switch(key->type){
    case ACD_TYPE_MASTER:
      if (state != ACD_LED_OFF && state != ACD_LED_ON && state != ACD_LED_ENABLED) {goodState = false;}
      break;
    case ACD_TYPE_MQTT_COND:
    case ACD_TYPE_GPIO_COND:
      goodState = set_cond_state(acdata,  key, state);
      break;
    case ACD_TYPE_EZO_PH:
    case ACD_TYPE_EZO_ORP:
    case ACD_TYPE_EZO_TEMP:
    case ACD_TYPE_MQTT_TEMP:
    case ACD_TYPE_D1W_TEMP:
      if (state != ACD_LED_ON && state != ACD_LED_DISABLED) {goodState = false;}
      break;
    case ACD_TYPE_GPIO_PMP:
    case ACD_TYPE_EZO_PMP:
      if (state != ACD_LED_OFF && state != ACD_LED_ON && state != ACD_LED_ENABLED && state != ACD_LED_DISABLED) {goodState = false;}
      if (state == ACD_LED_ENABLED && acdata->keys->state != ACD_LED_ON) {state = ACD_LED_DISABLED;}
      break;
    default:
      goodState = false;
      break;
  }

  if (!goodState) {
    LOG(LOG_ERR, "Device %s can't be set to %s", key->label, acd_state_to_str(state));
    return;
  }
  SET_IF_CHANGED(key->state , state, acdata->is_dirty);
}


bool set_cond_state(struct aquachemdata *acdata, acd_key_t *cond, acd_state_t state)
{
  //  conditions     on / off

  if (state == ACD_LED_ON || state == ACD_LED_OFF ) {
    SET_IF_CHANGED(cond->state , state, acdata->is_dirty);
    //if ( state == ACD_LED_OFF && acdata->keys->state == ACD_LED_ON) {
    //  master_state_change(acdata, ACD_LED_ENABLED);
    //}
  } else {
     //LOG(LOG_ERR, "Condition %s can't be set to %s", cond->label, acd_state_to_str(state));
     return false;
  }

  check_master(acdata);
  return true;
}
