#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <gpiod.h>
#include <stdbool.h>

#include "gpio.h"

// ─── Bus utilities ────────────────────────────────────────────────────────────

// sudo apt install libgpiod-dev

// Iterate /dev/gpiochip* entries — libgpiod v2 removed chip iterators
// so we scan the directory ourselves
void gpio_detect(bool deepscan)
{
  struct dirent *entry;
  DIR *dev = opendir("/dev");
  if (!dev)
  {
    fprintf(stderr, "gpio_detect: cannot open /dev\n");
    return;
  }

  printf("\nDetected GPIO chips:\n");

  while ((entry = readdir(dev)) != NULL)
  {
    if (strncmp(entry->d_name, "gpiochip", 8) != 0)
      continue;

    char chip_path[261];
    snprintf(chip_path, sizeof(chip_path), "/dev/%s", entry->d_name);

    struct gpiod_chip *chip = gpiod_chip_open(chip_path);
    if (!chip) continue;

    struct gpiod_chip_info *info = gpiod_chip_get_info(chip);
    if (!info) { gpiod_chip_close(chip); continue; }

    unsigned int num_lines = gpiod_chip_info_get_num_lines(info);

    printf("  %-16s  label: %-30s  lines: %u\n",
      chip_path,
      gpiod_chip_info_get_label(info),
      num_lines);

    // Print each line — name, consumer, direction
    if(deepscan) {
      for (unsigned int i = 0; i < num_lines; i++)
      {
        struct gpiod_line_info *linfo = gpiod_chip_get_line_info(chip, i);
        if (!linfo) continue;

        const char *name     = gpiod_line_info_get_name(linfo);
        const char *consumer = gpiod_line_info_get_consumer(linfo);
        int         used     = gpiod_line_info_is_used(linfo);
        enum gpiod_line_direction dir = gpiod_line_info_get_direction(linfo);

        printf("    line %3u: %-20s %-20s [%s]%s\n",
          i,
          name     ? name     : "unnamed",
          consumer ? consumer : "unused",
          dir == GPIOD_LINE_DIRECTION_OUTPUT ? "output" : "input",
          used ? " *" : "");

        gpiod_line_info_free(linfo);
      }
      printf("\n");
    }

    gpiod_chip_info_free(info);
    gpiod_chip_close(chip);
  }

  closedir(dev);
  printf("\n");
}

// ─── Line management ─────────────────────────────────────────────────────────

int gpio_open(gpio_handle_t *h, const char *chip_path, int pin,
              gpio_dir_t direction, gpio_active_t active, const char *label)
{
  memset(h, 0, sizeof(*h));
  h->pin       = pin;
  h->direction = direction;
  h->active    = active;
  strncpy(h->label, label ? label : "aquachemd", sizeof(h->label) - 1);

  // Open the chip
  h->chip = gpiod_chip_open(chip_path);
  if (!h->chip)
  {
    fprintf(stderr, "gpio_open: cannot open chip '%s'\n", chip_path);
    return GPIO_ERROR;
  }

  // Build line settings
  struct gpiod_line_settings *settings = gpiod_line_settings_new();
  if (!settings)
  {
    fprintf(stderr, "gpio_open: failed to allocate line settings\n");
    gpiod_chip_close(h->chip);
    h->chip = NULL;
    return GPIO_ERROR;
  }

  if (direction == GPIO_OUTPUT)
  {
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    // Default to de-asserted (relay off) on open
    gpiod_line_settings_set_output_value(settings,
      active == GPIO_ACTIVE_LOW ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
  }
  else
  {
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
  }

  // Build line config and attach settings to our pin offset
  struct gpiod_line_config *line_cfg = gpiod_line_config_new();
  if (!line_cfg)
  {
    gpiod_line_settings_free(settings);
    gpiod_chip_close(h->chip);
    h->chip = NULL;
    return GPIO_ERROR;
  }

  unsigned int offset = (unsigned int)pin;
  if (gpiod_line_config_add_line_settings(line_cfg, &offset, 1, settings) < 0)
  {
    fprintf(stderr, "gpio_open: failed to add line settings for pin %d\n", pin);
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);
    gpiod_chip_close(h->chip);
    h->chip = NULL;
    return GPIO_ERROR;
  }

  // Build request config — sets the consumer label shown by gpioinfo
  struct gpiod_request_config *req_cfg = gpiod_request_config_new();
  if (req_cfg)
    gpiod_request_config_set_consumer(req_cfg, h->label);

  // Request the line
  h->request = gpiod_chip_request_lines(h->chip, req_cfg, line_cfg);

  // Free config objects — request keeps its own copy
  if (req_cfg)  gpiod_request_config_free(req_cfg);
  gpiod_line_config_free(line_cfg);
  gpiod_line_settings_free(settings);

  if (!h->request)
  {
    fprintf(stderr, "gpio_open: failed to request line %d on '%s'\n", pin, chip_path);
    gpiod_chip_close(h->chip);
    h->chip = NULL;
    return GPIO_ERROR;
  }

  return GPIO_SUCCESS;
}

void gpio_close(gpio_handle_t *h)
{
  if (!h) return;
  if (h->request) gpiod_line_request_release(h->request);
  if (h->chip)    gpiod_chip_close(h->chip);
  h->request = NULL;
  h->chip    = NULL;
}

// ─── Read / Write ─────────────────────────────────────────────────────────────

int gpio_write(gpio_handle_t *h, int value)
{
  if (!h || !h->request) return GPIO_ERROR;
  if (h->direction != GPIO_OUTPUT)
  {
    fprintf(stderr, "gpio_write: pin %d is not configured as output\n", h->pin);
    return GPIO_ERROR;
  }

  // Translate logical value to physical, respecting active polarity
  enum gpiod_line_value physical;
  if (h->active == GPIO_ACTIVE_LOW)
    physical = value ? GPIOD_LINE_VALUE_INACTIVE : GPIOD_LINE_VALUE_ACTIVE;
  else
    physical = value ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;

  return gpiod_line_request_set_value(h->request, (unsigned int)h->pin, physical) == 0
    ? GPIO_SUCCESS : GPIO_ERROR;
}

int gpio_read(gpio_handle_t *h)
{
  if (!h || !h->request) return GPIO_ERROR;

  enum gpiod_line_value physical =
    gpiod_line_request_get_value(h->request, (unsigned int)h->pin);

  if (physical == GPIOD_LINE_VALUE_ERROR) return GPIO_ERROR;

  int logical = (physical == GPIOD_LINE_VALUE_ACTIVE) ? 1 : 0;

  // Flip if active-low
  return (h->active == GPIO_ACTIVE_LOW) ? !logical : logical;
}

// ─── Relay helpers ────────────────────────────────────────────────────────────

int relay_on(gpio_handle_t *h)
{
  return gpio_write(h, 1);
}

int relay_off(gpio_handle_t *h)
{
  return gpio_write(h, 0);
}

int relay_is_on(gpio_handle_t *h)
{
  return gpio_read(h);
}
