#pragma once

#include "driver/gpio.h"
#include <stdbool.h>

typedef struct {
    gpio_num_t pin;
    bool state;
} status_led_t;

// Initializes the GPIO configuration for status LED as output
void led_init(status_led_t* led, gpio_num_t pin);

// Turns status LED ON (true) or OFF (false)
void led_set(status_led_t* led, bool on);

// Toggles status LED state
void led_toggle(status_led_t* led);
