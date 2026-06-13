/****************

Make sure to have the below in /boot/firmware/config.txt to enable I2C on Raspberry Pi:

dtparam=i2c_arm=on
dtparam=i2c_arm_baudrate=100000

# To enable 1 wire
dtoverlay=w1-gpio

# Install i2c-tools for testing and debugging:
sudo apt install i2c-tools

# Load i2c-dev module
sudo modprobe i2c-dev

# Verify /dev/i2c-1 appears
ls /dev/i2c*

# Make it survive reboot (We shouldn't need to do this, but testing we do)
echo "i2c-dev" | sudo tee /etc/modules-load.d/i2c.conf

Usage:
  ./aquachemd                       - normal mode, loop reading sensors
  ./aquachemd scan                  - scan I2C bus and identify devices, scan GPIO chips
  ./aquachemd deepscan              - scan I2C bus and identify devices, scan GPIO chips with line details
  ./aquachemd calibrate mid         - calibrate pH mid-point (7.00)
  ./aquachemd calibrate low         - calibrate pH low-point (4.00)
  ./aquachemd calibrate high        - calibrate pH high-point (10.00)
  ./aquachemd calibrate orp <mv>    - calibrate ORP at specified mV value

*/

#define _POSIX_C_SOURCE 200809L // for clock_gettime and clock_nanosleep
#include <time.h>

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "version.h"
#include "aquachemd.h"
#include "utils.h"
#include "ezo.h"
#include "1wire.h"
#include "sysfs.h"
#include "utils.h"
#include "config.h"
#include "net_services.h"
#include "acd_timer.h"
#include "gpio_monitor.h"
#include "state_manager.h"
#include "sensor_stats.h"

#ifdef USE_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

#ifdef WITH_GPIOD
#include "gpio.h"
#endif

bool start_upgrade();

//void setKeyLed(struct aquachemdata *acdata, acd_key_t *key, acd_state_t state);

void intHandler(int sig_num)
{

  if (sig_num == SIGRUPGRADE) {
    LOG(LOG_NOTICE, "Starting upgrade\n");
    if (! start_upgrade()) {
      LOG(LOG_ERR, "%s upgrade failed!\n",AQUACHEMD_SHORT_NAME);
    }
    return; // Let the upgrade process terminate us.
  }

  /*******************
   * 
   * safety guard so the stop task can't be skipped if the process is killed mid-dose. 
   * signal handler calling pump_stop() / relay_off() on SIGTERM/SIGINT 
   * 
   */

  for (acd_key_t *curr = _acdconfig_.keys; curr != NULL; curr = curr->next) {
    if (curr->type == ACD_TYPE_GPIO_PMP) {
      LOG(LOG_NOTICE,"Making sure pump %s if off\n", curr->label, curr->data.gpio.pin);
      if (gpio_write(&curr->data.gpio, 0) == GPIO_ERROR) {
        LOG(LOG_ERR,"GPIO Failed, manually turn %s off\n", curr->label);
      }
    }
  }

  if (sig_num == SIGRESTART) {
    if (is_running_under_systemd()) {
      LOG(LOG_WARNING, "Restarting %s!\n",AQUACHEMD_SHORT_NAME);
      // If we are deamonized, we need to use the system
      if(fork() == 0) {
        sleep(2);
        char *newargv[] = {"/bin/systemctl", "restart", "aquachemd", NULL};
        char *newenviron[] = { NULL };
        execve(newargv[0], newargv, newenviron);
        exit (EXIT_SUCCESS);
      }
    } else {
      LOG(LOG_ERR, "Can't restart %s, not running as daemon!\n",AQUACHEMD_SHORT_NAME);
      return;
    }
  }

  LOG(LOG_NOTICE, "Stopping %s!\n",AQUACHEMD_SHORT_NAME);

  exit(EXIT_SUCCESS);
}

void setup_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = intHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT,  &sa, NULL);  // Ctrl+C
    sigaction(SIGTERM, &sa, NULL);  // kill / systemd stop
    sigaction(SIGHUP,  &sa, NULL);  // terminal closed / daemon reload
    sigaction(SIGQUIT, &sa, NULL);  // Ctrl+backslash
    sigaction(SIGPIPE, &sa, NULL);  // broken pipe
}


typedef enum {
  ACD_MSG_CLEAR = -1,
  ACD_MSG_NONE,
  ACD_MSG_STARTING,
  ACD_MSG_CONDITION_FAILED,
  ACD_MSG_SENSOR_READ_FAILED
} display_message_t;

/**
 * Updates the global display message based on a message type and an optional string.
 * @param type    The display_message_t enum (e.g., ACD_MSG_SENSOR_READ_FAILED).
 * @param message An additional string to append (e.g., "Temp 1"). Can be NULL.
 */
void update_display_message(struct aquachemdata *acddata, display_message_t type, const char *message) {
    // 1. Check for the "Clear" condition first
    // If type is CLEAR/NONE and the message is NULL, empty the buffer and exit.

    if ((type == ACD_MSG_CLEAR || type == ACD_MSG_NONE) && message == NULL) {
      if (acddata->display_message[0] != '\0') {
        acddata->is_dirty = true;
      }
      acddata->display_message[0] = '\0';
      return;
    }

    // Only proceed if the current message is empty
    if (acddata->display_message[0] != '\0') {
      return;
    }

    const char *prefix = "";
    const char *suffix = (message == NULL) ? "" : message;

    switch (type) {
        case ACD_MSG_CONDITION_FAILED:
          prefix = "Check failed: ";
          break;
        case ACD_MSG_SENSOR_READ_FAILED:
          prefix = "Sensor read err: ";
          break;
        //case ACD_MSG_AQUACHEM_OFF
        //  prefix = "AquachemD is off";
        //  break;
        case ACD_MSG_STARTING:
          prefix = "Starting : ";
          break;
        case ACD_MSG_NONE:
        default:
          prefix = "";
          break;
    }

    snprintf(acddata->display_message, DISPLAY_MSG_SIZE, "%s%s", prefix, suffix);

    LOG(LOG_DEBUG, "Display Message Updated: %s", acddata->display_message);
}



double elapsed_ms(const struct timespec *start_time) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    long delta_sec = now.tv_sec - start_time->tv_sec;
    long delta_nsec = now.tv_nsec - start_time->tv_nsec;

    if (delta_nsec < 0) {
        delta_sec -= 1;
        delta_nsec += 1000000000L;
    }

    return (delta_sec * 1000.0) + (delta_nsec / 1000000.0);
}

// ─── Main ─────────────────────────────────────────────────────────────────────

int main(int argc, char *argv[])
{
  struct aquachemdata acddata;
  struct timespec next_wake;

  snprintf(acddata.display_message, DISPLAY_MSG_SIZE, "Starting: %s v%s", AQUACHEMD_SHORT_NAME, AQUACHEMD_VERSION);
  CLEAR_DIRTY(acddata.is_dirty);

  char *bname = basename(argv[0]);
  if (bname == NULL || strlen(bname) == 0) {
    bname = "aquachemd"; // Hardcoded fallback
  }
  // Safely copy into the fixed buffer
  strncpy(acddata.self, bname, sizeof(acddata.self) - 1);
  acddata.self[sizeof(acddata.self) - 1] = '\0';

  if (!ezo_bus_available())
  {
    LOG(LOG_ERR, "Error: I2C bus %s not available.\n", I2C_BUS);
    LOG(LOG_ERR, "Run: sudo modprobe i2c-dev\n");
    LOG(LOG_ERR, "To persist across reboots: echo i2c-dev | sudo tee /etc/modules-load.d/i2c.conf\n");
    return 1;
  }

  // ── Scan mode ───────────────────────────────────────────────────────────────
  if (argc >= 2 && (strcasecmp(argv[1], "scan") == 0 || strcasecmp(argv[1], "deepscan") == 0))
  {
#ifdef WITH_GPIOD
    int deep = strcasecmp(argv[1], "deepscan") == 0;
    gpio_detect(deep);
#endif
    w1_detect();
    ezo_i2cdetect();
    return 0;
  }

  // ── Calibration mode ────────────────────────────────────────────────────────
  if (argc >= 2 && strcasecmp(argv[1], "calibrate") == 0)
  {
    int idx=2;
    if (argc < 3)
    {
      fprintf(stderr, "Usage: %s calibrate ph  <low|mid|high>\n", argv[0]);
      fprintf(stderr, "       %s calibrate orp <mv_value>\n", argv[0]);
      fprintf(stderr, "       %s calibrate rtd <°C_value>\n", argv[0]);
      fprintf(stderr, "       %s calibrate prs <psi_value>\n", argv[0]);
      return 1;
    }

    if (strcasecmp(argv[idx], "ph") == 0) {idx=3;}

    if (strcasecmp(argv[idx], "mid") == 0)
    {
      printf("Calibrating pH mid-point (7.00)...\n");
      return ph_calibrate_mid() == EZO_SUCCESS ? 0 : 1;
    }
    else if (strcasecmp(argv[idx], "low") == 0)
    {
      printf("Calibrating pH low-point (4.00)...\n");
      return ph_calibrate_low() == EZO_SUCCESS ? 0 : 1;
    }
    else if (strcasecmp(argv[idx], "high") == 0)
    {
      printf("Calibrating pH high-point (10.00)...\n");
      return ph_calibrate_high() == EZO_SUCCESS ? 0 : 1;
    }
    else if (strcasecmp(argv[2], "orp") == 0)
    {
      if (argc < 4)
      {
        fprintf(stderr, "Usage: %s calibrate orp <mv_value>\n", argv[0]);
        return 1;
      }
      float mv = atof(argv[3]);
      printf("Calibrating ORP at %.2f mV...\n", mv);
      return orp_calibrate(mv) == EZO_SUCCESS ? 0 : 1;
    }
    else if ( (strcasecmp(argv[2], "rtd") == 0) ||
              (strcasecmp(argv[2], "temp") == 0))
    {
      if (argc < 4)
      {
        fprintf(stderr, "Usage: %s calibrate rtd <degC>\n", argv[0]);
        return 1;
      }
      float temp = atof(argv[3]);
      printf("Calibrating RTD / Temperature probe at %.2f°C...\n", temp);
      return rtd_calibrate(temp) == EZO_SUCCESS ? 0 : 1;
    }
    else if ( (strcasecmp(argv[2], "prs") == 0))
    {
      if (argc < 4)
      {
        fprintf(stderr, "Usage: %s calibrate prs <psi>\n", argv[0]);
        return 1;
      }
      float temp = atof(argv[3]);
      printf("Calibrating PRS sensor at %.2fpsi...\n", temp);
      return prs_calibrate(temp) == EZO_SUCCESS ? 0 : 1;
    }
    else
    {
      fprintf(stderr, "Unknown calibration point '%s'\n", argv[2]);
      fprintf(stderr, "Valid options: low, mid, high, orp <mv>, rtd <degC>\n");
      return 1;
    }
  }

  // If we get here, we are in normal mode, so we continue with initialization and then loop reading sensors.
  // parse normal command line arguments
  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "-h") == 0)
    {
      //printHelp();
      printf("ADD HELP\n");
      return 0;
    }
    //if (strcmp(argv[i], "-d") == 0)
    //{
    //  _acdconfig_.deamonize = false;
    //}
    else if (strcmp(argv[i], "-c") == 0)
    {
      _acdconfig_.config_file = cleanalloc(argv[++i], STR_FULL_LENGTH);
    }
    else if (strcmp(argv[i], "-v") == 0)
    {
      _acdconfig_.log_level = LOG_DEBUG;
    }
  }
/*
  // ── Unknown argument ────────────────────────────────────────────────────────
  if (argc >= 2)
  {
    fprintf(stderr, "Unknown argument '%s'\n", argv[1]);
    fprintf(stderr, "Usage: %s [scan | calibrate <low|mid|high> | calibrate orp <mv>]\n", argv[0]);
    return 1;
  }
*/

  // Make sure we catch termination
  setup_signal_handlers();

  // Test if to use systemd/journal or not.
  init_logging_backend();

  LOG_STARTUP_EVENT();
  //FORCE_LOG(LOG_NOTICE, "Starting %s (%s) v%s\n", AQUACHEMD_NAME, AQUACHEMD_SHORT_NAME, AQUACHEMD_VERSION);

  // Setup master key/button
  acddata.keys = malloc(sizeof(acd_key_t));
  acddata.keys->ID = "AquachemD";
  acddata.keys->type = ACD_TYPE_MASTER;
  acddata.keys->label = "AquachemD";
  acddata.keys->state = ACD_LED_ENABLED;

  parse_config_file(&acddata);
  acddata.keys->label = _acdconfig_.main_label; // Update main label from config

  start_net_services(&acddata);

  // Setup any specifics for GPIO / D1W etc
  for (acd_key_t *curr = acddata.keys; curr != NULL; curr = curr->next) {
    if (curr->type == ACD_TYPE_GPIO_COND) {
      LOG(LOG_DEBUG,"Setting up GPIO Condition: %s, pin %d\n", curr->label, curr->data.gpio.pin);
      if (gpio_open(&curr->data.gpio, _acdconfig_.gpio_chip, curr->data.gpio.pin, GPIO_INPUT, curr->data.gpio.active) != 0) {
        LOG(LOG_ERR, "Failed to open GPIO for %s, pin %d\n", curr->label, curr->data.gpio.pin);
      }
    } else if (curr->type == ACD_TYPE_GPIO_PMP) {
      LOG(LOG_DEBUG,"Setting up GPIO Pump: %s, pin %d\n", curr->label, curr->data.gpio.pin);
      if (gpio_open(&curr->data.gpio, _acdconfig_.gpio_chip, curr->data.gpio.pin, GPIO_OUTPUT, curr->data.gpio.active) != 0) {
        LOG(LOG_ERR, "Failed to open GPIO for %s, pin %d\n", curr->label, curr->data.gpio.pin);
      } else {
        gpio_write(&curr->data.gpio, 0); // Turn pump off
        //sync_pump_state(&acddata, curr);
      }
    } else if (curr->type == ACD_TYPE_D1W_TEMP) {
      w1_init_generic(&curr->data.w1, curr->data.w1.path, curr->data.w1.scale, curr->data.w1.offset);
    } else if (curr->type == ACD_TYPE_SYSFS_VALUE) {
      sysfs_init_sensor(&curr->data.sysfs);
    }

    if (curr->type == ACD_TYPE_MASTER) {
      curr->state = ACD_LED_ENABLED;
    } else if (IS_INPUT(curr->type)) {
      curr->state = ACD_LED_DISABLED; // DON'T use setKeyLed() here, startup need to force to enabled.
      if (isMASKSET(curr->flags,CALC_AVERAGE)) {
        pthread_mutex_init(&curr->stats.lock, NULL);
        reset_sensor_average(&curr->stats);
      }
    } else if (IS_OUTPUT(curr->type)) {
      // GPIO status will be set from sync_pump_state() above
      //if (curr->type != ACD_TYPE_GPIO_PMP) {
        curr->state = ACD_LED_ENABLED;
      //}
    } else if (IS_CONDITION(curr->type)) {
      curr->state = ACD_LED_OFF;
    }

    // reset error count
    curr->err_cnt = 0;
  }

  start_gpio_monitor(&acddata);

  // --- EVERYTHING IS READY ---
  #ifdef USE_SYSTEMD
  sd_notify(0, "READY=1");
  #endif // USE_SYSTEMD

  // ── Normal mode — loop reading sensors ──────────────────────────────────────

  int reading_log_level = LOG_INFO;
  if (_acdconfig_.log_sensor_readings) {
    reading_log_level = LOG_NOTICE;
  } 

  update_display_message(&acddata, ACD_MSG_CLEAR, NULL);
  clock_gettime(CLOCK_MONOTONIC, &next_wake);

  while (1)
  {
    acd_scope_t sensors_read_scope = ACD_SCOPE_GLOBAL;
    float temp_reading_for_ph = UNKNOWN;
    char *master_temp_label;
    bool all_conditions_met = true; // Should be able to get rid of this all together now, and just use acddata.keys->state 

    LOG(reading_log_level,"---- taking reading(s) ----\n");
    update_display_message(&acddata, ACD_MSG_CLEAR, NULL);

    if (acddata.keys->state == ACD_LED_OFF) {
      LOG(LOG_DEBUG,"AquachemD is off, skipping reading of sensors!\n");
      goto next_wake; // Skip the rest of the loop and go straight to sleep if any condition is not met
    }

    for (acd_key_t *curr = acddata.keys; curr != NULL; curr = curr->next) {
     if (IS_CONDITION(curr->type)) {
        if (curr->type == ACD_TYPE_GPIO_COND) {
          // This should have been changed from the gpio_monitor, but 2nd check doesn't hurt
          if (sensor_is_met(&curr->data.gpio) > 0 && !curr->met) {
            ASSIGN_IF_CHANGED(curr->met, !curr->met, acddata.is_dirty, curr->is_dirty);
            set_key_state(&acddata, curr, curr->met?ACD_LED_ON:ACD_LED_OFF);
          }
        }
        if (!curr->met) {
          if ( !isMASKSET(curr->flags, CONDITION_NOTIFIED)) {
            LOG(LOG_WARNING,"Condition not met: %s\n", curr->label);
            update_display_message(&acddata, ACD_MSG_CONDITION_FAILED, curr->label);
            setMASK(curr->flags, CONDITION_NOTIFIED);
          }
          all_conditions_met = false;
        } else if (curr->met && isMASKSET(curr->flags, CONDITION_NOTIFIED)) {
          removeMASK(curr->flags, CONDITION_NOTIFIED);
          LOG(LOG_NOTICE,"Condition satisfied: %s\n", curr->label);
        }
      } else if (curr->type == ACD_TYPE_GPIO_PMP) {
        check_pump_state(&acddata, curr);
      }
    }
    
    //  Master state = ON, scope = Allow         // All good.
    //  Master state = ENABLED, scope = Global   // Condition set to Global failed (but can read local sensors)
    //  Master state = ON, scope = Local         // Condition set to local failed (is can read all sensors, but not dose)
    //LOG(LOG_ERR, "Master state = %s, scope = %s\n",acd_state_to_str(acddata.keys->state), acd_scope_to_str(acddata.keys->scope) );

    if (all_conditions_met && acddata.keys->state != ACD_LED_OFF) {
      sensors_read_scope = ACD_SCOPE_ALLOW;
    //} else if (!all_conditions_met && acddata.keys->state == ACD_LED_ENABLED && acddata.keys->scope == ACD_SCOPE_GLOBAL) {
    } else if (!all_conditions_met && acddata.keys->scope == ACD_SCOPE_GLOBAL && acddata.keys->state != ACD_LED_OFF) {
      sensors_read_scope = ACD_SCOPE_LOCAL;
    } else if (acddata.keys->scope == ACD_SCOPE_GLOBAL || acddata.keys->state == ACD_LED_OFF){
      //LOG(LOG_DEBUG,"AquachemD sensor read scope global, skipping reading of sensors!\n");
      LOG(reading_log_level, "Master state = %s, scope = %s, skipping reading of sensors!\n",acd_state_to_str(acddata.keys->state), acd_scope_to_str(acddata.keys->scope) );
      goto next_wake; // Skip the rest of the loop and go straight to sleep if any condition is not met
    }


    for (acd_key_t *key = acddata.keys->next; key != NULL; key = key->next) {
      
      if (sensors_read_scope == ACD_SCOPE_LOCAL && key->scope == ACD_SCOPE_GLOBAL) {
        LOG(LOG_DEBUG,"AquachemD sensor read scope local, skipping reading of global sensor %s\n",key->label);
        //LOG(LOG_INFO, "Master  %s state=%s, scope=%s. Skipping reading of sensors",acd_state_to_str(curr->state), curr->label, acd_scope_to_str(curr->scope));
        continue;
      } else if (isMASKSET(key->flags,  ACD_FLAG_FAULTED)) {
        LOG(LOG_DEBUG,"Sensor %s failed, skipping\n",key->label);
        continue;
      } else if (key->state == ACD_LED_DISABLED) {
        LOG(LOG_DEBUG,"Sensor %s is disabled, skipping\n",key->label);
        continue;
      }

      switch (key->type) {
        case ACD_TYPE_MQTT_TEMP:          
          if (key->index == MASTER_ID) { // If this is the master temp sensor, also update the temp reading for pH compensation
            temp_reading_for_ph = key->value;
            master_temp_label = key->label;
          }
          LOG(LOG_DEBUG, "MQTT Sensor '%s' current value: %.2f\n", key->label, key->value);
          break;
        case ACD_TYPE_D1W_TEMP: {
          w1_reading_t temp_reading = w1_read(&key->data.w1);
          if (temp_reading.status == W1_SUCCESS) {
            LOG(reading_log_level,"Temp %s : %.2f°C\n", key->label, temp_reading.value);
            if (key->index == MASTER_ID) { // If this is the master temp sensor, also update the temp reading for pH compensation
              temp_reading_for_ph = temp_reading.value; 
              master_temp_label = key->label;
            }
            ASSIGN_IF_CHANGED(key->value, temp_reading.value, acddata.is_dirty, key->is_dirty);
            //SET_IF_CHANGED(key->state, ACD_LED_ON, acddata.is_dirty);
            set_key_state(&acddata, key, ACD_LED_ON);
            key->err_cnt=0;
            update_sensor_average(key);
          } else {
            LOG(LOG_WARNING, "D1w Temp Sensor '%s' read failed (status %d)\n", key->label, temp_reading.status);
            //SET_IF_CHANGED(key->state, ACD_LED_OFF, acddata.is_dirty);
            update_display_message(&acddata, ACD_MSG_SENSOR_READ_FAILED, key->label);
            sensor_read_error(&acddata, key);
          }
        } break;
        case ACD_TYPE_EZO_TEMP: {
          rtd_reading_t temp_reading = rtd_get_reading();
          if (temp_reading.status == EZO_SUCCESS) {
            LOG(reading_log_level,"Temp %s : %.2f°C\n", key->label, temp_reading.value);
            if (key->index == MASTER_ID) { // If this is the master temp sensor, also update the temp reading for pH compensation
              temp_reading_for_ph = temp_reading.value;
              master_temp_label = key->label;
            }
            ASSIGN_IF_CHANGED(key->value, temp_reading.value, acddata.is_dirty, key->is_dirty);
            //SET_IF_CHANGED(key->state, ACD_LED_ON, acddata.is_dirty);
            set_key_state(&acddata, key, ACD_LED_ON);
            key->err_cnt=0;
            update_sensor_average(key);
          } else {
            LOG(LOG_WARNING, "EZO Temp Sensor '%s' read failed (status %d)\n", key->label, temp_reading.status);
            //SET_IF_CHANGED(key->state, ACD_LED_OFF, acddata.is_dirty);
            update_display_message(&acddata, ACD_MSG_SENSOR_READ_FAILED, key->label);
            sensor_read_error(&acddata, key);
          }
        } break;
        case ACD_TYPE_EZO_PH: {
          ph_reading_t ph_reading;
          if (_acdconfig_.temp_compensated_ph == false) {
            ph_reading = ph_get_reading();
          } else if ( temp_reading_for_ph != UNKNOWN) {
            if (temp_reading_for_ph > _acdconfig_.ph_reading_temp_max || temp_reading_for_ph < _acdconfig_.ph_reading_temp_min ) {
              char buf[128];
              sprintf(buf, "Water temperature %.2f°C too %s for Ph reading", temp_reading_for_ph, temp_reading_for_ph < _acdconfig_.ph_reading_temp_min?"cold":"hot");
              LOG(LOG_WARNING, "%s\n", buf);
              //SET_IF_CHANGED(key->state, ACD_LED_OFF, acddata.is_dirty);
              set_key_state(&acddata, key, ACD_LED_OFF);
              update_display_message(&acddata, ACD_MSG_CONDITION_FAILED, buf);
              break;
            }
            LOG(reading_log_level, "Using %s, %.2f for pH compensated reading", master_temp_label, temp_reading_for_ph);
            ph_reading = ph_get_reading_compensated(temp_reading_for_ph);
          } else {
            LOG(LOG_WARNING, "EZO pH Sensor '%s' skipped compensation because temp is unknown\n", key->label);
            //SET_IF_CHANGED(key->state, ACD_LED_OFF, acddata.is_dirty);
            set_key_state(&acddata, key, ACD_LED_OFF);
            break;
          }
          
          if (ph_reading.status == EZO_SUCCESS) {
            LOG(reading_log_level,"EZO pH Sensor %s : %.2f\n", key->label, ph_reading.value);
            ASSIGN_IF_CHANGED(key->value, ph_reading.value, acddata.is_dirty, key->is_dirty);
            //SET_IF_CHANGED(key->state, ACD_LED_ON, acddata.is_dirty);
            set_key_state(&acddata, key, ACD_LED_ON);
            key->err_cnt=0;
            update_sensor_average(key);
          } else {
            LOG(LOG_WARNING, "EZO pH Sensor '%s' read failed (status %d)\n", key->label, ph_reading.status);
            //SET_IF_CHANGED(key->state, ACD_LED_OFF, acddata.is_dirty);
            update_display_message(&acddata, ACD_MSG_SENSOR_READ_FAILED, key->label);
            sensor_read_error(&acddata, key);
          }
        } break;
        case ACD_TYPE_EZO_ORP: {
          orp_reading_t orp_reading = orp_get_reading();
          if (orp_reading.status == EZO_SUCCESS) {
            LOG(reading_log_level,"EZO ORP Sensor %s : %.2f mV\n", key->label, orp_reading.value);
            ASSIGN_IF_CHANGED(key->value, orp_reading.value, acddata.is_dirty, key->is_dirty);
            //SET_IF_CHANGED(key->state, ACD_LED_ON, acddata.is_dirty);
            set_key_state(&acddata, key, ACD_LED_ON);
            key->err_cnt=0;
            update_sensor_average(key);
          } else {
            LOG(LOG_WARNING, "EZO ORP Sensor '%s' read failed (status %d)\n", key->label, orp_reading.status);
            //SET_IF_CHANGED(key->state, ACD_LED_OFF, acddata.is_dirty);
            update_display_message(&acddata, ACD_MSG_SENSOR_READ_FAILED, key->label);
            sensor_read_error(&acddata, key);
          }
         } break;
        case ACD_TYPE_EZO_PRS: {
          prs_reading_t prs_reading = prs_get_reading();
          if (prs_reading.status == EZO_SUCCESS) {
            LOG(reading_log_level,"EZO PRS Sensor %s : %.2f mV\n", key->label, prs_reading.value);
            ASSIGN_IF_CHANGED(key->value, prs_reading.value, acddata.is_dirty, key->is_dirty);
            //SET_IF_CHANGED(key->state, ACD_LED_ON, acddata.is_dirty);
            set_key_state(&acddata, key, ACD_LED_ON);
            key->err_cnt=0;
            update_sensor_average(key);
          } else {
            LOG(LOG_WARNING, "EZO PRS Sensor '%s' read failed (status %d)\n", key->label, prs_reading.status);
            //SET_IF_CHANGED(key->state, ACD_LED_OFF, acddata.is_dirty);
            update_display_message(&acddata, ACD_MSG_SENSOR_READ_FAILED, key->label);
            sensor_read_error(&acddata, key);
          }

        } break;
        case ACD_TYPE_SYSFS_VALUE:{
          sysfs_reading_t reading = sysfs_read_sensor(&key->data.sysfs);
          if (reading.status == SYSFS_SUCCESS) {
            LOG(reading_log_level,"%s : %.2f\n", key->label, reading.value);
            ASSIGN_IF_CHANGED(key->value, reading.value, acddata.is_dirty, key->is_dirty);
            set_key_state(&acddata, key, ACD_LED_ON);
            key->err_cnt=0;
            update_sensor_average(key);
          } else {
            LOG(LOG_WARNING, "System FS Sensor '%s' read failed (status %d)\n", key->label, reading.status);
            update_display_message(&acddata, ACD_MSG_SENSOR_READ_FAILED, key->label);
            sensor_read_error(&acddata, key);
          }
        } break;

        case ACD_TYPE_GPIO_PMP:
        case ACD_TYPE_EZO_PMP:
        case ACD_TYPE_MQTT_COND:
        case ACD_TYPE_GPIO_COND:
        case ACD_TYPE_MQTT_VALUE:
        break;

        default:
          LOG(LOG_WARNING, "Unknown sensor type for sensor '%s'\n", key->label);
      }
    }


next_wake:
    
    LOG(reading_log_level,"- reading(s) took: %.2fs -\n", elapsed_ms(&next_wake) / 1000);
    // Advance the target wake time by one interval
    next_wake.tv_sec += _acdconfig_.sensor_poll_time;

    // check for drift
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    
    // If we fell behind (loop took longer than poll_seconds), reset the base to 'now' 
    if (now.tv_sec >= next_wake.tv_sec) {
      next_wake = now; // Reset base to current time
    }
    // Sleep until the next wake time
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_wake, NULL);
  }

  return 0;
}







char *_upgrade_version = NULL;

#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

// Update this macro when you officially transition to your new GitHub organization
#define AQUACHEMD_REPO "AqualinkD/AquachemD"
#define AQUACHEMD_REMOTE_INSTALL "remote-install.sh"

void set_upgrade_version(char *version)
{
  _upgrade_version = malloc( (sizeof(char*) * strlen(version)) + 2);
  snprintf(_upgrade_version, strlen(version)+1, version);
}

bool start_upgrade(const char *version)
{
    int pipe_curl_to_bash[2];
    pid_t pid_curl, pid_bash;
    int status_curl, status_bash;
    char url[256];

    LOG(LOG_NOTICE, "Configuring upgrade pipeline!\n");

    // Format target GitHub API URL using our repo path configuration
    snprintf(url, sizeof(url), "https://api.github.com/repos/%s/contents/release/%s", AQUACHEMD_REPO,AQUACHEMD_REMOTE_INSTALL);

    char *curl_args[] = {"curl", "-fsSl", "-H", "Accept: application/vnd.github.raw", url, NULL};
    char *bash_args[] = {"bash", "-s", "--", (char *)(_upgrade_version ? _upgrade_version : ""), NULL};

    // 1. Notice must be logged BEFORE execution, as the script will terminate this process
    LOG(LOG_NOTICE, "Initiating daemon upgrade script (Target: %s)...", _upgrade_version ? _upgrade_version : "latest");

    if (pipe(pipe_curl_to_bash) == -1)
    {
        LOG(LOG_ERR, "Upgrade error: unable to open pipeline");
        return false;
    }

    // --- FORK 1: CURL ---
    pid_curl = fork();
    if (pid_curl == -1)
    {
        LOG(LOG_ERR, "Upgrade error: fork failed (curl)");
        close(pipe_curl_to_bash[0]);
        close(pipe_curl_to_bash[1]);
        return false;
    }

    if (pid_curl == 0)
    { // Inside Child Process (curl)
        close(pipe_curl_to_bash[0]); // Close unused read end
        if (dup2(pipe_curl_to_bash[1], STDOUT_FILENO) == -1) {
            _exit(EXIT_FAILURE);
        }
        close(pipe_curl_to_bash[1]);
        
        execvp("curl", curl_args);
        // If execvp returns, it failed. Prevent child from escaping into daemon main loop!
        _exit(127); 
    }

    // --- FORK 2: BASH ---
    pid_bash = fork();
    if (pid_bash == -1)
    {
        LOG(LOG_ERR, "Upgrade error: fork failed (bash)");
        close(pipe_curl_to_bash[0]);
        close(pipe_curl_to_bash[1]);
        
        // Clean up the already running curl child to prevent resource leaks
        kill(pid_curl, SIGTERM);
        waitpid(pid_curl, NULL, 0);
        return false;
    }

    if (pid_bash == 0)
    { // Inside Child Process (bash)
        close(pipe_curl_to_bash[1]); // Close unused write end
        if (dup2(pipe_curl_to_bash[0], STDIN_FILENO) == -1) {
            _exit(EXIT_FAILURE);
        }
        close(pipe_curl_to_bash[0]);
        
        execvp("bash", bash_args);
        // If execvp returns, it failed. Prevent child from escaping.
        _exit(127); 
    }

    // --- PARENT PROCESS CLEANUP ---
    // Close parent copy of descriptors immediately so EOF signals pass cleanly down the pipe
    close(pipe_curl_to_bash[0]);
    close(pipe_curl_to_bash[1]);

    /* * NOTE ON THE PROCESS LIFECYCLE:
     * If remote_install.sh executes successfully, your service manager (systemd)
     * will catch up to this parent process right here while it blocks on waitpid().
     * A SIGTERM will hit this daemon, stopping it cleanly so the installer can replace it.
     */

    // Wait for curl download phase to finalize
    if (waitpid(pid_curl, &status_curl, 0) == -1)
    {
        LOG(LOG_ERR, "Upgrade error: waitpid failed (curl)");
        return false;
    }

    // If curl explicitly failed (e.g., 404, network drop), stop bash from hanging on empty stdin
    if (WIFEXITED(status_curl) && WEXITSTATUS(status_curl) != 0) {
        LOG(LOG_ERR, "Upgrade error: curl failed with exit code: %d", WEXITSTATUS(status_curl));
        kill(pid_bash, SIGTERM);
        waitpid(pid_bash, NULL, 0);
        return false;
    }

    // Wait for bash installer phase to complete
    if (waitpid(pid_bash, &status_bash, 0) == -1)
    {
        LOG(LOG_ERR, "Upgrade error: waitpid failed (bash)");
        return false;
    }

    // Parse bash output metrics (Only reached if script doesn't force kill this process)
    if (WIFEXITED(status_bash))
    {
        if (WEXITSTATUS(status_bash) != 0) {
            LOG(LOG_ERR, "Upgrade error: bash script exited with error code: %d", WEXITSTATUS(status_bash));
            return false;
        }
    }
    else if (WIFSIGNALED(status_bash))
    {
        LOG(LOG_ERR, "Upgrade error: bash script terminated by signal: %d", WTERMSIG(status_bash));
        return false;
    }

    LOG(LOG_NOTICE, "Upgrade pipeline completed without daemon restart.");
    return true;
}






















#ifdef EXAMPLE_CODE

// Below is some code to implement a more robust task scheduler that would allow for more complex tasks 
// like dosing for a specific duration without blocking the main loop. 
// It's not fully implemented yet, but I wanted to keep it around as a reference for how
 // might implement this in the future.


#include <time.h>

// ─── Task scheduler ───────────────────────────────────────────────────────────

typedef void (*task_fn)(struct aquachemdata *data);

typedef struct {
  const char   *name;
  task_fn       fn;
  int           interval_sec;     // how often to run
  int           duration_sec;     // 0 = instant, >0 = run for this long then stop
  struct timespec next_run;       // absolute time of next execution
  struct timespec stop_at;        // for duration tasks: when to stop
  bool          running;          // for duration tasks: currently active
} task_t;

// ─── Individual task functions ────────────────────────────────────────────────

static void task_read_sensors(struct aquachemdata *data)
{
  LOG(LOG_NOTICE, "---\n");
  data->temp_reading = rtd_get_reading();

  if (data->temp_reading.status == EZO_SUCCESS)
  {
    SET_DIRTY(data->is_dirty);
    LOG(LOG_NOTICE, "Temp: %.2f°C\n", data->temp_reading.value);
    data->ph_reading = ph_get_reading_compensated(data->temp_reading.value);
  }
  else
  {
    LOG(LOG_NOTICE, "Temp: read failed (status %d)\n", data->temp_reading.status);
    data->ph_reading = ph_get_reading();
  }

  if (data->ph_reading.status == EZO_SUCCESS)
  {
    SET_DIRTY(data->is_dirty);
    LOG(LOG_NOTICE, "pH  : %.2f\n", data->ph_reading.value);
  }
  else
    LOG(LOG_NOTICE, "pH  : read failed (status %d)\n", data->ph_reading.status);

  data->orp_reading = orp_get_reading();
  if (data->orp_reading.status == EZO_SUCCESS)
  {
    SET_DIRTY(data->is_dirty);
    LOG(LOG_NOTICE, "ORP : %.2f mV\n", data->orp_reading.value);
  }
  else
    LOG(LOG_NOTICE, "ORP : read failed (status %d)\n", data->orp_reading.status);

  LOG(LOG_NOTICE, "---\n");
}

static void task_acid_dose_start(struct aquachemdata *data)
{
  // Start dosing — EZO-PMP or GPIO relay
  LOG(LOG_NOTICE, "Acid dose: START\n");
  pump_dose_continuous(5.0f);   // or relay_on(&acid_relay)
}

static void task_acid_dose_stop(struct aquachemdata *data)
{
  // Stop dosing
  LOG(LOG_NOTICE, "Acid dose: STOP\n");
  pump_stop();                  // or relay_off(&acid_relay)
}

// ─── Scheduler helpers ────────────────────────────────────────────────────────

static void timespec_add_sec(struct timespec *ts, int sec)
{
  ts->tv_sec += sec;
}

// Returns true if ts is in the past or now
static bool timespec_due(const struct timespec *ts)
{
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (now.tv_sec > ts->tv_sec) ||
         (now.tv_sec == ts->tv_sec && now.tv_nsec >= ts->tv_nsec);
}

// Sleep until the earliest upcoming task
static void sleep_until_next(task_t *tasks, int num_tasks)
{
  struct timespec earliest = tasks[0].next_run;

  for (int i = 1; i < num_tasks; i++)
  {
    if (tasks[i].next_run.tv_sec < earliest.tv_sec ||
       (tasks[i].next_run.tv_sec == earliest.tv_sec &&
        tasks[i].next_run.tv_nsec < earliest.tv_nsec))
      earliest = tasks[i].next_run;
  }

  // Don't sleep if already overdue
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  if (earliest.tv_sec <= now.tv_sec) return;

  clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &earliest, NULL);
}

// ─── Main loop ────────────────────────────────────────────────────────────────

void run_main_loop(struct aquachemdata *data)
{
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);

  // ── Define tasks ────────────────────────────────────────────────────────────
  // For dosing: we use two tasks — start and stop — offset by duration_sec
  // start fires every interval, stop fires interval + duration seconds later

  task_t tasks[] = {
    {
      .name         = "sensors",
      .fn           = task_read_sensors,
      .interval_sec = _acdconfig_.sensor_poll_time,
      .next_run     = now,
    },
    {
      .name         = "acid_start",
      .fn           = task_acid_dose_start,
      .interval_sec = 30,    // dose every 30 seconds
      .next_run     = now,
    },
    {
      .name         = "acid_stop",
      .fn           = task_acid_dose_stop,
      .interval_sec = 30,    // same interval as start
      .next_run     = now,
    },
  };

  // Offset the stop task by the dose duration so it fires 5s after start
  tasks[2].next_run.tv_sec += 5;

  int num_tasks = sizeof(tasks) / sizeof(tasks[0]);

  // ── Main loop ───────────────────────────────────────────────────────────────
  while (1)
  {
    clock_gettime(CLOCK_MONOTONIC, &now);

    for (int i = 0; i < num_tasks; i++)
    {
      if (!timespec_due(&tasks[i].next_run))
        continue;

      tasks[i].fn(data);

      // Schedule next run — advance from the scheduled time not from now
      // to prevent drift accumulating
      timespec_add_sec(&tasks[i].next_run, tasks[i].interval_sec);

      // Warn if we're falling behind
      clock_gettime(CLOCK_MONOTONIC, &now);
      if (now.tv_sec > tasks[i].next_run.tv_sec)
        LOG(LOG_WARNING, "Task '%s' overran its interval\n", tasks[i].name);
    }

    sleep_until_next(tasks, num_tasks);
  }
}

#endif



#ifdef EXAMPLE_CODE

// GPIO input and set event trigger.

#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CHIP_PATH "/dev/gpiochip4"
#define LINE_OFFSET 17

int main(void) {
    struct gpiod_chip *chip;
    struct gpiod_line_settings *settings;
    struct gpiod_line_config *line_cfg;
    struct gpiod_request_config *req_cfg;
    struct gpiod_line_request *request;
    struct gpiod_edge_event_buffer *event_buffer;
    struct gpiod_edge_event *event;

    // 1. Open the GPIO chip
    chip = gpiod_chip_open(CHIP_PATH);
    if (!chip) {
        perror("Failed to open chip");
        return EXIT_FAILURE;
    }

    // 2. Configure line settings (Input + Both Edges)
    settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_edge_detection(settings, GPIOD_LINE_EDGE_BOTH);

    // 3. Map settings to the specific line offset
    line_cfg = gpiod_line_config_new();
    unsigned int offsets[] = { LINE_OFFSET };
    gpiod_line_config_add_line_settings(line_cfg, offsets, 1, settings);

    // 4. Set up the request (consumer name)
    req_cfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req_cfg, "edge-detector-c");

    // 5. Request the line
    request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);
    if (!request) {
        perror("Failed to request lines");
        goto clean_up;
    }

    // 6. Create a buffer to hold incoming events
    event_buffer = gpiod_edge_event_buffer_new(16); // Holds up to 16 events
    if (!event_buffer) {
        perror("Failed to create event buffer");
        goto clean_up;
    }

    printf("Waiting for edge events on pin %d...\n", LINE_OFFSET);

    while (1) {
        // Block and wait for an event (NULL means infinite timeout)
        int ret = gpiod_line_request_wait_edge_events(request, NULL);
        if (ret < 0) {
            perror("Error waiting for events");
            break;
        }

        // Read the events into our buffer
        int num_events = gpiod_line_request_read_edge_events(request, event_buffer, 16);
        if (num_events < 0) {
            perror("Error reading events");
            break;
        }

        // Process the events
        for (int i = 0; i < num_events; i++) {
            event = gpiod_edge_event_buffer_get_event(event_buffer, i);
            int ev_type = gpiod_edge_event_get_event_type(event);
            uint64_t timestamp = gpiod_edge_event_get_timestamp_ns(event);

            const char *type_str = (ev_type == GPIOD_EDGE_EVENT_RISING_EDGE) ? "Rising" : "Falling";
            printf("Event: %s edge detected at %lu ns\n", type_str, timestamp);
        }
    }

clean_up:
    // Clean up allocated memory and release lines
    if (event_buffer) gpiod_edge_event_buffer_free(event_buffer);
    if (request) gpiod_line_request_release(request);
    if (req_cfg) gpiod_request_config_free(req_cfg);
    if (line_cfg) gpiod_line_config_free(line_cfg);
    if (settings) gpiod_line_settings_free(settings);
    if (chip) gpiod_chip_close(chip);

    return EXIT_SUCCESS;
}


#endif







#ifdef EXAMPLE_CODE
// EZO PUMP (I2C) Dose 5ml of pH down, poll until done

pump_dose(5.0f);
sleep(1);
while (pump_get_status().is_pumping)
  sleep(1);
printf("Dispensed: %.2f ml total\n", pump_get_total_volume());


// GPIO Dosing pump example
// CM4 — pH dosing pump on gpiochip0 pin 17, active-high relay
gpio_handle_t ph_pump;
gpio_open(&ph_pump, "gpiochip0", 17, GPIO_OUTPUT, GPIO_ACTIVE_HIGH, "ph_pump");

// Turn pump on for 5 seconds then off
relay_on(&ph_pump);
sleep(5);
relay_off(&ph_pump);
gpio_close(&ph_pump);

// 1wire sensors example

// Option 1 — explicit path
w1_sensor_t pool_temp;
w1_init_ds18b20(&pool_temp, "/sys/bus/w1/devices/28-0304949760eb", "pool_water");
w1_reading_t r = w1_read(&pool_temp);

// Option 2 — auto-discover all DS18B20s on the bus
w1_sensor_t sensors[8];
int found = w1_find_ds18b20(sensors, 8);
for (int i = 0; i < found; i++)
{
  w1_reading_t r = w1_read(&sensors[i]);
  printf("%s: %.3f °C\n", sensors[i].label, r.value);
}

// Option 3 — generic sensor with custom scale/offset
w1_sensor_t flow;
w1_init_generic(&flow, "/sys/bus/w1/devices/xx-xxxx", 0.01f, 0.0f, "flow_meter");

#endif