



#include "aquachemd.h"
#include "utils.h"
#include "acd_timer.h"
#include "net_services.h"

bool set_cond_state(struct aquachemdata *acdata, acd_key_t *cond, acd_state_t state);
bool set_key_state(struct aquachemdata *acdata, acd_key_t *key, acd_state_t state);

void check_master(struct aquachemdata *acdata);


acd_scope_t check_master_action(struct aquachemdata *acdata) {
  if (acdata->keys->scope == ACD_ACTION_BLOCK || acdata->keys->state == ACD_LED_OFF){
    return ACD_ACTION_BLOCK;
  } else if (acdata->keys->scope == ACD_ACTION_LIMIT){
    return ACD_ACTION_LIMIT;
  }
  return ACD_ACTION_ALLOW;
}

void sensor_read_error(struct aquachemdata *acddata, acd_key_t *key) {
  if (key->err_cnt++ >= SENSOR_FAULT_THRESHOLD) {
    LOG(LOG_ERR, "Sensor %s too many read errors, removing from rotation", key->label);
    //LOG(LOG_WARNING, "Add code to remove key in sensor_error(), will need to also modify main function for loop since key->next will be null on return", key->label);
    setMASK(key->flags,  ACD_FLAG_FAULTED);
    //SET_IF_CHANGED(key->state, ACD_LED_DISABLED, acddata->is_dirty);
    set_key_state(acddata, key, ACD_LED_DISABLED);
    key->err_cnt = 0;
  } else {
    //SET_IF_CHANGED(key->state, ACD_LED_OFF, acddata->is_dirty);
    set_key_state(acddata, key, ACD_LED_OFF);
  }
}


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
    LOG(LOG_NOTICE, "Dosing time calculated as %s for %d(s) (pH %.2f)",key->label, runtime, current_ph);
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
  ASSIGN_IF_CHANGED(key->state , ACD_LED_ON, acdata->is_dirty, key->is_dirty);
  
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
    LOG_PUMP_EVENT(key, actual_runtime, key->value, dose_ml);
    post_dosing_event(key, actual_runtime, dose_ml);
  }
  
  ASSIGN_IF_CHANGED(key->state, ACD_LED_ENABLED, acdata->is_dirty, key->is_dirty);
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

  LOG(LOG_DEBUG, "Request to set %s to %s (current state: %s)", key->label, acd_state_to_str(state), acd_state_to_str(key->state));

  if (state == key->state) {
    LOG(LOG_DEBUG, "%s is already %s, no state change needed", key->label, acd_state_to_str(state));
    return true;
  }

  // Bunch of logic for different key states.
  switch(key->type) {
    case ACD_TYPE_MASTER:
      if (key->state == ACD_LED_OFF && state == ACD_LED_ON) { //if we are off, use the on state as enabled.
        ASSIGN_IF_CHANGED(key->state , ACD_LED_ENABLED, acdata->is_dirty, key->is_dirty);
        check_master(acdata); // Set things to enabled
      } else if (key->state == ACD_LED_DISABLED && state == ACD_LED_ON) { // if disabled, can't turn on
        LOG(LOG_WARNING, "%s is %s, can't turn %s", key->label, acd_state_to_str(key->state), acd_state_to_str(state));
        return false;
      } else if (state == ACD_LED_OFF) { // Turn sensors off.
        ASSIGN_IF_CHANGED(key->state , state, acdata->is_dirty, key->is_dirty);
        check_master(acdata); // Set things to disabled
      } else {
        ASSIGN_IF_CHANGED(key->state , state, acdata->is_dirty, key->is_dirty);
      }
      break;
    case ACD_TYPE_GPIO_PMP:
    case ACD_TYPE_EZO_PMP:
      // If we are in an off state, don't disable.  This is when pump is off and condition is not met, leave pump on off state.
      if (key->state == ACD_LED_OFF && (state == ACD_LED_DISABLED )) {
        LOG(LOG_WARNING, "%s is %s, can't turn %s", key->label, acd_state_to_str(key->state), acd_state_to_str(state));
        return false;
      }

      // Master is blocking any enable or on states
      if (acdata->keys->scope != ACD_ACTION_ALLOW && (state == ACD_LED_ON || state == ACD_LED_ENABLED )) {
        LOG(LOG_WARNING, "Master is in %s but with scope %s, can't turn %s to %s, setting to %s", acd_state_to_str(acdata->keys->state), acd_scope_to_str(acdata->keys->scope), key->label, acd_state_to_str(state), acd_state_to_str(ACD_LED_DISABLED));
        // We can turn to disable though.
        ASSIGN_IF_CHANGED(key->state , ACD_LED_DISABLED, acdata->is_dirty, key->is_dirty); 
        return false;
      }

      //if we are off, use the on state as enabled.
      if (key->state == ACD_LED_OFF && (state == ACD_LED_ON || state == ACD_LED_ENABLED)) {
        if (acdata->keys->state == ACD_LED_OFF) {
          ASSIGN_IF_CHANGED(key->state , ACD_LED_DISABLED, acdata->is_dirty, key->is_dirty); // Master is off, can only set to disabled.
        } else {
          ASSIGN_IF_CHANGED(key->state , ACD_LED_DISABLED, acdata->is_dirty, key->is_dirty); // Master is has ACD_ACTION_BLOCK | ACD_ACTION_LIMIT, can only set disabled.
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
        ASSIGN_IF_CHANGED(key->state , ACD_LED_OFF, acdata->is_dirty, key->is_dirty);
       } else if (key->state == ACD_LED_DISABLED && state == ACD_LED_OFF) {
        ASSIGN_IF_CHANGED(key->state , ACD_LED_OFF, acdata->is_dirty, key->is_dirty);
      } else {
        //SET_IF_CHANGED(key->state , state, acdata->is_dirty);
        LOG(LOG_WARNING, "%s is %s, can't turn %s", key->label, acd_state_to_str(key->state), acd_state_to_str(state));
        return false;
      }
      break;
    case ACD_TYPE_MQTT_COND:
    case ACD_TYPE_GPIO_COND:
      // The only time we "request" a state change on a condition is if it's from DELAY to ON.
      if (state == ACD_LED_DELAY || isMASKSET(key->flags, DELAY_ACTIVE)) {
        // Make sure the condition is still met.
        set_cond_state(acdata, key, key->met?ACD_LED_ON:ACD_LED_OFF);
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

/*
void OLD_check_master_OLD(struct aquachemdata *acdata) {
  acd_key_t *failed_condition = NULL;
  //acd_action_t action = ACD_ACTION_ALLOW;

  // Turn everything to disabled if master if off
  if ( acdata->keys->state == ACD_LED_OFF ) {
    for (acd_key_t *curr = acdata->keys->next; curr != NULL; curr = curr->next) {
      if (!IS_CONDITION(curr->type) && curr->state != ACD_LED_OFF ) {
        set_key_state(acdata, curr, ACD_LED_DISABLED);
        //LOG(LOG_ERR, "State Manager - set %s to disable\n",curr->label);
      } else {
        //LOG(LOG_ERR, "State Manager - leave %s at %s\n",curr->label, acd_state_to_str(curr->state));
      }
    }
    return;
  }

  // Check for any conditions that are not met.
  acdata->keys->scope = ACD_ACTION_ALLOW; // Reset to good, below will set to bad.
  for (acd_key_t *curr = acdata->keys->next; curr != NULL; curr = curr->next) {
    if (IS_CONDITION(curr->type)) {LOG(LOG_INFO, "State Manager - Condition %s, scope %s, %s",curr->label, acd_scope_to_str(curr->scope), curr->met?"Safe":"Not Safe");}
    if (IS_CONDITION(curr->type) && curr->met == false) {
      if (curr->scope == ACD_ACTION_BLOCK) {
        failed_condition = curr;
        acdata->keys->scope = ACD_ACTION_BLOCK;
      } else if (curr->scope == ACD_ACTION_LIMIT && acdata->keys->scope != ACD_ACTION_BLOCK) {
        // Don't set to limit if already block
        acdata->keys->scope = ACD_ACTION_LIMIT; 
      }
      LOG(LOG_INFO,"Condition %s not met",curr->label);
    }
  }

  LOG(LOG_INFO, "State Manager - Master actions = %s",acdata->keys->scope==ACD_ACTION_ALLOW?"Allow":(acdata->keys->scope==ACD_ACTION_LIMIT?"Limit":"Block") );

  // failed condition LOCAL scope blocks outputs but not inputs. Master set to limit.
  // failed condition GLOBAL scope blocks everything inputs & outputs except input/sensor scope of local.
  // sensor/input scope local = keep reading on failed global condition (ie MQTT external sensor, or maybe filter pressure)

  // Set master status depening on any conditions that were not met.
  if (failed_condition == NULL) {
    SET_IF_CHANGED(acdata->keys->state, ACD_LED_ON, acdata->is_dirty);
  } else {
    SET_IF_CHANGED(acdata->keys->state, ACD_LED_ENABLED, acdata->is_dirty);
  }

  for (acd_key_t *curr = acdata->keys->next; curr != NULL; curr = curr->next) {
    if (IS_OUTPUT(curr->type)) {
      // turn off any outputs on any conditions that are not met
      if ( (failed_condition != NULL || acdata->keys->scope == ACD_ACTION_LIMIT) && curr->state == ACD_LED_ON) {
        turn_pump_off(acdata, curr);
        if (failed_condition != NULL)
          LOG(LOG_INFO, "State Manager - Condition %s not safe!, %s is on turning off", failed_condition->label, curr->label);
        else
          LOG(LOG_INFO, "State Manager - Condition not safe!, %s is on turning off", curr->label);
      }

      // Set the outputs to disabled or enabled depending on condition(s)
      if (failed_condition != NULL || acdata->keys->scope == ACD_ACTION_LIMIT ) {
        if (SET_IF_CHANGED(curr->state, ACD_LED_DISABLED, acdata->is_dirty)) {
          LOG(LOG_INFO, "State Manager - Set %s input to %s (scope %s)",acd_state_to_str(curr->state), curr->label, acd_scope_to_str(curr->scope));
        }
      } else if (failed_condition == NULL && curr->state != ACD_LED_OFF && curr->state != ACD_LED_ON) {
        if (SET_IF_CHANGED(curr->state, ACD_LED_ENABLED, acdata->is_dirty)) {
          LOG(LOG_INFO, "State Manager - Set %s input to %s (scope %s)",acd_state_to_str(curr->state), curr->label, acd_scope_to_str(curr->scope));
        }
      }
    } else if (IS_INPUT(curr->type)) {
     if (acdata->keys->scope == ACD_ACTION_ALLOW || acdata->keys->scope == ACD_ACTION_LIMIT) {
        // ACD_ACTION_ALLOW
        if (SET_IF_CHANGED(curr->state, ACD_LED_ENABLED, acdata->is_dirty)) {
          LOG(LOG_INFO, "State Manager - Set %s input to %s (scope %s)",acd_state_to_str(curr->state), curr->label, acd_scope_to_str(curr->scope));
        }
     } else {
       // ACD_ACTION_LIMIT or ACD_ACTION_BLOCK
       if (curr->scope == ACD_SCOPE_GLOBAL) {
          if (SET_IF_CHANGED(curr->state, ACD_LED_DISABLED, acdata->is_dirty)) {
            LOG(LOG_INFO, "State Manager - Set %s input to %s (scope %s)",acd_state_to_str(curr->state), curr->label, acd_scope_to_str(curr->scope));
          }
        } else {
          if (SET_IF_CHANGED(curr->state, ACD_LED_ENABLED , acdata->is_dirty)) {
            LOG(LOG_INFO, "State Manager - Set %s input to %s (scope %s)",acd_state_to_str(curr->state), curr->label, acd_scope_to_str(curr->scope));
          }
        }
     } 
    }
  }
}
*/


void check_master(struct aquachemdata *acdata) {
  acd_key_t *failed_condition = NULL;
  //acd_action_t action = ACD_ACTION_ALLOW;

  // Turn everything to disabled if master if off
  if ( acdata->keys->state == ACD_LED_OFF ) {
    for (acd_key_t *curr = acdata->keys->next; curr != NULL; curr = curr->next) {
      if (!IS_CONDITION(curr->type) && curr->state != ACD_LED_OFF /*&& curr->scope != ACD_SCOPE_LOCAL*/) {
        set_key_state(acdata, curr, ACD_LED_DISABLED);
        //LOG(LOG_ERR, "State Manager - set %s to disable\n",curr->label);
      } else {
        //LOG(LOG_ERR, "State Manager - leave %s at %s\n",curr->label, acd_state_to_str(curr->state));
      }
    }
    return;
  }

  // Check for any conditions that are not met.
  acdata->keys->scope = ACD_ACTION_ALLOW; // Reset to good, below will set to bad.
  for (acd_key_t *curr = acdata->keys->next; curr != NULL; curr = curr->next) {
    if (IS_CONDITION(curr->type)) {LOG(LOG_INFO, "State Manager - Condition %s, scope %s, %s",curr->label, acd_scope_to_str(curr->scope), curr->met?"Safe":"Not Safe");}
    //if (IS_CONDITION(curr->type) && curr->met == false) {
    if (IS_CONDITION(curr->type) && (curr->met == false || curr->state == ACD_LED_DELAY)) {
      if (curr->scope == ACD_ACTION_BLOCK) {
        failed_condition = curr;
        acdata->keys->scope = ACD_ACTION_BLOCK;
      } else if (curr->scope == ACD_ACTION_LIMIT && acdata->keys->scope != ACD_ACTION_BLOCK) {
        // Don't set to limit if already block
        acdata->keys->scope = ACD_ACTION_LIMIT; 
      }
      LOG(LOG_INFO,"Condition %s not met",curr->label);
    }
  }

  LOG(LOG_INFO, "State Manager - Master actions = %s",acd_scope_to_str(acdata->keys->scope) );

  // failed condition LOCAL scope blocks outputs but not inputs. Master set to limit.
  // failed condition GLOBAL scope blocks everything inputs & outputs except input/sensor scope of local.
  // sensor/input scope local = keep reading on failed global condition (ie MQTT external sensor, or maybe filter pressure)

  // Set master status depening on any conditions that were not met.
  if (failed_condition == NULL) {
    //SET_IF_CHANGED(acdata->keys->state, ACD_LED_ON, acdata->is_dirty);
    set_key_state(acdata, acdata->keys, ACD_LED_ON);
  } else {
    //SET_IF_CHANGED(acdata->keys->state, ACD_LED_ENABLED, acdata->is_dirty);
    set_key_state(acdata, acdata->keys, ACD_LED_ENABLED);
  }

  for (acd_key_t *curr = acdata->keys->next; curr != NULL; curr = curr->next) {
    if (IS_OUTPUT(curr->type)) {
      // turn off any outputs on any conditions that are not met
      if ( (failed_condition != NULL || acdata->keys->scope == ACD_ACTION_LIMIT) && curr->state == ACD_LED_ON) {
        turn_pump_off(acdata, curr);
        if (failed_condition != NULL)
          LOG(LOG_INFO, "State Manager - Condition %s not safe!, %s is on turning off", failed_condition->label, curr->label);
        else
          LOG(LOG_INFO, "State Manager - Condition not safe!, %s is on turning off", curr->label);
      }

      // Set the outputs to disabled or enabled depending on condition(s)
      if (failed_condition != NULL || acdata->keys->scope == ACD_ACTION_LIMIT ) {
        set_key_state(acdata, curr, ACD_LED_DISABLED);
      } else if (failed_condition == NULL && curr->state != ACD_LED_OFF && curr->state != ACD_LED_ON) {
        set_key_state(acdata, curr, ACD_LED_ENABLED);
      }
    } else if (IS_INPUT(curr->type)) {
      if (acdata->keys->scope == ACD_ACTION_ALLOW || acdata->keys->scope == ACD_ACTION_LIMIT) {
        // wrapping the set_key_state with the IF so the device doesn;t flash to enabled then on, if was previously on
        if (curr->state != ACD_LED_ON) {set_key_state(acdata, curr, ACD_LED_ENABLED);}
      } else { // ACD_ACTION_BLOCK
       if (curr->scope == ACD_SCOPE_GLOBAL) {
          set_key_state(acdata, curr, ACD_LED_DISABLED);
        } else {
          // wrapping the set_key_state with the IF so the device doesn;t flash to enabled then on, if was previously on
          if (curr->state != ACD_LED_ON) {set_key_state(acdata, curr, ACD_LED_ENABLED);}
        }
      } 
    }
  }
}


bool set_key_state(struct aquachemdata *acdata, acd_key_t *key, acd_state_t state)
{
  //  Master         off / on / enabled ----> Means a condition is not met.
  //  sensors        on / disabled / enabled (when master turns them on only, then go to on after first reading.
  //. sensors        off when a single poll failed, disabled when taken out of poll cycle.
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
    case ACD_TYPE_EZO_PRS:
    case ACD_TYPE_MQTT_TEMP:
    case ACD_TYPE_D1W_TEMP:
    case ACD_TYPE_SYSFS_VALUE:
    /*
      if (state != ACD_LED_ON && state != ACD_LED_DISABLED) {
        goodState = false;
      }*/
     // Above is accurate for a request, below is accurate for master to change.
      if (state != ACD_LED_ON && state != ACD_LED_DISABLED && state != ACD_LED_ENABLED && state != ACD_LED_OFF) {
        goodState = false;
      }
      if (goodState && acdata->keys->state != ACD_LED_ON && state == ACD_LED_ON && key->scope == ACD_SCOPE_GLOBAL) {
        // If mater was turned off while sampeling we can get the change request here.
        goodState = false;
      }
      break;
    case ACD_TYPE_GPIO_PMP:
    case ACD_TYPE_EZO_PMP:
      if (state != ACD_LED_OFF && state != ACD_LED_ON && state != ACD_LED_ENABLED && state != ACD_LED_DISABLED) {
        goodState = false;
      }
      if (state == ACD_LED_ENABLED && acdata->keys->state != ACD_LED_ON) {
        state = ACD_LED_DISABLED;
      }
      break;
    case ACD_TYPE_NONE:
      goodState = false;
      break;
  }

  if (!goodState) {
    // If we set to delay, we return false, but don;t print the error.
    if (isMASKSET(key->flags, DELAY_ACTIVE) || key->state == ACD_LED_DELAY) {
      return false;
    }
    LOG(LOG_ERR, "Device %s can't be set to %s", key->label, acd_state_to_str(state));
    return false;
  }

  // If being asked to turn on, clear any flags.
  if ( (state == ACD_LED_ON || state == ACD_LED_ENABLED) && isMASKSET(key->flags, ACD_FLAG_FAULTED)) {
    removeMASK(key->flags, ACD_FLAG_FAULTED);
  }

  //return SET_IF_CHANGED(key->state , state, acdata->is_dirty);

  if (ASSIGN_IF_CHANGED(key->state , state, acdata->is_dirty, key->is_dirty)) {
    LOG(LOG_INFO, "State Manager - Set %s to %s (scope %s)",acd_state_to_str(key->state), key->label, acd_scope_to_str(key->scope));
    return true;
  } else {
    LOG(LOG_DEBUG, "State Manager - Request to set same state, ignored %s to %s (scope %s)",acd_state_to_str(key->state), key->label, acd_scope_to_str(key->scope));
    //LOG(LOG_INFO, "State Manager - No change for %s, remains at %s (scope %s)",curr->label, acd_state_to_str(curr->state), acd_scope_to_str(curr->scope));
    return false;
  }
}


bool set_cond_state(struct aquachemdata *acdata, acd_key_t *cond, acd_state_t state)
{
  //  conditions     on / off

  // If condition set to on, but it has a delay, then start the timer.
  if (state == ACD_LED_ON && cond->delay_on > 0) {
    if (cond->state == ACD_LED_OFF) {
      LOG(LOG_NOTICE, "Condition '%s' met, delaying activation %d seconds!",cond->label, cond->delay_on);
      ASSIGN_IF_CHANGED(cond->state , ACD_LED_DELAY, acdata->is_dirty, cond->is_dirty);
      start_delay(acdata, cond, 0, cond->delay_on);
      return false;
    } else if (cond->state == ACD_LED_DELAY || isMASKSET(cond->flags, DELAY_ACTIVE)) {
      if ( get_timer_left_sec(cond) > 0 ) {
        LOG(LOG_WARNING, "State Manager - Request to set '%s' on, but still in delay, ignored!",cond->label);
        return false;
      }
      // Good to fall through to set new ON state.
    }
  } else if (state == ACD_LED_OFF && cond->state == ACD_LED_DELAY) {
    // cancel the delay and fall through to turning off.
    clear_timer(acdata, cond);
  }


  if (state == ACD_LED_ON || state == ACD_LED_OFF ) {
    if (ASSIGN_IF_CHANGED(cond->state , state, acdata->is_dirty, cond->is_dirty)) {
      LOG(LOG_INFO, "State Manager - Condition %s changed to %s",cond->label, acd_state_to_str(cond->state));
    }
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
