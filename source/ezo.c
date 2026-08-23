#define _GNU_SOURCE
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>

#include "ezo.h"
#include "utils.h"
#include "i2c.h"

// ─── Real implementation ──────────────────────────────────────────────────────
// All code in this block is compiled only when DUMMY_SENSORS is NOT defined.
// To build with fake sensors: make dummy
// To build normally:          make

#ifndef DUMMY_SENSORS

#include <linux/i2c-dev.h>

// ─── Core I2C functions (private) ────────────────────────────────────────────

static int ezo_open(int address)
{
  int fd = open(I2C_BUS, O_RDWR);
  if (fd < 0) return EZO_ERROR;
  if (ioctl(fd, I2C_SLAVE, address) < 0) { close(fd); return EZO_ERROR; }
  return fd;
}

static int ezo_get_wait_ms(const char *cmd)
{
  if (strncasecmp(cmd, "R", 1) == 0 && strlen(cmd) == 1)
    return EZO_WAIT_READ;
  if (strncasecmp(cmd, "Cal", 3) == 0)
    return EZO_WAIT_CALIBRATE;
  if (strncasecmp(cmd, "D,", 2) == 0 ||
      strncasecmp(cmd, "DC,", 3) == 0 ||
      strncasecmp(cmd, "STOP", 4) == 0 ||
      strncasecmp(cmd, "P", 1) == 0)
    return EZO_WAIT_PUMP;
  return EZO_WAIT_GENERAL;
}

static int ezo_read(int fd, char *response, int len, int wait_ms)
{
  unsigned char buf[32] = {0};
  usleep(wait_ms * 1000);

  if (read(fd, buf, sizeof(buf)) < 0)
    return EZO_ERROR;

  if (buf[0] != EZO_SUCCESS)
    return buf[0];

  strncpy(response, (char *)&buf[1], len - 1);
  response[len - 1] = '\0';
  return EZO_SUCCESS;
}

static int ezo_send_cmd(int fd, const char *cmd)
{
  return write(fd, cmd, strlen(cmd));
}

static int ezo_command(int address, const char *cmd, char *result, int result_len)
{
  int fd = ezo_open(address);
  if (fd < 0) return EZO_ERROR;

  int wait_ms = ezo_get_wait_ms(cmd);
  ezo_send_cmd(fd, cmd);
  int status = ezo_read(fd, result, result_len, wait_ms);

  close(fd);
  return status;
}

// ─── I2C detect ──────────────────────────────────────────────────────────────

typedef struct {
  int         addr;
  const char *name;
} ezo_addr_map_t;

static const ezo_addr_map_t ezo_known_devices[] = {
  { 0x61, "DO"    },
  { 0x62, "ORP"   },
  { 0x63, "pH"    },
  { 0x64, "EC"    },
  { 0x66, "RTD"  },
  { 0x67, "PUMP"  },
  { 0x68, "FLOW"   },
  { 0x69, "CO2"   },
  { 0x6A, "O2"    },
  { 0x6B, "PRS"   },
  { 0x70, "HUM"   },
  { 0,    NULL    }
};

static const char *ezo_name_from_addr(int addr)
{
  for (int i = 0; ezo_known_devices[i].name != NULL; i++)
    if (ezo_known_devices[i].addr == addr)
      return ezo_known_devices[i].name;
  return NULL;
}

static const char *ezo_query_device_type(int addr)
{
  static char type_buf[16];

  int fd = open(I2C_BUS, O_RDWR);
  if (fd < 0) return NULL;
  if (ioctl(fd, I2C_SLAVE, addr) < 0) { close(fd); return NULL; }

  write(fd, "i", 1);
  usleep(EZO_WAIT_GENERAL * 1000);

  unsigned char buf[32] = {0};
  if (read(fd, buf, sizeof(buf)) < 0 || buf[0] != EZO_SUCCESS)
  {
    close(fd);
    return NULL;
  }
  close(fd);

  char *start = strchr((char *)&buf[1], ',');
  if (!start) return NULL;
  start++;
  char *end = strchr(start, ',');
  if (!end) end = start + strlen(start);

  int len = end - start;
  if (len <= 0 || len >= (int)sizeof(type_buf)) return NULL;
  strncpy(type_buf, start, len);
  type_buf[len] = '\0';
  return type_buf;
}

void ezo_i2cdetect()
{
  int fd = open(I2C_BUS, O_RDWR);
  if (fd < 0) { perror("open i2c"); return; }

  printf("\nScanning I2C bus %s...\n\n", I2C_BUS);
  printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");

  int detected[128] = {0};
  int count = 0;

  for (int row = 0; row < 8; row++)
  {
    printf("%02x: ", row * 16);
    for (int col = 0; col < 16; col++)
    {
      int addr = row * 16 + col;
      if (addr < 0x08 || addr > 0x77) { printf("   "); continue; }
      if (ioctl(fd, I2C_SLAVE, addr) < 0) { printf("   "); continue; }

      unsigned char buf;
      if (read(fd, &buf, 1) < 0)
        printf("-- ");
      else
      {
        printf("%02x ", addr);
        detected[count++] = addr;
      }
    }
    printf("\n");
  }
  close(fd);

  if (count == 0) { printf("\nNo devices found.\n"); return; }

  printf("\nDetected devices:\n");
  for (int i = 0; i < count; i++)
  {
    int addr = detected[i];
    const char *known   = ezo_name_from_addr(addr);
    const char *queried = ezo_query_device_type(addr);

    if (queried)
      printf("  0x%02x  confirmed: %-6s  (default addr for: %s)\n",
        addr, queried, known ? known : "unknown");
    else if (known)
      printf("  0x%02x  likely:    %-6s  (by default address, unconfirmed)\n",
        addr, known);
    else {
      const char *known = i2c_name_from_addr(addr);
      if (known)
        printf("  0x%02x  likely: %s (by default address, unconfirmed)\n", addr, known);
      else
        printf("  0x%02x  unknown device\n", addr);
    }
  }
  printf("\n");
}

// ─── Bus availability ─────────────────────────────────────────────────────────

int ezo_bus_available()
{
  int fd = open(I2C_BUS, O_RDWR);
  if (fd < 0) return 0;
  close(fd);
  return 1;
}

// ─── Shared EZO helpers ───────────────────────────────────────────────────────

int ezo_get_info(int address, char *info, int len)
{
  return ezo_command(address, "i", info, len);
}

int ezo_get_status(int address, char *status, int len)
{
  return ezo_command(address, "Status", status, len);
}

int ezo_clear_calibration(int address)
{
  char response[32];
  return ezo_command(address, "Cal,clear", response, sizeof(response));
}

int ezo_get_cal_status(int address, ezo_cal_status_t *cal)
{
  char response[32];
  int status = ezo_command(address, "Cal,?", response, sizeof(response));
  if (status == EZO_SUCCESS)
  {
    int points = 0;
    if (sscanf(response, "?CAL,%d", &points) == 1)
      cal->points = points;
    else
      cal->points = EZO_ERROR;
  }
  return status;
}

int ezo_sleep(int address)
{
  int fd = ezo_open(address);
  if (fd < 0) return EZO_ERROR;
  ezo_send_cmd(fd, "Sleep");
  close(fd);
  return EZO_SUCCESS;
}

// ─── pH sensor ────────────────────────────────────────────────────────────────

ph_reading_t ph_get_reading()
{
  ph_reading_t result = {0.0f, EZO_ERROR};
  char response[32];
  result.status = ezo_command(EZO_PH_ADDR, "R", response, sizeof(response));
  if (result.status == EZO_SUCCESS)
    result.value = atof(response);
  return result;
}

ph_reading_t ph_get_reading_compensated(float temp_c)
{
  ph_reading_t result = {0.0f, EZO_ERROR};
  char cmd[32];
  char response[32];
  snprintf(cmd, sizeof(cmd), "T,%.2f", temp_c);
  ezo_command(EZO_PH_ADDR, cmd, response, sizeof(response));
  result.status = ezo_command(EZO_PH_ADDR, "R", response, sizeof(response));
  if (result.status == EZO_SUCCESS)
    result.value = atof(response);
  return result;
}

// Median of 3 readings — discards outliers from interference spikes
ph_reading_t ph_get_reading_filtered()
{
  float readings[3];
  int valid = 0;
  for (int i = 0; i < 3; i++)
  {
    ph_reading_t r = ph_get_reading();
    if (r.status == EZO_SUCCESS)
      readings[valid++] = r.value;
  }
  if (valid == 0) return (ph_reading_t){0.0f, EZO_ERROR};
  if (valid < 3)  return (ph_reading_t){readings[0], EZO_SUCCESS};
  if (readings[0] > readings[1]) { float t = readings[0]; readings[0] = readings[1]; readings[1] = t; }
  if (readings[1] > readings[2]) { float t = readings[1]; readings[1] = readings[2]; readings[2] = t; }
  if (readings[0] > readings[1]) { float t = readings[0]; readings[0] = readings[1]; readings[1] = t; }
  return (ph_reading_t){readings[1], EZO_SUCCESS};
}

int ph_calibrate(ph_cal_point_t point, float ph_value)
{
  char cmd[32];
  char response[32];
  const char *point_str[] = {"low", "mid", "high"};
  snprintf(cmd, sizeof(cmd), "Cal,%s,%.2f", point_str[point], ph_value);
  return ezo_command(EZO_PH_ADDR, cmd, response, sizeof(response));
}

int ph_calibrate_mid()  { return ph_calibrate(PH_CAL_MID,  PH_REF_MID); }
int ph_calibrate_low()  { return ph_calibrate(PH_CAL_LOW,  PH_REF_LOW); }
int ph_calibrate_high() { return ph_calibrate(PH_CAL_HIGH, PH_REF_HIGH); }

int ph_get_cal_status(ezo_cal_status_t *cal) { return ezo_get_cal_status(EZO_PH_ADDR, cal); }
int ph_clear_calibration()                    { return ezo_clear_calibration(EZO_PH_ADDR); }
int ph_get_info(char *info, int len)          { return ezo_get_info(EZO_PH_ADDR, info, len); }
int ph_get_status(char *status, int len)      { return ezo_get_status(EZO_PH_ADDR, status, len); }
int ph_sleep()                                { return ezo_sleep(EZO_PH_ADDR); }

int ph_calibrate_by_value(float calibrationValue) {
    // Dynamically calculate the midpoints
    float low_mid_boundary  = (PH_REF_LOW + PH_REF_MID) / 2.0f;   // (4.00 + 7.00) / 2 = 5.50f
    float mid_high_boundary = (PH_REF_MID + PH_REF_HIGH) / 2.0f; // (7.00 + 10.00) / 2 = 8.50f

    //Anything below the low/mid midpoint
    if (calibrationValue < low_mid_boundary) {
        return ph_calibrate_low();
    }
    // Anything between the low/mid midpoint and mid/high midpoint
    else if (calibrationValue >= low_mid_boundary && calibrationValue <= mid_high_boundary) {
        return ph_calibrate_mid();
    }
    // Anything above the mid/high midpoint
    else {
        return ph_calibrate_high();
    }
}

// ─── ORP sensor ───────────────────────────────────────────────────────────────

orp_reading_t orp_get_reading()
{
  orp_reading_t result = {0.0f, EZO_ERROR};
  char response[32];
  result.status = ezo_command(EZO_ORP_ADDR, "R", response, sizeof(response));
  if (result.status == EZO_SUCCESS)
    result.value = atof(response);
  return result;
}

int orp_calibrate(float mv_value)
{
  char cmd[32];
  char response[32];
  snprintf(cmd, sizeof(cmd), "Cal,%.2f", mv_value);
  return ezo_command(EZO_ORP_ADDR, cmd, response, sizeof(response));
}

int orp_get_cal_status(ezo_cal_status_t *cal) { return ezo_get_cal_status(EZO_ORP_ADDR, cal); }
int orp_clear_calibration()                    { return ezo_clear_calibration(EZO_ORP_ADDR); }
int orp_get_info(char *info, int len)          { return ezo_get_info(EZO_ORP_ADDR, info, len); }
int orp_get_status(char *status, int len)      { return ezo_get_status(EZO_ORP_ADDR, status, len); }
int orp_sleep()                                { return ezo_sleep(EZO_ORP_ADDR); }

// ─── RTD temperature sensor ───────────────────────────────────────────────────

static int rtd_command(const char *cmd, char *result, int result_len)
{
  int fd = ezo_open(EZO_RTD_ADDR);
  if (fd < 0) return EZO_ERROR;
  int wait_ms = (strncasecmp(cmd, "R", 1) == 0 && strlen(cmd) == 1)
    ? EZO_WAIT_RTD : ezo_get_wait_ms(cmd);
  ezo_send_cmd(fd, cmd);
  int status = ezo_read(fd, result, result_len, wait_ms);
  close(fd);
  return status;
}

rtd_reading_t rtd_get_reading()
{
  rtd_reading_t result = {0.0f, RTD_SCALE_CELSIUS, EZO_ERROR};
  char response[32];
  result.status = rtd_command("R", response, sizeof(response));
  if (result.status == EZO_SUCCESS)
    result.value = atof(response);
  return result;
}

int rtd_set_scale(rtd_scale_t scale)
{
  char response[32];
  const char *cmd;
  switch (scale)
  {
    case RTD_SCALE_FAHRENHEIT: cmd = "S,f"; break;
    case RTD_SCALE_KELVIN:     cmd = "S,k"; break;
    default:                   cmd = "S,c"; break;
  }
  return rtd_command(cmd, response, sizeof(response));
}

int rtd_calibrate(float known_temp)
{
  char cmd[32];
  char response[32];
  snprintf(cmd, sizeof(cmd), "Cal,%.2f", known_temp);
  return rtd_command(cmd, response, sizeof(response));
}

int rtd_get_cal_status(ezo_cal_status_t *cal) { return ezo_get_cal_status(EZO_RTD_ADDR, cal); }
int rtd_clear_calibration()                    { return ezo_clear_calibration(EZO_RTD_ADDR); }
int rtd_get_info(char *info, int len)          { return ezo_get_info(EZO_RTD_ADDR, info, len); }
int rtd_get_status(char *status, int len)      { return ezo_get_status(EZO_RTD_ADDR, status, len); }
int rtd_sleep()                                { return ezo_sleep(EZO_RTD_ADDR); }

// ─── Pressure sensor (EZO-PRS) ────────────────────────────────────────────────

prs_reading_t prs_get_reading()
{
  prs_reading_t result = {0.0f, EZO_ERROR};
  char response[32];
  result.status = ezo_command(EZO_PRS_ADDR, "R", response, sizeof(response));
  if (result.status == EZO_SUCCESS)
    result.value = atof(response);
  return result;
}

int prs_calibrate(float psi_value)
{
  char cmd[32];
  char response[32];
  snprintf(cmd, sizeof(cmd), "Cal,%.2f", psi_value);
  return ezo_command(EZO_PRS_ADDR, cmd, response, sizeof(response));
}

int prs_calibrate_zero()
{
  char response[32];
  return ezo_command(EZO_PRS_ADDR, "Cal,0", response, sizeof(response));
}

int prs_get_cal_status(ezo_cal_status_t *cal) { return ezo_get_cal_status(EZO_PRS_ADDR, cal); }
int prs_clear_calibration()                   { return ezo_clear_calibration(EZO_PRS_ADDR); }
int prs_get_info(char *info, int len)         { return ezo_get_info(EZO_PRS_ADDR, info, len); }
int prs_get_status(char *status, int len)     { return ezo_get_status(EZO_PRS_ADDR, status, len); }
int prs_sleep()                               { return ezo_sleep(EZO_PRS_ADDR); }

// ─── Dosing pump (EZO-PMP) ────────────────────────────────────────────────────

int pump_dose(float ml)
{
  char cmd[32]; char response[32];
  snprintf(cmd, sizeof(cmd), "D,%.2f", ml);
  return ezo_command(EZO_PMP_ADDR, cmd, response, sizeof(response));
}

int pump_dose_continuous(float ml_per_min)
{
  char cmd[32]; char response[32];
  snprintf(cmd, sizeof(cmd), "DC,%.2f", ml_per_min);
  return ezo_command(EZO_PMP_ADDR, cmd, response, sizeof(response));
}

int pump_dose_volume_at_rate(float ml, float ml_per_min)
{
  char cmd[32]; char response[32];
  snprintf(cmd, sizeof(cmd), "D,%.2f,%.2f", ml, ml_per_min);
  return ezo_command(EZO_PMP_ADDR, cmd, response, sizeof(response));
}

int pump_stop()
{
  char response[32];
  return ezo_command(EZO_PMP_ADDR, "X", response, sizeof(response));
}

int pump_pause()
{
  char response[32];
  return ezo_command(EZO_PMP_ADDR, "P", response, sizeof(response));
}

int pump_resume()
{
  char response[32];
  return ezo_command(EZO_PMP_ADDR, "P", response, sizeof(response));
}

int pump_set_direction(pump_dir_t dir)
{
  char response[32];
  return ezo_command(EZO_PMP_ADDR,
    dir == PUMP_REVERSE ? "R" : "F",
    response, sizeof(response));
}

pump_status_t pump_get_status()
{
  pump_status_t result = {0.0f, 0, EZO_ERROR};
  char response[32];
  result.status = ezo_command(EZO_PMP_ADDR, "?STATUS", response, sizeof(response));
  if (result.status == EZO_SUCCESS)
  {
    char state; float vol;
    if (sscanf(response, "?STATUS,%c,%f", &state, &vol) == 2)
    {
      result.volume_ml  = vol;
      result.is_pumping = (state == 'P') ? 1 : 0;
    }
  }
  return result;
}

float pump_get_total_volume()
{
  char response[32];
  if (ezo_command(EZO_PMP_ADDR, "?V", response, sizeof(response)) != EZO_SUCCESS)
    return -1.0f;
  float total = 0.0f;
  sscanf(response, "?V,%f", &total);
  return total;
}

int pump_clear_total_volume()
{
  char response[32];
  return ezo_command(EZO_PMP_ADDR, "Clear", response, sizeof(response));
}

int pump_get_info(char *info, int len) { return ezo_get_info(EZO_PMP_ADDR, info, len); }
int pump_sleep()                       { return ezo_sleep(EZO_PMP_ADDR); }

//#endif // DUMMY_SENSORS
#endif // ifndef DUMMY_SENSORS

// ─── Dummy sensor implementation ──────────────────────────────────────────────
// Compiled only when DUMMY_SENSORS is defined: make dummy
// All functions return plausible pool chemistry values with small random drift.
// No I2C hardware required — safe to run on any machine.

#ifdef DUMMY_SENSORS

/*
// Small random float drift in range [-range, +range]
static float dummy_drift(float range)
{
  return ((float)(rand() % 1000) / 1000.0f - 0.5f) * 2.0f * range;
}
*/
// ─── Bus / detect (dummy) ─────────────────────────────────────────────────────

int ezo_bus_available()
{
  static int seeded = 0;
  if (!seeded) { srand((unsigned int)time(NULL)); seeded = 1; }
  return 1;   // always available in dummy mode
}

void ezo_i2cdetect()
{
  printf("\n[DUMMY] Simulated I2C bus scan on %s\n\n", I2C_BUS);
  printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
  printf("60:              62 63          67 68               \n\n");
  printf("Detected devices:\n");
  printf("  0x62  confirmed: ORP    (default addr for: ORP)\n");
  printf("  0x63  confirmed: pH     (default addr for: pH)\n");
  printf("  0x67  confirmed: PUMP   (default addr for: PUMP)\n");
  printf("  0x68  confirmed: RTD    (default addr for: RTD)\n");
  printf("\n");
}

void simulate_read_time()
{
  // Sleep for 1 second
  sleep(1);
}

// ─── Shared EZO helpers (dummy) ───────────────────────────────────────────────

int ezo_get_info(int address, char *info, int len)
{
  simulate_read_time();
  snprintf(info, len, "?I,DUMMY,1.00");
  return EZO_SUCCESS;
}

int ezo_get_status(int address, char *status, int len)
{
  simulate_read_time();
  snprintf(status, len, "?STATUS,P,5.00");
  return EZO_SUCCESS;
}

int ezo_clear_calibration(int address)  { return EZO_SUCCESS; }
int ezo_sleep(int address)              { return EZO_SUCCESS; }

int ezo_get_cal_status(int address, ezo_cal_status_t *cal)
{
  simulate_read_time();
  cal->points = 3;
  return EZO_SUCCESS;
}

// ─── pH sensor (dummy) ────────────────────────────────────────────────────────

ph_reading_t ph_get_reading()
{
  simulate_read_time();
  return (ph_reading_t){ 7.20f + dummy_drift(0.15f), EZO_SUCCESS };
}

ph_reading_t ph_get_reading_compensated(float temp_c)
{
  simulate_read_time();
  (void)temp_c;
  // between 7.1 and 8.1 = Use 7.6 as the base, and 0.5 as the range
  return (ph_reading_t){ 7.60f + dummy_drift(0.5f), EZO_SUCCESS };
}

ph_reading_t ph_get_reading_filtered()
{
  simulate_read_time();
  // Call dummy ph_get_reading 3x and return median — same logic as real version
  float r[3];
  for (int i = 0; i < 3; i++) r[i] = ph_get_reading().value;
  if (r[0] > r[1]) { float t = r[0]; r[0] = r[1]; r[1] = t; }
  if (r[1] > r[2]) { float t = r[1]; r[1] = r[2]; r[2] = t; }
  if (r[0] > r[1]) { float t = r[0]; r[0] = r[1]; r[1] = t; }
  return (ph_reading_t){ r[1], EZO_SUCCESS };
}

int ph_calibrate(ph_cal_point_t point, float ph_value)
{
  simulate_read_time();
  const char *point_str[] = {"low", "mid", "high"};
  printf("[DUMMY] pH calibrate %s at %.2f — OK\n", point_str[point], ph_value);
  return EZO_SUCCESS;
}

int ph_calibrate_mid()  { return ph_calibrate(PH_CAL_MID,  PH_REF_MID); }
int ph_calibrate_low()  { return ph_calibrate(PH_CAL_LOW,  PH_REF_LOW); }
int ph_calibrate_high() { return ph_calibrate(PH_CAL_HIGH,  PH_REF_HIGH); }

int ph_get_cal_status(ezo_cal_status_t *cal) { return ezo_get_cal_status(EZO_PH_ADDR, cal); }
int ph_clear_calibration()                    { return ezo_clear_calibration(EZO_PH_ADDR); }
int ph_get_info(char *info, int len)          { return ezo_get_info(EZO_PH_ADDR, info, len); }
int ph_get_status(char *status, int len)      { return ezo_get_status(EZO_PH_ADDR, status, len); }
int ph_sleep()                                { return ezo_sleep(EZO_PH_ADDR); }

int ph_calibrate_by_value(float calibrationValue) {
    // Dynamically calculate the midpoints
    float low_mid_boundary  = (PH_REF_LOW + PH_REF_MID) / 2.0f;   // (4.00 + 7.00) / 2 = 5.50f
    float mid_high_boundary = (PH_REF_MID + PH_REF_HIGH) / 2.0f; // (7.00 + 10.00) / 2 = 8.50f

    //Anything below the low/mid midpoint
    if (calibrationValue < low_mid_boundary) {
        return ph_calibrate_low();
    }
    // Anything between the low/mid midpoint and mid/high midpoint
    else if (calibrationValue >= low_mid_boundary && calibrationValue <= mid_high_boundary) {
        return ph_calibrate_mid();
    }
    // Anything above the mid/high midpoint
    else {
        return ph_calibrate_high();
    }
}

// ─── ORP sensor (dummy) ───────────────────────────────────────────────────────

orp_reading_t orp_get_reading()
{
  simulate_read_time();
  return (orp_reading_t){ 650.0f + dummy_drift(20.0f), EZO_SUCCESS };
}

int orp_calibrate(float mv_value)
{
  simulate_read_time();
  printf("[DUMMY] ORP calibrate at %.2f mV — OK\n", mv_value);
  return EZO_SUCCESS;
}

int orp_get_cal_status(ezo_cal_status_t *cal) { return ezo_get_cal_status(EZO_ORP_ADDR, cal); }
int orp_clear_calibration()                    { return ezo_clear_calibration(EZO_ORP_ADDR); }
int orp_get_info(char *info, int len)          { return ezo_get_info(EZO_ORP_ADDR, info, len); }
int orp_get_status(char *status, int len)      { return ezo_get_status(EZO_ORP_ADDR, status, len); }
int orp_sleep()                                { return ezo_sleep(EZO_ORP_ADDR); }

// ─── RTD temperature sensor (dummy) ──────────────────────────────────────────

rtd_reading_t rtd_get_reading()
{
  simulate_read_time();
  // Return between 28 and 29
  //return (rtd_reading_t){ 28.5f + dummy_drift(0.5f), RTD_SCALE_CELSIUS, EZO_SUCCESS };

  // Return between -5 and 75
  //return (rtd_reading_t){ 35.0f + dummy_drift(40.0f), RTD_SCALE_CELSIUS, EZO_SUCCESS };

  return (rtd_reading_t){ 30.0f + dummy_drift(1.0f), RTD_SCALE_CELSIUS, EZO_SUCCESS };
  
}

int rtd_set_scale(rtd_scale_t scale)
{
  const char *scales[] = {"Celsius", "Fahrenheit", "Kelvin"};
  printf("[DUMMY] RTD scale set to %s — OK\n", scales[scale]);
  return EZO_SUCCESS;
}

int rtd_calibrate(float known_temp)
{
  simulate_read_time();
  printf("[DUMMY] RTD calibrate at %.2f — OK\n", known_temp);
  return EZO_SUCCESS;
}

int rtd_get_cal_status(ezo_cal_status_t *cal) { return ezo_get_cal_status(EZO_RTD_ADDR, cal); }
int rtd_clear_calibration()                    { return ezo_clear_calibration(EZO_RTD_ADDR); }
int rtd_get_info(char *info, int len)          { return ezo_get_info(EZO_RTD_ADDR, info, len); }
int rtd_get_status(char *status, int len)      { return ezo_get_status(EZO_RTD_ADDR, status, len); }
int rtd_sleep()                                { return ezo_sleep(EZO_RTD_ADDR); }

// ─── Pressure sensor (dummy) ──────────────────────────────────────────────────

prs_reading_t prs_get_reading()
{
  simulate_read_time();
  // Return a plausible clean pool filter pressure around 15.0 psi
  return (prs_reading_t){ 15.0f + dummy_drift(1.0f), EZO_SUCCESS };
}

int prs_calibrate(float psi_value)
{
  simulate_read_time();
  printf("[DUMMY] PRS calibrate at %.2f psi — OK\n", psi_value);
  return EZO_SUCCESS;
}

int prs_calibrate_zero()
{
  simulate_read_time();
  printf("[DUMMY] PRS calibrate zero (atmospheric) — OK\n");
  return EZO_SUCCESS;
}

int prs_get_cal_status(ezo_cal_status_t *cal) { return ezo_get_cal_status(EZO_PRS_ADDR, cal); }
int prs_clear_calibration()                   { return ezo_clear_calibration(EZO_PRS_ADDR); }
int prs_get_info(char *info, int len)         { return ezo_get_info(EZO_PRS_ADDR, info, len); }
int prs_get_status(char *status, int len)     { return ezo_get_status(EZO_PRS_ADDR, status, len); }
int prs_sleep()                               { return ezo_sleep(EZO_PRS_ADDR); }

// ─── Dosing pump (dummy) ──────────────────────────────────────────────────────

int pump_dose(float ml)
{
  printf("[DUMMY] Pump dose %.2f ml — OK\n", ml);
  return EZO_SUCCESS;
}

int pump_dose_continuous(float ml_per_min)
{
  printf("[DUMMY] Pump continuous at %.2f ml/min — OK\n", ml_per_min);
  return EZO_SUCCESS;
}

int pump_dose_volume_at_rate(float ml, float ml_per_min)
{
  printf("[DUMMY] Pump %.2f ml at %.2f ml/min — OK\n", ml, ml_per_min);
  return EZO_SUCCESS;
}

int pump_stop()                        { printf("[DUMMY] Pump stop\n");   return EZO_SUCCESS; }
int pump_pause()                       { printf("[DUMMY] Pump pause\n");  return EZO_SUCCESS; }
int pump_resume()                      { printf("[DUMMY] Pump resume\n"); return EZO_SUCCESS; }

int pump_set_direction(pump_dir_t dir)
{
  printf("[DUMMY] Pump direction: %s\n", dir == PUMP_REVERSE ? "reverse" : "forward");
  return EZO_SUCCESS;
}

pump_status_t pump_get_status()
{
  simulate_read_time();
  return (pump_status_t){ 0.0f, 0, EZO_SUCCESS };
}

float pump_get_total_volume()          { return 0.0f; }
int   pump_clear_total_volume()        { return EZO_SUCCESS; }
int   pump_get_info(char *info, int len) { return ezo_get_info(EZO_PMP_ADDR, info, len); }
int   pump_sleep()                     { return ezo_sleep(EZO_PMP_ADDR); }

#endif // DUMMY_SENSORS