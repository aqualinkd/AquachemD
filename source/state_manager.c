



#include "aquachemd.h"
#include "utils.h"
#include "acd_timer.h"

bool set_cond_state(struct aquachemdata *acdata, acd_key_t *cond, acd_state_t state);
void set_key_state(struct aquachemdata *acdata, acd_key_t *key, acd_state_t state);

// This is a request from MQTT/WebSocket/Web, NOT for a system change


void turn_pump_on(struct aquachemdata *acdata, acd_key_t *key) {
  
  if (key->type == ACD_TYPE_GPIO_PMP) {
    relay_on(&key->data.gpio);
    key->ison = pump_is_on(&key->data.gpio);
  } else {
    LOG(LOG_ERR, "Add Code in state_manage.c - turn_pump_on()");
  }
  SET_IF_CHANGED(key->state , ACD_LED_ON, acdata->is_dirty);
  start_timer(acdata, key, 0, key->runtime);
}

void turn_pump_off(struct aquachemdata *acdata, acd_key_t *key) {

  if (key->type == ACD_TYPE_GPIO_PMP) {
    relay_off(&key->data.gpio);
    key->ison = pump_is_on(&key->data.gpio);
  } else {
    LOG(LOG_ERR, "Add Code in state_manage.c - turn_pump_off()");
  }
  SET_IF_CHANGED(key->state , ACD_LED_ENABLED, acdata->is_dirty);
  clear_timer(acdata, key);
}

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

bool state_change_request(struct aquachemdata *acdata, acd_key_t *key, acd_state_t state)
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
      } else if (key->state == ACD_LED_DISABLED && state == ACD_LED_ON) { // if disabled, can't turn on
        LOG(LOG_WARNING, "%s is %s, can't turn %s", key->label, acd_state_to_str(key->state), acd_state_to_str(state));
        return false;
      } else {
        SET_IF_CHANGED(key->state , state, acdata->is_dirty);
      }
      break;
    case ACD_TYPE_GPIO_PMP:
    case ACD_TYPE_EZO_PMP:
      // If we are in an off state, don't disable or enable 
      if (key->state == ACD_LED_OFF && (state == ACD_LED_DISABLED )) {
        LOG(LOG_WARNING, "%s is %s, can't turn %s", key->label, acd_state_to_str(key->state), acd_state_to_str(state));
        return false;
      }
      //if we are off, use the on state as enabled.
      if (key->state == ACD_LED_OFF && (state == ACD_LED_ON || state == ACD_LED_ENABLED)) {
        SET_IF_CHANGED(key->state , ACD_LED_ENABLED, acdata->is_dirty);
      } else if (state == ACD_LED_ON) {
        if (key->state == ACD_LED_ENABLED ) {
          turn_pump_on(acdata, key);
        } else {
          LOG(LOG_WARNING, "%s is %s, can't turn %s", key->label, acd_state_to_str(key->state), acd_state_to_str(state));
          return false;
        }
      } else if (key->state == ACD_LED_ON && state == ACD_LED_OFF) {
        turn_pump_off(acdata, key);
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
  if ( acdata->keys->state == ACD_LED_OFF )
    return;

  for (acd_key_t *curr = acdata->keys->next; curr != NULL; curr = curr->next) {
    if (IS_CONDITION(curr->type) && curr->met == false) {
      failed_condition = curr;
      //printf("***** check_master() - failed condition %s\n",failed_condition->label);
      //break;
    }
  }

  if (failed_condition == NULL) {
    SET_IF_CHANGED(acdata->keys->state, ACD_LED_ON, acdata->is_dirty);
  } else {
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
