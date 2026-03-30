#ifndef ONEWIRE_H_
#define ONEWIRE_H_

// Base path for all 1-wire devices on Linux
#define W1_BASE_PATH    "/sys/bus/w1/devices"

// DS18B20 family code prefix — all DS18B20 device dirs start with "28-"
#define W1_DS18B20_PREFIX  "28-"

// Scale factors
#define W1_DS18B20_SCALE   0.001f   // raw integer is millidegrees C
#define W1_GENERIC_SCALE   1.0f     // raw value used as-is by default

// Return codes
#define W1_SUCCESS         0
#define W1_ERROR          -1
#define W1_CRC_ERROR      -2
#define W1_NOT_FOUND      -3

// Sensor type — determines how the raw value is interpreted
typedef enum {
  W1_TYPE_DS18B20  = 0,   // temperature: raw / 1000.0 = degrees C
  W1_TYPE_GENERIC  = 1    // raw value scaled by user-supplied factor
} w1_sensor_type_t;

// A 1-wire sensor handle
// path:    full path to the sensor directory e.g. /sys/bus/w1/devices/28-xxxx
// type:    sensor type, determines default scale
// scale:   multiply raw integer value by this to get engineering units
//          DS18B20: 0.001 (millidegrees -> degrees C)
//          generic: set to whatever the datasheet specifies
// offset:  add this after scaling (for calibration trim)
// label:   human-readable name e.g. "pool_water", "ambient"
typedef struct {
  char              path[256];   // full path to sensor sysfs directory
  w1_sensor_type_t  type;
  float             scale;
  float             offset;
  char              label[32];
} w1_sensor_t;

// Reading result
typedef struct {
  float value;     // scaled + offset value in engineering units
  long  raw;       // raw integer direct from kernel
  int   status;    // W1_SUCCESS or error code
} w1_reading_t;

// ─── Discovery ────────────────────────────────────────────────────────────────

// Scan /sys/bus/w1/devices and print all detected 1-wire devices.
// Shows device ID, family, and current temperature for DS18B20 sensors.
void w1_detect();

// Find all DS18B20 sensors on the bus.
// sensors[]:  caller-supplied array to fill
// max:        size of the array
// Returns number of sensors found, or W1_ERROR
int w1_find_ds18b20(w1_sensor_t *sensors, int max);

// ─── Sensor initialisation ────────────────────────────────────────────────────

// Initialise a DS18B20 handle from its sysfs directory path.
// path:   e.g. "/sys/bus/w1/devices/28-0304949760eb"
//         or the full temperature file path — either is accepted
// label:  human-readable name shown in output
void w1_init_ds18b20(w1_sensor_t *s, const char *path, const char *label);

// Initialise a generic 1-wire sensor handle with custom scale and offset.
// path:   path to the sensor sysfs directory
// scale:  multiply raw value by this to get engineering units
// offset: add after scaling
// label:  human-readable name
void w1_init_generic(w1_sensor_t *s, const char *path,
                     float scale, float offset, const char *label);

// ─── Reading ─────────────────────────────────────────────────────────────────

// Read the sensor value using the simple 'temperature' sysfs file.
// Preferred method — kernel handles CRC internally.
w1_reading_t w1_read(const w1_sensor_t *s);

// Read using the 'w1_slave' file — includes explicit CRC validation.
// Use this if you need to verify data integrity yourself or if the
// 'temperature' file is not available on your kernel version.
w1_reading_t w1_read_with_crc(const w1_sensor_t *s);

// ─── Utility ─────────────────────────────────────────────────────────────────

// Convert Celsius to Fahrenheit
float w1_c_to_f(float celsius);

// Convert Celsius to Kelvin
float w1_c_to_k(float celsius);

// Return a human-readable error string for a W1_* status code
const char *w1_strerror(int status);

#endif
