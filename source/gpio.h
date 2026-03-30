#ifndef GPIO_H_
#define GPIO_H_

#include <gpiod.h>

// GPIO direction
typedef enum {
  GPIO_INPUT  = 0,
  GPIO_OUTPUT = 1
} gpio_dir_t;

// GPIO active state — for relays: most are active-high, some active-low
typedef enum {
  GPIO_ACTIVE_HIGH = 0,   // line is active when value = 1 (most relays)
  GPIO_ACTIVE_LOW  = 1    // line is active when value = 0 (some relay boards)
} gpio_active_t;

// GPIO return codes
#define GPIO_SUCCESS   0
#define GPIO_ERROR    -1

// Handle for a single GPIO line (libgpiod v2)
// chip_path:  full device path e.g. "/dev/gpiochip0"
// pin:        GPIO line offset on that chip
// request:    active line request — holds the line and keeps it owned
typedef struct {
  struct gpiod_chip         *chip;
  struct gpiod_line_request *request;
  int                        pin;
  gpio_dir_t                 direction;
  gpio_active_t              active;
  char                       label[32];   // consumer label shown by gpioinfo
} gpio_handle_t;

// ─── Bus utilities ────────────────────────────────────────────────────────────

// List all GPIO chips and their lines — equivalent to gpiodetect + gpioinfo.
// Useful for finding chip paths and pin numbers on an unknown board.
//void gpio_detect();
void gpio_detect(bool deepscan);

// ─── Line management ─────────────────────────────────────────────────────────

// Open a GPIO line for input or output.
// chip_path:  full device path e.g. "/dev/gpiochip0"
// pin:        GPIO line offset on that chip
// direction:  GPIO_INPUT or GPIO_OUTPUT
// active:     GPIO_ACTIVE_HIGH or GPIO_ACTIVE_LOW
// label:      consumer name shown by gpioinfo (e.g. "ph_pump", "cl_pump")
// Returns GPIO_SUCCESS or GPIO_ERROR
int gpio_open(gpio_handle_t *h, const char *chip_path, int pin,
              gpio_dir_t direction, gpio_active_t active, const char *label);

// Release the GPIO line and close the chip
void gpio_close(gpio_handle_t *h);

// ─── Read / Write ─────────────────────────────────────────────────────────────

// Write a logical value (0 or 1) to an output line.
// Respects active polarity — 1 always means "asserted" regardless of wiring.
// Returns GPIO_SUCCESS or GPIO_ERROR
int gpio_write(gpio_handle_t *h, int value);

// Read a logical value from an input line.
// Returns 0 or 1 (respects active polarity), or GPIO_ERROR on failure
int gpio_read(gpio_handle_t *h);

// ─── Relay helpers ────────────────────────────────────────────────────────────

// Turn relay on (assert the line — handles active-low transparently)
int relay_on(gpio_handle_t *h);

// Turn relay off (de-assert the line)
int relay_off(gpio_handle_t *h);

// Returns 1 if relay is on, 0 if off, GPIO_ERROR on failure
int relay_is_on(gpio_handle_t *h);

#endif
