#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>

#include "i2c.h"
#include "utils.h"

// ─── Real implementation ──────────────────────────────────────────────────────
// All code in this block is compiled only when DUMMY_SENSORS is NOT defined.
// To build with fake sensors: make dummy
// To build normally:          make

#ifndef DUMMY_SENSORS

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

// Set to 0 if bring-up shows PTE7300 registers are actually big-endian on
// your unit — see the header comment on PTE7300 byte order. Quick check
// with i2c-tools from the shell (adjust bus/addr as needed):
//   i2cset -y 1 0x6c 0x22 0x93 0x8b i   # write START, LSB-first as this driver does
//   i2cget -y 1 0x6c 0x2e w             # i2c_smbus word read is LSB-first too
// If the pressure reading only makes sense with the two data bytes swapped,
// flip this to 0.
#define PTE7300_LITTLE_ENDIAN 1

// ─── Generic transport ────────────────────────────────────────────────────────

int i2c_bus_available(const char *bus_path)
{
  int fd = open(bus_path, O_RDWR);
  if (fd < 0) return 0;
  close(fd);
  return 1;
}

int i2c_write_reg(const char *bus_path, int address, unsigned char reg,
                   const unsigned char *data, int len)
{
  int fd = open(bus_path, O_RDWR);
  if (fd < 0) return I2C_ERROR;

  unsigned char buf[1 + 32];
  if (len > 32) { close(fd); return I2C_ERROR; }
  buf[0] = reg;
  if (len > 0) memcpy(&buf[1], data, len);

  struct i2c_msg msg = { .addr = (unsigned short)address, .flags = 0,
                          .len = (unsigned short)(1 + len), .buf = buf };
  struct i2c_rdwr_ioctl_data rdwr = { .msgs = &msg, .nmsgs = 1 };

  int ret = ioctl(fd, I2C_RDWR, &rdwr);
  close(fd);
  return (ret < 0) ? I2C_ERROR : I2C_SUCCESS;
}

int i2c_read_reg(const char *bus_path, int address, unsigned char reg,
                  unsigned char *data, int len)
{
  int fd = open(bus_path, O_RDWR);
  if (fd < 0) return I2C_ERROR;

  struct i2c_msg msgs[2];
  msgs[0].addr  = (unsigned short)address;
  msgs[0].flags = 0;
  msgs[0].len   = 1;
  msgs[0].buf   = &reg;

  msgs[1].addr  = (unsigned short)address;
  msgs[1].flags = I2C_M_RD;
  msgs[1].len   = (unsigned short)len;
  msgs[1].buf   = data;

  struct i2c_rdwr_ioctl_data rdwr = { .msgs = msgs, .nmsgs = 2 };

  int ret = ioctl(fd, I2C_RDWR, &rdwr);
  close(fd);
  return (ret < 0) ? I2C_ERROR : I2C_SUCCESS;
}

int i2c_read_bytes(const char *bus_path, int address, unsigned char *data, int len)
{
  int fd = open(bus_path, O_RDWR);
  if (fd < 0) return I2C_ERROR;
  if (ioctl(fd, I2C_SLAVE, address) < 0) { close(fd); return I2C_ERROR; }

  int ret = read(fd, data, len);
  close(fd);
  return (ret == len) ? I2C_SUCCESS : I2C_ERROR;
}

int i2c_write_bytes(const char *bus_path, int address, const unsigned char *data, int len)
{
  int fd = open(bus_path, O_RDWR);
  if (fd < 0) return I2C_ERROR;
  if (ioctl(fd, I2C_SLAVE, address) < 0) { close(fd); return I2C_ERROR; }

  int ret = write(fd, data, len);
  close(fd);
  return (ret == len) ? I2C_SUCCESS : I2C_ERROR;
}

int i2c_probe_address(const char *bus_path, int address)
{
  int fd = open(bus_path, O_RDWR);
  if (fd < 0) return 0;
  if (ioctl(fd, I2C_SLAVE, address) < 0) { close(fd); return 0; }

  unsigned char dummy;
  int ok = (read(fd, &dummy, 1) >= 0);
  close(fd);
  return ok;
}

// ─── Known-device table (extend this as you add more generic I2C sensors) ───
// Only sensors with a fixed/default factory address are worth listing here —
// HSC/SSC addresses are order-code specific, so they're deliberately absent.

typedef struct {
  int         addr;
  const char *name;
} i2c_addr_map_t;

static const i2c_addr_map_t i2c_known_devices[] = {
  { PTE7300_ADDR_DEFAULT, "PTE7300 pressure (non-CRC)" },
  { 0,                    NULL                          }
};

static const char *i2c_name_from_addr(int addr)
{
  for (int i = 0; i2c_known_devices[i].name != NULL; i++)
    if (i2c_known_devices[i].addr == addr)
      return i2c_known_devices[i].name;
  return NULL;
}

void i2c_generic_detect(const char *bus_path)
{
  int fd = open(bus_path, O_RDWR);
  if (fd < 0) { perror("open i2c"); return; }

  printf("\nScanning I2C bus %s (generic sensors)...\n\n", bus_path);
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
    const char *known = i2c_name_from_addr(addr);
    if (known)
      printf("  0x%02x  likely: %s (by default address, unconfirmed)\n", addr, known);
    else
      printf("  0x%02x  unknown device (could be HSC/SSC — those have no fixed default)\n", addr);
  }
  printf("\n");
}

// ─── Shared scaling helper ─────────────────────────────────────────────────
// Linear map from raw counts to engineering units — used by every sensor
// family below (PTE7300's ±16000 span, HSC/SSC's transfer function span,
// and both sensors' temperature outputs).

static float i2c_scale(long raw, long min_counts, long max_counts, float min_value, float max_value)
{
  return min_value + (float)(raw - min_counts) * (max_value - min_value) / (float)(max_counts - min_counts);
}

// ─── PTE7300 pressure sensor ──────────────────────────────────────────────────

static int pte7300_write_cmd(i2c_sensor_t *s, unsigned int cmd)
{
  unsigned char data[2];
#if PTE7300_LITTLE_ENDIAN
  data[0] = (unsigned char)(cmd & 0xFF);
  data[1] = (unsigned char)((cmd >> 8) & 0xFF);
#else
  data[0] = (unsigned char)((cmd >> 8) & 0xFF);
  data[1] = (unsigned char)(cmd & 0xFF);
#endif
  return i2c_write_reg(I2C_GENERIC_BUS, s->address, PTE7300_REG_CMD, data, 2);
}

static int pte7300_read_i16(i2c_sensor_t *s, unsigned char reg, short *out)
{
  unsigned char data[2];
  int status = i2c_read_reg(I2C_GENERIC_BUS, s->address, reg, data, 2);
  if (status != I2C_SUCCESS) return status;

#if PTE7300_LITTLE_ENDIAN
  *out = (short)(data[0] | (data[1] << 8));
#else
  *out = (short)((data[0] << 8) | data[1]);
#endif
  return I2C_SUCCESS;
}

static int pte7300_read_u16(i2c_sensor_t *s, unsigned char reg, unsigned short *out)
{
  unsigned char data[2];
  int status = i2c_read_reg(I2C_GENERIC_BUS, s->address, reg, data, 2);
  if (status != I2C_SUCCESS) return status;

#if PTE7300_LITTLE_ENDIAN
  *out = (unsigned short)(data[0] | (data[1] << 8));
#else
  *out = (unsigned short)((data[0] << 8) | data[1]);
#endif
  return I2C_SUCCESS;
}

void i2c_sensor_init_pte7300(i2c_sensor_t *s, int address, float min_value, float max_value)
{
  s->type       = I2C_SENSOR_PTE7300;
  s->address    = address;
  s->min_value  = min_value;
  s->max_value  = max_value;
  s->min_counts = -16000;   // fixed by hardware, not part-dependent
  s->max_counts =  16000;
}

static i2c_reading_t pte7300_get_reading(i2c_sensor_t *s)
{
  i2c_reading_t result = { 0.0f, 0.0f, I2C_ERROR };

  int status = pte7300_write_cmd(s, PTE7300_CMD_START);
  if (status != I2C_SUCCESS) { result.status = status; return result; }

  usleep(PTE7300_WAIT_MS * 1000);

  short raw_pressure = 0, raw_temp = 0;
  unsigned short raw_status = 0;

  status = pte7300_read_i16(s, PTE7300_REG_PRESSURE, &raw_pressure);
  if (status != I2C_SUCCESS) { result.status = status; return result; }

  status = pte7300_read_i16(s, PTE7300_REG_TEMP, &raw_temp);
  if (status != I2C_SUCCESS) { result.status = status; return result; }

  pte7300_read_u16(s, PTE7300_REG_STATUS, &raw_status);   // best-effort, non-fatal

  result.value   = i2c_scale(raw_pressure, s->min_counts, s->max_counts, s->min_value, s->max_value);
  result.temp_c  = i2c_scale(raw_temp, -16000, 16000, -40.0f, 125.0f);
  result.status  = I2C_SUCCESS;

  (void)raw_status;   // decode further here if you need CRC4/event bits later
  return result;
}

static int pte7300_sleep(i2c_sensor_t *s)  { return pte7300_write_cmd(s, PTE7300_CMD_SLEEP); }
static int pte7300_reset(i2c_sensor_t *s)  { return pte7300_write_cmd(s, PTE7300_CMD_RESET); }

int i2c_sensor_idle_pte7300(i2c_sensor_t *s)
{
  if (s->type != I2C_SENSOR_PTE7300) return I2C_UNSUPPORTED;
  return pte7300_write_cmd(s, PTE7300_CMD_IDLE);
}

int i2c_sensor_get_serial_pte7300(i2c_sensor_t *s, unsigned int *serial)
{
  if (s->type != I2C_SENSOR_PTE7300) return I2C_UNSUPPORTED;

  unsigned short lo, hi;
  int status = pte7300_read_u16(s, PTE7300_REG_SERIAL_LO, &lo);
  if (status != I2C_SUCCESS) return status;
  status = pte7300_read_u16(s, PTE7300_REG_SERIAL_HI, &hi);
  if (status != I2C_SUCCESS) return status;

  *serial = ((unsigned int)hi << 16) | lo;
  return I2C_SUCCESS;
}

// ─── Honeywell HSC/SSC pressure sensor ───────────────────────────────────────

void i2c_sensor_init_hsc(i2c_sensor_t *s, int address, float min_value, float max_value,
                          int min_counts, int max_counts)
{
  s->type       = I2C_SENSOR_HSC;
  s->address    = address;
  s->min_value  = min_value;
  s->max_value  = max_value;
  s->min_counts = min_counts;
  s->max_counts = max_counts;
}

void i2c_sensor_init_hsc_default(i2c_sensor_t *s, int address, float min_value, float max_value)
{
  i2c_sensor_init_hsc(s, address, min_value, max_value, HSC_COUNTS_MIN_DEFAULT, HSC_COUNTS_MAX_DEFAULT);
}

static i2c_reading_t hsc_get_reading(i2c_sensor_t *s)
{
  i2c_reading_t result = { 0.0f, 0.0f, I2C_ERROR };

  // No command needed — the device free-runs; just read the last conversion.
  // 4 bytes gets both pressure and temperature in one transaction.
  unsigned char buf[4];
  int status = i2c_read_bytes(I2C_GENERIC_BUS, s->address, buf, sizeof(buf));
  if (status != I2C_SUCCESS) { result.status = status; return result; }

  unsigned char status_bits = buf[0] & HSC_STATUS_MASK;
  if (status_bits == HSC_STATUS_DIAGNOSTIC)   { result.status = I2C_ERROR;   return result; }
  if (status_bits == HSC_STATUS_COMMAND_MODE) { result.status = I2C_ERROR;   return result; }
  if (status_bits == HSC_STATUS_STALE)        { result.status = I2C_PENDING; /* fall through, value still returned below */ }

  unsigned int raw_pressure = ((buf[0] & 0x3F) << 8) | buf[1];                       // 14-bit
  unsigned int raw_temp     = ((unsigned int)buf[2] << 3) | (buf[3] >> 5);           // 11-bit

  result.value  = i2c_scale(raw_pressure, s->min_counts, s->max_counts, s->min_value, s->max_value);
  result.temp_c = i2c_scale(raw_temp, 0, HSC_TEMP_COUNTS_MAX, HSC_TEMP_MIN_C, HSC_TEMP_MAX_C);

  if (status_bits == HSC_STATUS_NORMAL) result.status = I2C_SUCCESS;
  return result;
}

// ─── Generic dispatch ─────────────────────────────────────────────────────────

int i2c_sensor_is_connected(i2c_sensor_t *s)
{
  // Same underlying check regardless of family — both just need an ACK.
  return i2c_probe_address(I2C_GENERIC_BUS, s->address);
}

i2c_reading_t i2c_sensor_get_reading(i2c_sensor_t *s)
{
  switch (s->type)
  {
    case I2C_SENSOR_PTE7300: return pte7300_get_reading(s);
    case I2C_SENSOR_HSC:     return hsc_get_reading(s);
    default:                 return (i2c_reading_t){ 0.0f, 0.0f, I2C_ERROR };
  }
}

int i2c_sensor_sleep(i2c_sensor_t *s)
{
  switch (s->type)
  {
    case I2C_SENSOR_PTE7300: return pte7300_sleep(s);
    case I2C_SENSOR_HSC:     return I2C_UNSUPPORTED;   // free-running, no sleep command over I2C
    default:                 return I2C_ERROR;
  }
}

int i2c_sensor_reset(i2c_sensor_t *s)
{
  switch (s->type)
  {
    case I2C_SENSOR_PTE7300: return pte7300_reset(s);
    case I2C_SENSOR_HSC:     return I2C_UNSUPPORTED;   // no reset command over I2C
    default:                 return I2C_ERROR;
  }
}

#endif // ifndef DUMMY_SENSORS

// ─── Dummy sensor implementation ──────────────────────────────────────────────
// Compiled only when DUMMY_SENSORS is defined: make dummy
// Returns plausible values with small random drift, no hardware required.

#ifdef DUMMY_SENSORS

int i2c_bus_available(const char *bus_path) { (void)bus_path; return 1; }

void i2c_generic_detect(const char *bus_path)
{
  (void)bus_path;
  printf("\n[DUMMY] Simulated generic I2C bus scan\n\n");
  printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
  printf("60:                          6c                    \n\n");
  printf("Detected devices:\n");
  printf("  0x6c  likely: PTE7300 pressure (non-CRC) (by default address, unconfirmed)\n\n");
}

int i2c_write_reg(const char *bus_path, int address, unsigned char reg,
                   const unsigned char *data, int len)
{
  (void)bus_path; (void)address; (void)reg; (void)data; (void)len;
  return I2C_SUCCESS;
}

int i2c_read_reg(const char *bus_path, int address, unsigned char reg,
                  unsigned char *data, int len)
{
  (void)bus_path; (void)address; (void)reg;
  memset(data, 0, len);
  return I2C_SUCCESS;
}

int i2c_read_bytes(const char *bus_path, int address, unsigned char *data, int len)
{
  (void)bus_path; (void)address;
  memset(data, 0, len);
  return I2C_SUCCESS;
}

int i2c_write_bytes(const char *bus_path, int address, const unsigned char *data, int len)
{
  (void)bus_path; (void)address; (void)data; (void)len;
  return I2C_SUCCESS;
}

int i2c_probe_address(const char *bus_path, int address)
{
  (void)bus_path;
  return (address == PTE7300_ADDR_DEFAULT);
}

// ─── Sensor instance setup (dummy) — same as real, no hardware touched ──────

void i2c_sensor_init_pte7300(i2c_sensor_t *s, int address, float min_value, float max_value)
{
  s->type       = I2C_SENSOR_PTE7300;
  s->address    = address;
  s->min_value  = min_value;
  s->max_value  = max_value;
  s->min_counts = -16000;
  s->max_counts =  16000;
}

void i2c_sensor_init_hsc(i2c_sensor_t *s, int address, float min_value, float max_value,
                          int min_counts, int max_counts)
{
  s->type       = I2C_SENSOR_HSC;
  s->address    = address;
  s->min_value  = min_value;
  s->max_value  = max_value;
  s->min_counts = min_counts;
  s->max_counts = max_counts;
}

void i2c_sensor_init_hsc_default(i2c_sensor_t *s, int address, float min_value, float max_value)
{
  i2c_sensor_init_hsc(s, address, min_value, max_value, HSC_COUNTS_MIN_DEFAULT, HSC_COUNTS_MAX_DEFAULT);
}

int i2c_sensor_is_connected(i2c_sensor_t *s) { (void)s; return 1; }

i2c_reading_t i2c_sensor_get_reading(i2c_sensor_t *s)
{
  // Plausible clean pool filter pressure, same idea as prs_get_reading() in
  // ezo.c's dummy block — centered around 15 psi (or the unit's midpoint if
  // your span isn't psi-ish) with a little drift, clamped to the configured span.
  float mid   = (s->min_value + s->max_value) / 2.0f;
  float value = (s->max_value - s->min_value > 5.0f) ? 15.0f + dummy_drift(1.0f) : mid + dummy_drift((s->max_value - s->min_value) * 0.02f);
  if (value < s->min_value) value = s->min_value;
  if (value > s->max_value) value = s->max_value;

  return (i2c_reading_t){ value, 25.0f + dummy_drift(1.0f), I2C_SUCCESS };
}

int i2c_sensor_sleep(i2c_sensor_t *s)
{
  return (s->type == I2C_SENSOR_PTE7300) ? I2C_SUCCESS : I2C_UNSUPPORTED;
}

int i2c_sensor_reset(i2c_sensor_t *s)
{
  return (s->type == I2C_SENSOR_PTE7300) ? I2C_SUCCESS : I2C_UNSUPPORTED;
}

int i2c_sensor_idle_pte7300(i2c_sensor_t *s)
{
  return (s->type == I2C_SENSOR_PTE7300) ? I2C_SUCCESS : I2C_UNSUPPORTED;
}

int i2c_sensor_get_serial_pte7300(i2c_sensor_t *s, unsigned int *serial)
{
  if (s->type != I2C_SENSOR_PTE7300) return I2C_UNSUPPORTED;
  *serial = 0xDEADBEEF;
  return I2C_SUCCESS;
}

#endif // DUMMY_SENSORS