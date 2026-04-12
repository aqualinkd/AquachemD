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

#include "version.h"
#include "aquachemd.h"
#include "utils.h"
#include "ezo.h"
#include "1wire.h"
#include "utils.h"
#include "config.h"
#include "net_services.h"


#ifdef WITH_GPIOD
#include "gpio.h"
#endif



void intHandler(int sig_num)
{
  /*******************
   * ADD
   * safety guard so the stop task can't be skipped if the process is killed mid-dose. 
   * signal handler calling pump_stop() / relay_off() on SIGTERM/SIGINT 
   * 
   */



  /*
  if (sig_num == SIGRUPGRADE) {
    if (! run_aqualinkd_upgrade(_aqualink_data.upgrade_version)) {
      LOG(AQUA_LOG,LOG_ERR, "AqualinkD upgrade failed!\n");
    }
    return; // Let the upgrade process terminate us.
  }

  LOG(AQUA_LOG,LOG_WARNING, "Stopping!\n");

  _keepRunning = false;

  if (sig_num == SIGRESTART) {
    LOG(AQUA_LOG,LOG_WARNING, "Restarting AqualinkD!\n");
    // If we are deamonized, we need to use the system
    if (_aqconfig_.deamonize) {
      if(fork() == 0) {
        sleep(2);
        char *newargv[] = {"/bin/systemctl", "restart", "aqualinkd", NULL};
        char *newenviron[] = { NULL };
        execve(newargv[0], newargv, newenviron);
        exit (EXIT_SUCCESS);
      }
#ifdef SELF_RESTART
    } else {
      _restart = true;
#endif
    }
  }
  //LOG(AQUA_LOG,LOG_NOTICE, "Stopping!\n");
  //if (dummy){}// stop compile warnings

  stopPacketLogger();
  close_serial_port(-1);
  */
  LOG(LOG_NOTICE, "Stopping!\n");
  exit(EXIT_SUCCESS);
}


// ─── Main ─────────────────────────────────────────────────────────────────────

int main(int argc, char *argv[])
{
  struct aquachemdata acddata;
  struct timespec next_wake;
  //_acdconfig_.log_level = LOG_NOTICE;
  _acdconfig_.deamonize = true;
  _acdconfig_.config_file = "/etc/aquachemd.conf";

#ifdef DUMMY_SENSORS
//  _acdconfig_.log_level = LOG_DEBUG;
#endif

  CLEAR_DIRTY(acddata.is_dirty);


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
    if (argc < 3)
    {
      fprintf(stderr, "Usage: %s calibrate <low|mid|high>\n", argv[0]);
      fprintf(stderr, "       %s calibrate orp <mv_value>\n", argv[0]);
      return 1;
    }

    if (strcasecmp(argv[2], "mid") == 0)
    {
      printf("Calibrating pH mid-point (7.00)...\n");
      return ph_calibrate_mid() == EZO_SUCCESS ? 0 : 1;
    }
    else if (strcasecmp(argv[2], "low") == 0)
    {
      printf("Calibrating pH low-point (4.00)...\n");
      return ph_calibrate_low() == EZO_SUCCESS ? 0 : 1;
    }
    else if (strcasecmp(argv[2], "high") == 0)
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
    else
    {
      fprintf(stderr, "Unknown calibration point '%s'\n", argv[2]);
      fprintf(stderr, "Valid options: low, mid, high, orp <mv>\n");
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
    if (strcmp(argv[i], "-d") == 0)
    {
      _acdconfig_.deamonize = false;
    }
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


  FORCE_LOG(LOG_NOTICE, "Starting %s (%s) v%s\n", AQUACHEMD_NAME, AQUACHEMD_SHORT_NAME, AQUACHEMD_VERSION);



  acddata.keys = malloc(sizeof(acd_key_t));
  acddata.keys->ID = "AquachemD";
  acddata.keys->type = KEY_TYPE_NONE;
  acddata.keys->label = "AquachemD";
  acddata.keys->state = ACD_LED_ENABLED;

  parse_config_file(&acddata);
  acddata.keys->label = _acdconfig_.main_label; // Update main label from config

  start_net_services(&acddata);

  // Setup any GPIO based condition monitoring
  for (acd_condition *curr = acddata.conditions; curr != NULL; curr = curr->next) {
    if (curr->type == COND_GPIO) {
      LOG(LOG_DEBUG,"Setting up GPIO Condition: %s\n", curr->label);
      curr->gpio_handle = malloc(sizeof(gpio_handle_t));
      if (curr->gpio_handle == NULL) {
        LOG(LOG_ERR, "Failed to allocate memory for GPIO handle\n");
        continue; 
      }
        //memset(curr->gpio_handle, 0, sizeof(gpio_handle_t));
      //if (gpio_open(curr->gpio_handle, _acdconfig_.gpio_chip, curr->gpio_pin, GPIO_INPUT, GPIO_ACTIVE_HIGH, curr->label) != 0) {
      if (gpio_open(curr->gpio_handle, _acdconfig_.gpio_chip, curr->gpio_pin, GPIO_INPUT, curr->gpio_value, curr->label) != 0) {
        LOG(LOG_ERR, "Failed to open GPIO for %s\n", curr->label);
        curr->gpio_handle = NULL; // Mark as unavailable
      }
    }
  }


  // ── Normal mode — loop reading sensors ──────────────────────────────────────

  clock_gettime(CLOCK_MONOTONIC, &next_wake);
  while (1)
  {
    float temp_reading_for_ph = UNKNOWN;
    bool all_conditions_met = true;

    LOG(LOG_NOTICE,"--- loop ---\n");

    if (acddata.keys->state == ACD_LED_OFF) {
      LOG(LOG_DEBUG,"AquachemD is off, skipping sensor read\n");
      goto next_wake; // Skip the rest of the loop and go straight to sleep if any condition is not met
    }

    for (acd_condition *curr = acddata.conditions; curr != NULL; curr = curr->next) {
      if (curr->type == COND_GPIO && curr->gpio_handle != NULL) {
        curr->met = (gpio_read(curr->gpio_handle) == curr->gpio_value) ? true : false;
      }

      if (!curr->met) {
        LOG(LOG_WARNING,"Condition not met: %s, not taking reading\n", curr->label);
        all_conditions_met = false;
        // Set aquachemd ket to enabled, and everything to off.
        if (acddata.keys->state == ACD_LED_ON)
          SET_IF_CHANGED(acddata.keys->state, ACD_LED_ENABLED, acddata.is_dirty);

        // Move to 2nd key and set everything to off
        for (acd_key_t *curr = acddata.keys->next; curr != NULL; curr = curr->next) {
          SET_IF_CHANGED(curr->state, ACD_LED_OFF, acddata.is_dirty);
        }

        if (_acdconfig_.post_condition == false) {
          // if we are not posting conditions, we can skip the rest of the loop and go straight to sleep as soon as we find one that is not met. If we are posting conditions, we want to keep going through the loop to update the state of all conditions and sensors so that the UI will reflect the current status.
          goto next_wake; // Skip the rest of the loop and go straight to sleep if any condition is not met
        }
        
      }
    }

    if (all_conditions_met && acddata.keys->state != ACD_LED_OFF) {
      LOG(LOG_DEBUG,"All Conditions met: turning on %s\n", acddata.keys->label);
      SET_IF_CHANGED(acddata.keys->state, ACD_LED_ON, acddata.is_dirty);
    } 

    if (!all_conditions_met || acddata.keys->state != ACD_LED_ON) {
      goto next_wake; // Skip the rest of the loop and go straight to sleep if any condition is not met
    }

    for (acd_key_t *key = acddata.keys->next; key != NULL; key = key->next) {
      
      switch (key->type) {
        case KEY_TYPE_MQTT_TEMP:          
          if (key->index == MASTER_ID) { temp_reading_for_ph = key->value; }// If this is the master temp sensor, also update the temp reading for pH compensation
          LOG(LOG_DEBUG, "MQTT Sensor '%s' current value: %.2f\n", key->label, key->value);
          break;
        case KEY_TYPE_D1W_TEMP:{
          /*
            float new_value = d1w_get_temp(key->data.d1w.id);
            LOG(LOG_DEBUG, "D1W Sensor '%s' read value: %.2f\n", key->label, new_value);
            if (new_value != DEVICE_DISCONNECTED_C) {
              if (key->index == MASTER_ID) {  temp_reading_for_ph = new_value; }// If this is the master temp sensor, also update the temp reading for pH compensation
              SET_IF_CHANGED(key->value, new_value, acddata.is_dirty); 
            } else {
              LOG(LOG_WARNING, "D1W Sensor '%s' read failed (device disconnected)\n", key->label);
            }
              */
        } break;
        case KEY_TYPE_EZO_TEMP: {
          rtd_reading_t temp_reading = rtd_get_reading();
          if (temp_reading.status == EZO_SUCCESS) {
            LOG(LOG_NOTICE,"Temp %s : %.2f°C\n", key->label, temp_reading.value);
            if (key->index == MASTER_ID) { temp_reading_for_ph = temp_reading.value; }// If this is the master temp sensor, also update the temp reading for pH compensation
            SET_IF_CHANGED(key->value, temp_reading.value, acddata.is_dirty);
            SET_IF_CHANGED(key->state, ACD_LED_ON, acddata.is_dirty);
          } else {
            LOG(LOG_WARNING, "EZO Temp Sensor '%s' read failed (status %d)\n", key->label, temp_reading.status);
            SET_IF_CHANGED(key->state, ACD_LED_OFF, acddata.is_dirty);
          }
        } break;
        case KEY_TYPE_EZO_PH: {
          ph_reading_t ph_reading;
          if (_acdconfig_.temp_compensated_ph == false) {
            ph_reading = ph_get_reading();
          } else if ( temp_reading_for_ph != UNKNOWN) {
            ph_reading = ph_get_reading_compensated(temp_reading_for_ph);
          } else {
            LOG(LOG_WARNING, "EZO pH Sensor '%s' skipped compensation because temp is unknown\n", key->label);
            SET_IF_CHANGED(key->state, ACD_LED_OFF, acddata.is_dirty);
            break;
          }
          
          if (ph_reading.status == EZO_SUCCESS) {
            LOG(LOG_NOTICE,"pH %s : %.2f°C\n", key->label, ph_reading.value);
            SET_IF_CHANGED(key->value, ph_reading.value, acddata.is_dirty);
            SET_IF_CHANGED(key->state, ACD_LED_ON, acddata.is_dirty);
          } else {
            LOG(LOG_WARNING, "EZO pH Sensor '%s' read failed (status %d)\n", key->label, ph_reading.status);
            SET_IF_CHANGED(key->state, ACD_LED_OFF, acddata.is_dirty);
          }
        } break;
        case KEY_TYPE_EZO_ORP: {
          orp_reading_t orp_reading = orp_get_reading();
          if (orp_reading.status == EZO_SUCCESS) {
            LOG(LOG_NOTICE,"ORP %s : %.2f mV\n", key->label, orp_reading.value);
            SET_IF_CHANGED(key->value, orp_reading.value, acddata.is_dirty);
            SET_IF_CHANGED(key->state, ACD_LED_ON, acddata.is_dirty);
          } else {
            LOG(LOG_WARNING, "EZO ORP Sensor '%s' read failed (status %d)\n", key->label, orp_reading.status);
            SET_IF_CHANGED(key->state, ACD_LED_OFF, acddata.is_dirty);
          }
         } break;
        default:
          LOG(LOG_WARNING, "Unknown sensor type for sensor '%s'\n", key->label);
      }
    }


next_wake:
    LOG(LOG_NOTICE,"------------\n");
    // Advance the target wake time by one interval
    next_wake.tv_sec += _acdconfig_.sensor_poll_time;
    // Sleep until the next wake time
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_wake, NULL);
  }

  return 0;
}

#ifdef EXAMPLE_CODEß

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