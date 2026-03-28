/****************

Make sure to have the below in /boot/firmware/config.txt to enable I2C on Raspberry Pi:

dtparam=i2c_arm=on
dtparam=i2c_arm_baudrate=100000

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
  ./aquachemd scan                  - scan I2C bus and identify devices
  ./aquachemd calibrate mid         - calibrate pH mid-point (7.00)
  ./aquachemd calibrate low         - calibrate pH low-point (4.00)
  ./aquachemd calibrate high        - calibrate pH high-point (10.00)
  ./aquachemd calibrate orp <mv>    - calibrate ORP at specified mV value

*/

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>

#include "aquachem.h"
#include "gpio.h"

// ─── Main ─────────────────────────────────────────────────────────────────────

int main(int argc, char *argv[])
{
  if (!ezo_bus_available())
  {
    fprintf(stderr, "Error: I2C bus %s not available.\n", I2C_BUS);
    fprintf(stderr, "Run: sudo modprobe i2c-dev\n");
    fprintf(stderr, "To persist across reboots: echo i2c-dev | sudo tee /etc/modules-load.d/i2c.conf\n");
    return 1;
  }

  // ── Scan mode ───────────────────────────────────────────────────────────────
  if (argc >= 2 && strcasecmp(argv[1], "scan") == 0)
  {
    ezo_i2cdetect();
    //gpio_detect();
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

  // ── Unknown argument ────────────────────────────────────────────────────────
  if (argc >= 2)
  {
    fprintf(stderr, "Unknown argument '%s'\n", argv[1]);
    fprintf(stderr, "Usage: %s [scan | calibrate <low|mid|high> | calibrate orp <mv>]\n", argv[0]);
    return 1;
  }

  // ── Normal mode — loop reading sensors ──────────────────────────────────────
  while (1)
  {

    // If we have a I2C Temps ensor
    
    ph_reading_t ph;
    rtd_reading_t temp = rtd_get_reading();

    if (temp.status == EZO_SUCCESS)
    {
      printf("Temp: %.2f°C\n", temp.value);
      ph = ph_get_reading_compensated(temp.value);
    } else {
      printf("Temp: read failed (status %d)\n", temp.status);
      ph = ph_get_reading();
    }
    //ph_reading_t ph = ph_get_reading();

    if (ph.status == EZO_SUCCESS)
      printf("pH  : %.2f\n", ph.value);
    else
      printf("pH  : read failed (status %d)\n", ph.status);
    

    orp_reading_t orp = orp_get_reading();
    if (orp.status == EZO_SUCCESS)
      printf("ORP : %.2f mV\n", orp.value);
    else
      printf("ORP: read failed (status %d)\n", orp.status);

    printf("---\n");

    sleep(5);
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
#endif