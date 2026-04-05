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
  _acdconfig_.log_level = LOG_NOTICE;
  _acdconfig_.deamonize = true;
  _acdconfig_.config_file = "/etc/aquachemd.conf";

#ifdef DUMMY_SENSORS
  _acdconfig_.log_level = LOG_DEBUG;
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

  parse_config_file(&acddata);

  start_net_services(&acddata);

  // ── Normal mode — loop reading sensors ──────────────────────────────────────

  clock_gettime(CLOCK_MONOTONIC, &next_wake);
  while (1)
  {
    LOG(LOG_NOTICE,"---\n");
    acddata.temp_reading = rtd_get_reading();

    if (acddata.temp_reading.status == EZO_SUCCESS)
    {
      SET_DIRTY(acddata.is_dirty);
      LOG(LOG_NOTICE,"Temp: %.2f°C\n", acddata.temp_reading.value);
      acddata.ph_reading = ph_get_reading_compensated(acddata.temp_reading.value);
    } else {
      LOG(LOG_NOTICE,"Temp: read failed (status %d)\n", acddata.temp_reading.status);
      acddata.ph_reading = ph_get_reading();
    }

    if (acddata.ph_reading.status == EZO_SUCCESS) {
      SET_DIRTY(acddata.is_dirty);
      LOG(LOG_NOTICE,"pH  : %.2f\n", acddata.ph_reading.value);
    } else {
      LOG(LOG_NOTICE,"pH  : read failed (status %d)\n", acddata.ph_reading.status);
    }

    acddata.orp_reading = orp_get_reading();
    if (acddata.orp_reading.status == EZO_SUCCESS) {
      SET_DIRTY(acddata.is_dirty);
      LOG(LOG_NOTICE,"ORP : %.2f mV\n", acddata.orp_reading.value);
    } else {
      LOG(LOG_NOTICE,"ORP: read failed (status %d)\n", acddata.orp_reading.status);
    }

    LOG(LOG_NOTICE,"---\n");


    // Advance the target wake time by one interval
    next_wake.tv_sec += _acdconfig_.sensor_poll_time;
    // Sleep until the next wake time
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_wake, NULL);
  }

  return 0;
}

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