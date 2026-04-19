
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <gpiod.h>
#include <poll.h>
#include <sys/eventfd.h> // NEW: For zero-overhead thread signaling

#include "aquachemd.h"
#include "acd_types.h"
#include "utils.h"
#include "gpio.h"

#define MAX_GPIO_FDS 20

static pthread_t gpio_thread_id = 0;
static volatile bool run_gpio_monitor = false;
static int shutdown_fd = -1; // NEW: The shutdown signal file descriptor


/*
To use _ll_mutex, define it globally in aquachem.h
// Define and initialize the mutex globally
//pthread_mutex_t _ll_mutex = PTHREAD_MUTEX_INITIALIZER;

// Then declare here as extern
//extern pthread_mutex_t _ll_mutex;
*/

void *gpio_monitor_worker(void *ptr);

void start_gpio_monitor(struct aquachemdata *acddata) {
    if (run_gpio_monitor) return;

    // Create the event file descriptor
    shutdown_fd = eventfd(0, EFD_NONBLOCK);
    if (shutdown_fd < 0) {
        LOG(LOG_ERR, "GPIO: Failed to create eventfd\n");
        return;
    }

    run_gpio_monitor = true;
    if (pthread_create(&gpio_thread_id, NULL, gpio_monitor_worker, (void*)acddata) != 0) {
        run_gpio_monitor = false;
        close(shutdown_fd);
    }
}

void stop_gpio_monitor() {
    if (!run_gpio_monitor) return;
    run_gpio_monitor = false;

    if (gpio_thread_id != 0) {
        // Send a 64-bit integer into the eventfd. 
        // This instantly wakes up the poll() loop!
        uint64_t msg = 1;
        write(shutdown_fd, &msg, sizeof(uint64_t));
        
        pthread_join(gpio_thread_id, NULL);
        gpio_thread_id = 0;
    }
    
    if (shutdown_fd >= 0) {
        close(shutdown_fd);
        shutdown_fd = -1;
    }
}



// --- 1. Clean Context Structure ---
// Bundles the metadata so we aren't juggling parallel arrays
typedef struct {
    void *parent_obj;
    gpio_handle_t *handle;
    bool is_condition;
} monitor_ctx_t;

/*
// --- 2. The Setup Helper ---
static void register_line(gpio_handle_t *gh, void *obj, bool is_cond, 
                          struct pollfd *fds, monitor_ctx_t *ctx, int *count) {
    if (*count >= MAX_GPIO_FDS || !gh || !gh->request) return;

    int fd = gpiod_line_request_get_fd(gh->request);
    if (fd >= 0) {
        fds[*count].fd = fd;
        fds[*count].events = POLLIN; // Wait for data to read
        
        ctx[*count].parent_obj = obj;
        ctx[*count].handle = gh;
        ctx[*count].is_condition = is_cond;
        
        (*count)++;
    } else {
        LOG(LOG_ERR, "GPIO: Invalid FD for pin %d\n", gh->pin);
    }
}
*/

void *gpio_monitor_worker(void *ptr) {
    struct aquachemdata *acddata = (struct aquachemdata *)ptr;
    struct gpiod_edge_event_buffer *event_buffer;
    struct pollfd poll_fds[MAX_GPIO_FDS + 1];
    monitor_ctx_t ctx_map[MAX_GPIO_FDS]; 
    int num_lines = 0;

    LOG(LOG_NOTICE, "GPIO: Starting hardware monitor thread\n");

    //pthread_mutex_lock(&_ll_mutex);
    
    // 1. Register Keys (Outputs) - Monitor for ERRORS ONLY
    for (acd_key_t *curr = acddata->keys; curr; curr = curr->next) {
        if (curr->type == KEY_TYPE_GPIO_DOSER && curr->data.gpio.request) {
            int fd = gpiod_line_request_get_fd(curr->data.gpio.request);
            if (fd >= 0) {
                poll_fds[num_lines].fd = fd;
                poll_fds[num_lines].events = 0; // We don't want event triggers, just errors
                ctx_map[num_lines] = (monitor_ctx_t){curr, &curr->data.gpio, false};
                num_lines++;
            }
        }
    }

    // 2. Register Conditions (Inputs) - Monitor for EVENTS + ERRORS
    for (acd_condition_t *curr = acddata->conditions; curr; curr = curr->next) {
        if (curr->type == COND_GPIO && curr->data.gpio.request) {
            int fd = gpiod_line_request_get_fd(curr->data.gpio.request);
            if (fd >= 0) {
                poll_fds[num_lines].fd = fd;
                poll_fds[num_lines].events = POLLIN; // Wake up on edge changes
                ctx_map[num_lines] = (monitor_ctx_t){curr, &curr->data.gpio, true};
                num_lines++;
            }
        }
    }
    //pthread_mutex_unlock(&_ll_mutex);

    // Add shutdown handle
    int shutdown_idx = num_lines;
    poll_fds[shutdown_idx] = (struct pollfd){shutdown_fd, POLLIN, 0};
    
    event_buffer = gpiod_edge_event_buffer_new(16);

    while (run_gpio_monitor) {
    int ret = poll(poll_fds, num_lines + 1, -1);
    if (ret < 0) {
        if (errno == EINTR) continue;
        break;
    }

    if (poll_fds[shutdown_idx].revents & POLLIN) break;

    for (int i = 0; i < num_lines; i++) {
        // --- CATCH HARDWARE ERRORS ---
        if (poll_fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            LOG(LOG_ERR, "GPIO CRITICAL: Hardware failure on Pin %d\n", ctx_map[i].handle->pin);
            continue;
        }

        // --- HANDLE INPUT EVENTS (Conditions/Sensors) ---
        if (ctx_map[i].is_condition && (poll_fds[i].revents & POLLIN)) {
            // Drain the libgpiod edge events so the interrupt clears
            if (gpiod_line_request_read_edge_events(ctx_map[i].handle->request, event_buffer, 16) > 0) {
                
                // Use our new domain-specific function
                // This handles ACTIVE_LOW and REQ_ON/OFF internally
                int met = sensor_is_met(ctx_map[i].handle);
                
                if (met != GPIO_ERROR) {
                    acd_condition_t *c = (acd_condition_t *)ctx_map[i].parent_obj;
                    
                    // Only act if the state actually changed
                    if (c->met != (bool)met) {
                        c->met = (bool)met;
                        
                        LOG(LOG_INFO, "GPIO Condition Change: %s is now %s\n", 
                            c->label, 
                            c->met ? "SATISFIED" : "NOT MET");
                            
                        SET_DIRTY(acddata->is_dirty);
                    }
                }
            }
        }
    }
}

    gpiod_edge_event_buffer_free(event_buffer);
    return NULL;
}
















#ifdef DO_NOT_COMPILE


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <pthread.h>
#include <gpiod.h>
#include <poll.h>
#include <sys/eventfd.h> // NEW: For zero-overhead thread signaling

#include "aquachemd.h"
#include "acd_types.h"
#include "utils.h"
#include "gpio.h"
#include "config.h"



static pthread_t gpio_thread_id = 0;
static volatile bool run_gpio_monitor = false;

// Forward declaration of the worker
void *gpio_monitor_worker(void *ptr);

static pthread_t gpio_thread_id = 0;
static volatile bool run_gpio_monitor = false;
static int shutdown_fd = -1; // NEW: The shutdown signal file descriptor

// --- 1. Clean Context Structure ---
// Bundles the metadata so we aren't juggling parallel arrays
typedef struct {
    void *parent_obj;
    gpio_handle_t *handle;
    bool is_condition;
} monitor_ctx_t;

// --- 2. The Setup Helper ---
static void register_line(gpio_handle_t *gh, void *obj, bool is_cond, 
                          struct pollfd *fds, monitor_ctx_t *ctx, int *count) {
    if (*count >= MAX_GPIO_FDS || !gh || !gh->request) return;

    int fd = gpiod_line_request_get_fd(gh->request);
    if (fd >= 0) {
        fds[*count].fd = fd;
        fds[*count].events = POLLIN; // Wait for data to read
        
        ctx[*count].parent_obj = obj;
        ctx[*count].handle = gh;
        ctx[*count].is_condition = is_cond;
        
        (*count)++;
    } else {
        LOG(LOG_ERR, "GPIO: Invalid FD for pin %d\n", gh->pin);
    }
}

/**
 * start_gpio_monitor: Spawns the background thread to watch GPIO pins.
 */
void start_gpio_monitor(struct aquachemdata *acddata) {
    if (run_gpio_monitor) {
        LOG(LOG_WARNING, "GPIO: Monitor already running.\n");
        return;
    }

    run_gpio_monitor = true;
    
    if (pthread_create(&gpio_thread_id, NULL, gpio_monitor_worker, (void*)acddata) != 0) {
        LOG(LOG_ERR, "GPIO: Failed to create monitor thread: %s\n", strerror(errno));
        run_gpio_monitor = false;
        return;
    }

    LOG(LOG_NOTICE, "GPIO: Monitor thread started successfully.\n");
}

/**
 * stop_gpio_monitor: Signals the thread to exit and joins it.
 */
void stop_gpio_monitor() {
    if (!run_gpio_monitor) return;

    LOG(LOG_NOTICE, "GPIO: Stopping monitor thread...\n");
    
    run_gpio_monitor = false;

    if (gpio_thread_id != 0) {
        // Since poll() is blocking with -1 (forever), we use pthread_cancel
        // to wake the thread up immediately.
        pthread_cancel(gpio_thread_id);
        pthread_join(gpio_thread_id, NULL);
        gpio_thread_id = 0;
    }

    LOG(LOG_NOTICE, "GPIO: Monitor thread stopped.\n");
}

/**
 * gpio_monitor_worker: A single thread that waits for events on all 
 * registered GPIO lines in the acd_key_t linked list.
 */
#define MAX_GPIO_FDS 20 // Combined limit

void *gpio_monitor_worker(void *ptr) {
    struct aquachemdata *acddata = (struct aquachemdata *)ptr;
    struct gpiod_edge_event_buffer *event_buffer;
    struct pollfd poll_fds[MAX_GPIO_FDS];
    
    // This map stores the pointer to the actual object (key or condition)
    void *obj_map[MAX_GPIO_FDS];
    // This tells the loop how to cast the pointer in obj_map
    bool is_condition[MAX_GPIO_FDS]; 

    int num_lines = 0;

    LOG(LOG_INFO, "GPIO: Starting monitor thread\n");

    // Helper to add to poll list
    auto add_to_monitor = [&](void *obj, gpio_handle_t *gh, bool cond) {
        if (num_lines >= MAX_GPIO_FDS) return;
        int fd = gpiod_line_request_get_fd(gh->request);
        if (fd >= 0) {
            poll_fds[num_lines].fd = fd;
            poll_fds[num_lines].events = POLLIN;
            obj_map[num_lines] = obj;
            is_condition[num_lines] = cond;
            num_lines++;
            return;
        }
        LOG(LOG_ERR, "GPIO: failed to add monitoring of pin %d\n", gh->pin);
    };

    // 1. Load Keys
    for (acd_key_t *curr = acddata->keys; curr; curr = curr->next) {
        if (curr->type == KEY_TYPE_GPIO_DOSER && curr->data.gpio.request) {
            add_to_monitor(curr, &curr->data.gpio, false);
            LOG(LOG_DEBUG, "GPIO: Monitoring Key %s on pin %d\n", curr->label, curr->data.gpio.pin);
        }
    }

    // 2. Load Conditions
    for (acd_condition_t *curr = acddata->conditions; curr; curr = curr->next) {
        if (curr->type == COND_GPIO && curr->data.gpio.request) {
            add_to_monitor(curr, &curr->data.gpio, true);
            LOG(LOG_DEBUG, "GPIO: Monitoring Condition %s on pin %d\n", curr->label, curr->data.gpio.pin);
        }
    }

    if (num_lines == 0) return NULL;

    event_buffer = gpiod_edge_event_buffer_new(16);

    while (run_gpio_monitor) {
        int ret = poll(poll_fds, num_lines, 1000); // 1s timeout for clean exit check
        if (ret <= 0) continue; 

        for (int i = 0; i < num_lines; i++) {
            if (poll_fds[i].revents & POLLIN) {
                // Determine which handle we are reading
                gpio_handle_t *h;
                if (is_condition[i]) {
                    h = &((acd_condition_t *)obj_map[i])->data.gpio;
                } else {
                    h = &((acd_key_t *)obj_map[i])->data.gpio;
                }

                if (gpiod_line_request_read_edge_events(h->request, event_buffer, 16) > 0) {
                    struct gpiod_edge_event *ev = gpiod_edge_event_buffer_get_event(event_buffer, 0);
                    int type = gpiod_edge_event_get_event_type(ev);
                    bool high = (type == GPIOD_EDGE_EVENT_RISING_EDGE);

                    if (is_condition[i]) {
                        acd_condition_t *c = (acd_condition_t *)obj_map[i];
                        c->met = (high == c->target_state); // Logic check
                        LOG(LOG_NOTICE, "GPIO Cond: %s is now %s (Met: %d)\n", c->label, high ? "HIGH":"LOW", c->met);
                    } else {
                        acd_key_t *k = (acd_key_t *)obj_map[i];
                        k->state = high ? ON : OFF;
                        LOG(LOG_NOTICE, "GPIO Key: %s is now %s\n", k->label, high ? "HIGH":"LOW");
                    }
                    SET_DIRTY(acddata->is_dirty);
                }
            }
        }
    }

    gpiod_edge_event_buffer_free(event_buffer);
    return NULL;
}




 /*
void *gpio_monitor_worker(void *ptr) {
    struct aquachemdata *acddata = (struct aquachemdata *)ptr;
    struct gpiod_edge_event_buffer *event_buffer;
    struct pollfd poll_fds[MAX_GPIO_KEYS]; // Define a sensible max
    acd_key_t *key_map[MAX_GPIO_KEYS];
    acd_condition_t *condition_map[MAX_GPIO_KEYS];
    int num_lines = 0;

    LOG(LOG_INFO, "GPIO: Starting monitor thread\n");

    // 1. Initialize the poll array from your linked list
    for (acd_key_t *curr = acddata->keys; curr != NULL; curr = curr->next) {
        if (curr->type == KEY_TYPE_GPIO_DOSER && curr->data.gpio.request != NULL) {
            int fd = gpiod_line_request_get_fd(curr->data.gpio.request);
            if (fd >= 0) {
                poll_fds[num_lines].fd = fd;
                poll_fds[num_lines].events = POLLIN;
                key_map[num_lines] = curr;
                num_lines++;
                LOG(LOG_DEBUG, "GPIO: Added pin %d to monitor for %s\n",curr->data.gpio.pin, curr->label);
            } else {
              LOG(LOG_ERR, "GPIO: failed to add monitoring of pin %d\n",curr->data.gpio.pin);
            }
        }
        if (num_lines >= MAX_GPIO_KEYS) break;
    }

    for (acd_condition_t *curr = acddata->conditions; curr != NULL; curr = curr->next) {
        if (curr->type == COND_GPIO && curr->data.gpio.request != NULL) {
            int fd = gpiod_line_request_get_fd(curr->data.gpio.request);
            if (fd >= 0) {
                poll_fds[num_lines].fd = fd;
                poll_fds[num_lines].events = POLLIN;
                condition_map[num_lines] = curr;
                num_lines++;
                LOG(LOG_DEBUG, "GPIO: Added sensor pin %d to monitor %s\n",curr->data.gpio.pin, curr->label);
            } else {
              LOG(LOG_ERR, "GPIO: failed to add monitoring of pin %d\n",curr->data.gpio.pin);
            }
        }
        if (num_lines >= MAX_GPIO_KEYS) break;
    }

    if (num_lines == 0) {
        LOG(LOG_WARNING, "GPIO: No GPIO lines found to monitor. Exiting thread.\n");
        return NULL;
    }

    event_buffer = gpiod_edge_event_buffer_new(16);

    while (1) {
        // 2. Block here until ANY pin in the list changes. 
        // Zero CPU overhead while waiting.
        int ret = poll(poll_fds, num_lines, -1); 

        if (ret < 0) {
            if (errno == EINTR) continue;
            LOG(LOG_ERR, "GPIO: Poll error: %s\n", strerror(errno));
            break;
        }

        // 3. Check which FD triggered the event
        for (int i = 0; i < num_lines; i++) {
            if (poll_fds[i].revents & POLLIN) {
                acd_key_t *k = key_map[i];
                
                // Read the event to clear the interrupt
                if (gpiod_line_request_read_edge_events(k->data.gpio.request, event_buffer, 16) > 0) {
                    struct gpiod_edge_event *ev = gpiod_edge_event_buffer_get_event(event_buffer, 0);
                    int type = gpiod_edge_event_get_event_type(ev);

                    LOG(LOG_NOTICE, "GPIO Event: %s (%s) is now %s\n", k->label, k->ID, (type == GPIOD_EDGE_EVENT_RISING_EDGE) ? "HIGH" : "LOW");
                    // 4. Update the key state based on the hardware event
                    // Assuming your acd_state_t handles ON/OFF or HIGH/LOW
                    //k->state = (type == GPIOD_EDGE_EVENT_RISING_EDGE) ? ON : OFF;
                    
                    //LOG(LOG_NOTICE, "GPIO Event: %s (%s) is now %s\n", k->label, k->ID, (k->state == ON) ? "HIGH" : "LOW");

                    // Trigger your existing logic (e.g., notify WebUI or MQTT)
                    //SET_DIRTY(acddata->is_dirty);
                }
            }
        }
    }

    gpiod_edge_event_buffer_free(event_buffer);
    return NULL;
}
    */


#endif