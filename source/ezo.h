
#ifndef EZO_H_
#define EZO_H_

// I2C bus
#define I2C_BUS         "/dev/i2c-1"

// EZO device I2C addresses (default from Atlas Scientific)
#define EZO_DO_ADDR     0x61  // Dissolved Oxygen (DO)
#define EZO_ORP_ADDR    0x62  // Oxidation-Reduction Potential (ORP)
#define EZO_PH_ADDR     0x63  // pH Circuit
#define EZO_EC_ADDR     0x64  // Conductivity (EC)
#define EZO_FLO_ADDR    0x66  // Flow Meter (EZO-FLOW)
#define EZO_PMP_ADDR    0x67  // Dosing Pump (EZO-PMP)
#define EZO_RTD_ADDR    0x68  // Temperature (RTD)
#define EZO_CO2_ADDR    0x69  // Carbon Dioxide (CO2)
#define EZO_O2_ADDR     0x6A  // Oxygen (O2)
#define EZO_PRS_ADDR    0x6B  // Pressure (EZO-PRS)
#define EZO_HUM_ADDR    0x70  // Humidity (EZO-HUM)

// EZO response status codes
#define EZO_SUCCESS     1
#define EZO_PENDING     254
#define EZO_SYNTAX_ERR  2
#define EZO_NO_DATA     255

// Command wait times in milliseconds
#define EZO_WAIT_READ       900
#define EZO_WAIT_CALIBRATE  1600
#define EZO_WAIT_GENERAL    300
#define EZO_WAIT_RTD        600   // RTD only needs 600ms for a reading
#define EZO_WAIT_PUMP       100   // pump command acknowledgement

// ─── Enums ────────────────────────────────────────────────────────────────────

// Calibration point options for pH
typedef enum {
  PH_CAL_LOW  = 0,   // typically pH 4.00
  PH_CAL_MID  = 1,   // typically pH 7.00
  PH_CAL_HIGH = 2    // typically pH 10.00
} ph_cal_point_t;

// Temperature scale for RTD sensor
typedef enum {
  RTD_SCALE_CELSIUS    = 0,
  RTD_SCALE_FAHRENHEIT = 1,
  RTD_SCALE_KELVIN     = 2
} rtd_scale_t;

// Dosing pump direction
typedef enum {
  PUMP_FORWARD = 0,
  PUMP_REVERSE = 1
} pump_dir_t;

// ─── Structs ──────────────────────────────────────────────────────────────────

// Calibration status
typedef struct {
  int points;        // number of calibration points confirmed by device
} ezo_cal_status_t;

// pH reading result
typedef struct {
  float value;       // pH value e.g. 7.32
  int   status;      // EZO status code
} ph_reading_t;

// ORP reading result
typedef struct {
  float value;       // ORP in millivolts e.g. 350.5
  int   status;      // EZO status code
} orp_reading_t;

// RTD temperature reading result
typedef struct {
  float      value;   // temperature in the configured scale e.g. 25.00
  rtd_scale_t scale;  // scale the device is currently set to
  int        status;  // EZO status code
} rtd_reading_t;

// Dosing pump status
typedef struct {
  float volume_ml;    // volume dispensed in the last dose (ml)
  int   is_pumping;   // 1 if pump is currently running, 0 if idle
  int   status;       // EZO status code
} pump_status_t;

// ─── Bus utilities ────────────────────────────────────────────────────────────
int  ezo_bus_available();
void ezo_i2cdetect();

// ─── Generic EZO helpers (use when adding new sensor types) ──────────────────
int ezo_get_info(int address, char *info, int len);
int ezo_get_status(int address, char *status, int len);
int ezo_clear_calibration(int address);
int ezo_get_cal_status(int address, ezo_cal_status_t *cal);
int ezo_sleep(int address);

// ─── pH sensor ────────────────────────────────────────────────────────────────
ph_reading_t ph_get_reading();
ph_reading_t ph_get_reading_compensated(float temp_c);
ph_reading_t ph_get_reading_filtered();
int ph_calibrate(ph_cal_point_t point, float ph_value);
int ph_calibrate_low();
int ph_calibrate_mid();
int ph_calibrate_high();
int ph_get_cal_status(ezo_cal_status_t *cal);
int ph_clear_calibration();
int ph_get_info(char *info, int len);
int ph_get_status(char *status, int len);
int ph_sleep();

// ─── ORP sensor ───────────────────────────────────────────────────────────────
orp_reading_t orp_get_reading();
int orp_calibrate(float mv_value);
int orp_get_cal_status(ezo_cal_status_t *cal);
int orp_clear_calibration();
int orp_get_info(char *info, int len);
int orp_get_status(char *status, int len);
int orp_sleep();

// ─── RTD temperature sensor ───────────────────────────────────────────────────
rtd_reading_t rtd_get_reading();
int rtd_set_scale(rtd_scale_t scale);
int rtd_calibrate(float known_temp);
int rtd_get_cal_status(ezo_cal_status_t *cal);
int rtd_clear_calibration();
int rtd_get_info(char *info, int len);
int rtd_get_status(char *status, int len);
int rtd_sleep();

// ─── Dosing pump (EZO-PMP) ────────────────────────────────────────────────────
// Dose a fixed volume (ml) — pump runs until volume is dispensed then stops
int pump_dose(float ml);

// Dose at a continuous rate (ml/min) — runs until pump_stop() is called
int pump_dose_continuous(float ml_per_min);

// Dose a fixed volume at a specific rate (ml at ml/min)
int pump_dose_volume_at_rate(float ml, float ml_per_min);

// Stop pumping immediately
int pump_stop();

// Pause/resume pump (retains remaining dose target)
int pump_pause();
int pump_resume();

// Set pump direction
int pump_set_direction(pump_dir_t dir);

// Get current pump status and volume dispensed
pump_status_t pump_get_status();

// Get total volume dispensed since last clear (ml)
float pump_get_total_volume();

// Clear the total volume counter
int pump_clear_total_volume();

int pump_get_info(char *info, int len);
int pump_sleep();

#endif