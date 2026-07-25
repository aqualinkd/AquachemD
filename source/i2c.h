#ifndef I2C_H_
#define I2C_H_

// ─── Generic I2C sensor bus ───────────────────────────────────────────────────
// This module is for I2C sensors — as opposed to the EZO circuits (ezo.c)
// which use an ASCII command protocol, or 1-wire devices (1wire.c) which
// aren't I2C at all. Two quite different wire protocols live here already:
//   - PTE7300 (Sensata): memory-mapped registers, write-then-read
//   - HSC/SSC (Honeywell): no registers at all, just a raw multi-byte read
// Rather than a separate struct per family (which is what ezo.h does, since
// every EZO circuit is addressed by a fixed default address anyway), I2C
// sensors here share ONE instance struct (i2c_sensor_t) and ONE reading
// struct (i2c_reading_t) — see acd_types.h's acd_key_t.data union: this is
// meant to slot in there as a single `i2c_sensor_t i2c;` member that covers
// every sensor family below, current and future, instead of growing the
// union by one struct per part number.
//
// Adding a new I2C sensor family:
//   1. Add a value to i2c_sensor_type_t.
//   2. Write an i2c_sensor_init_<family>() that fills in an i2c_sensor_t.
//   3. Write a static <family>_get_reading(i2c_sensor_t*) in i2c.c and add
//      a case for it in i2c_sensor_get_reading()'s dispatch.
//   4. Add a DUMMY_SENSORS case so it works without hardware.
// If your sensor needs config fields the existing struct doesn't have,
// add them there rather than inventing a parallel struct — that's the
// point of sharing it.

#ifndef I2C_GENERIC_BUS
#define I2C_GENERIC_BUS   "/dev/i2c-1"   // same physical bus as EZO_BUS by default
#endif

// ─── Generic status codes (used by every sensor in this file) ───────────────
// Deliberately mirrors EZO_SUCCESS/EZO_ERROR's role in ezo.h.
#define I2C_SUCCESS       0
#define I2C_ERROR        -1   // open/ioctl/transfer failure (bus-level)
#define I2C_NOT_FOUND    -2   // device didn't ACK its address
#define I2C_TIMEOUT      -3   // conversion not ready in time
#define I2C_CRC_ERROR    -4   // CRC-protected transfer failed (reserved — PTE7300 CRC mode, etc.)
#define I2C_PENDING      -5   // conversion still running / data stale, try again shortly
#define I2C_UNSUPPORTED  -6   // valid call, but not supported by this sensor family

// ─── Generic low-level transport helpers ─────────────────────────────────────
// Register-based transport (PTE7300 and future register-mapped sensors) uses
// Linux i2c-dev's I2C_RDWR ioctl so reads use a proper repeated-start (write
// reg addr, repeated-start, read data) rather than two separate transactions
// with a STOP in between.
//
// Plain transport (HSC/SSC and future free-running sensors) is a bare
// read()/write() after ioctl(I2C_SLAVE) — no register addressing step at all.

int i2c_bus_available(const char *bus_path);

// Register-mapped transport
int i2c_write_reg(const char *bus_path, int address, unsigned char reg,
                   const unsigned char *data, int len);
int i2c_read_reg(const char *bus_path, int address, unsigned char reg,
                  unsigned char *data, int len);

// Plain transport — no register address, just N bytes in or out
int i2c_read_bytes(const char *bus_path, int address, unsigned char *data, int len);
int i2c_write_bytes(const char *bus_path, int address, const unsigned char *data, int len);

// Quick address probe — true if something ACKs at `address` on `bus_path`.
int i2c_probe_address(const char *bus_path, int address);

// Scan the bus and print any devices that ACK, alongside any known-device
// name match from the internal table (see i2c_known_devices in i2c.c).
void i2c_generic_detect(const char *bus_path);

// ─── Unit conversion helpers ──────────────────────────────────────────────────
#define I2C_BAR_TO_PSI(bar)   ((bar) * 14.5037738f)
#define I2C_KPA_TO_PSI(kpa)   ((kpa) * 0.1450377f)


// ─── Shared sensor instance + reading structs ────────────────────────────────

typedef enum {
  I2C_SENSOR_PTE7300 = 0,   // Sensata PTE7300 series (register-mapped)
  I2C_SENSOR_HSC     = 1,   // Honeywell HSC/SSC series (raw read, free-running)
} i2c_sensor_type_t;

// One instance struct for any sensor family in this file. min_value/max_value
// are always in whatever engineering unit you want readings back in (psi,
// bar, etc) and correspond to the physical calibrated span of your specific
// part. min_counts/max_counts are the raw ADC counts that correspond to
// those two points — fixed by the hardware for PTE7300 (its init function
// sets them for you), but part-dependent for HSC/SSC (its "transfer
// function" letter code — see i2c_sensor_init_hsc()).
typedef struct {
  i2c_sensor_type_t type;
  int   address;        // 7-bit I2C address
  float min_value;      // engineering-unit value at min_counts
  float max_value;      // engineering-unit value at max_counts
  int   min_counts;     // raw counts at min_value
  int   max_counts;     // raw counts at max_value
} i2c_sensor_t;

// A reading from any sensor in this file — mirrors ezo.h's ph_reading_t /
// orp_reading_t / prs_reading_t convention (value + status), with an
// optional secondary temperature reading most digital pressure sensors
// also expose.
typedef struct {
  float value;      // primary measurement, in the unit init() was given
  float temp_c;      // secondary/reference temperature, if the sensor provides one
  int   status;       // I2C_SUCCESS / I2C_* error code
} i2c_reading_t;

// Generic operations — dispatch internally based on sensor->type. This is
// the API acd core code should use; it doesn't need to know which sensor
// family it's talking to.
int i2c_sensor_is_connected(i2c_sensor_t *s);
i2c_reading_t i2c_sensor_get_reading(i2c_sensor_t *s);

// Power state controls — only meaningful for some families (PTE7300 has
// real power states; HSC/SSC I2C parts are free-running and don't). Returns
// I2C_UNSUPPORTED for families that don't support the call.
int i2c_sensor_sleep(i2c_sensor_t *s);
int i2c_sensor_reset(i2c_sensor_t *s);


// ─── PTE7300 pressure sensor (Sensata) ───────────────────────────────────────
// Digital gauge pressure sensor, e.g. PTE7300-44DM-0B400SN / -14DM-0B016SN.
// Register protocol per "PTE7300 I2C Pressure Sensor Installation &
// Communication Guide" (Sensata, 2021):
//   - 7-bit address 0x6C (non-CRC register protocol; this driver does not
//     implement the CRC-protected 0xDA variant)
//   - Pressure and bridge-temperature are int16 registers scaled to a fixed
//     ±16000 count span regardless of the sensor's physical range.
//   - A command word must be written to the CMD register to trigger a
//     measurement before reading (i2c_sensor_get_reading() does this for you).
//
// NOTE ON BYTE ORDER: the datasheet excerpt available didn't spell out
// register byte order in extractable text (it's shown in a diagram). This
// driver assumes little-endian (LSB first) — the common convention for this
// class of device. If your first real reading looks wildly wrong (e.g. not
// near ~14.7 psi / 1013 mbar at atmospheric with the sensor open to air),
// flip PTE7300_LITTLE_ENDIAN to 0 in i2c.c and re-test.

#define PTE7300_ADDR_DEFAULT   0x6C   // non-CRC protocol address

#define PTE7300_REG_CMD         0x22   // write: trigger state transitions
#define PTE7300_REG_PRESSURE    0x2E   // int16, ±16000 across configured span
#define PTE7300_REG_TEMP        0x30   // int16, -40..125 C mapped to ±16000
#define PTE7300_REG_STATUS      0x32   // uint16, status/event bits
#define PTE7300_REG_SERIAL_LO   0x50   // uint32 serial, low word
#define PTE7300_REG_SERIAL_HI   0x52   // uint32 serial, high word

#define PTE7300_CMD_IDLE    0x7BBA   // abort conversion, power state idle
#define PTE7300_CMD_START   0x8B93   // start a measurement, power state run
#define PTE7300_CMD_RESET   0xB169   // full power-up reset
#define PTE7300_CMD_SLEEP   0x6C32   // enter sleep (6.5uA typical)

#define PTE7300_WAIT_MS      10   // response time is <1ms typical; generous margin

// Initialise a PTE7300 instance.
//   address:   PTE7300_ADDR_DEFAULT unless you've changed it
//   min_value: engineering-unit reading at the bottom of the sensor's span (usually 0)
//   max_value: engineering-unit reading at the top of the sensor's span
// e.g. for the -14DM-0B016SN (0-16 bar) reported in psi:
//   i2c_sensor_init_pte7300(&s, PTE7300_ADDR_DEFAULT, 0.0f, I2C_BAR_TO_PSI(16.0f));
void i2c_sensor_init_pte7300(i2c_sensor_t *s, int address, float min_value, float max_value);

// PTE7300-specific extras beyond the generic API above.
int i2c_sensor_idle_pte7300(i2c_sensor_t *s);
int i2c_sensor_get_serial_pte7300(i2c_sensor_t *s, unsigned int *serial);


// ─── Honeywell HSC/SSC pressure sensor (TruStability, I2C output) ────────────
// Board-mount digital pressure sensor. Unlike PTE7300 there's no register
// map and no command needed — the device free-runs internally and a plain
// I2C read returns the latest conversion:
//   byte0: status[7:6], pressure[13:8]
//   byte1: pressure[7:0]
//   byte2: temperature[10:3]           (only if you read 4 bytes)
//   byte3: temperature[2:0], don't-care[4:0]
// Per Honeywell's "I2C Comms with Digital Output Pressure Sensors" technical
// note. Pressure is a 14-bit count (0-16383) mapped across the device's
// transfer function — the standard "A" transfer function used by most
// stock part numbers maps 10%-90% of full-scale to Pmin-Pmax
// (1638-14745 counts); other transfer function letter codes (check your
// part's ordering code, e.g. HSCMAND...A vs ...F) use a different count
// span — pass those in if yours differs.
//
// The 7-bit I2C address is set at the factory per your specific order code
// (there's no universal default) — check your part's datasheet page or
// order confirmation. 0x28 is a commonly used address for single-sensor
// setups but is NOT guaranteed for your part.

#define HSC_COUNTS_FULL_SCALE     16383   // 14-bit bridge output, all parts
#define HSC_COUNTS_MIN_DEFAULT     1638   // 10% of full scale — "A" transfer function
#define HSC_COUNTS_MAX_DEFAULT    14745   // 90% of full scale — "A" transfer function

#define HSC_TEMP_COUNTS_MAX        2047   // 11-bit temperature output, all parts
#define HSC_TEMP_MIN_C            -50.0f
#define HSC_TEMP_MAX_C            150.0f

#define HSC_STATUS_MASK            0xC0   // top 2 bits of byte0
#define HSC_STATUS_NORMAL          0x00   // valid data
#define HSC_STATUS_COMMAND_MODE    0x40   // device in command mode — no valid reading
#define HSC_STATUS_STALE           0x80   // data hasn't updated since last read
#define HSC_STATUS_DIAGNOSTIC      0xC0   // sensor fault

// Initialise an HSC/SSC instance with an explicit transfer-function count
// span (use this if your part's ordering code isn't the standard "A"
// transfer function — check the datasheet).
//   address:    factory-set 7-bit I2C address for your part
//   min_value:  engineering-unit value at min_counts (Pmin)
//   max_value:  engineering-unit value at max_counts (Pmax)
//   min_counts/max_counts: transfer function count span, e.g. HSC_COUNTS_MIN_DEFAULT/MAX_DEFAULT
void i2c_sensor_init_hsc(i2c_sensor_t *s, int address, float min_value, float max_value,
                          int min_counts, int max_counts);

// Convenience wrapper for the common "A" (10-90%) transfer function.
void i2c_sensor_init_hsc_default(i2c_sensor_t *s, int address, float min_value, float max_value);

#endif