#pragma once

#include "driver/gpio.h"
#include <stdbool.h>

typedef struct {
    gpio_num_t pin;
} button_t;

// Initializes GPIO pin with pull-up configurations
void button_init(button_t* btn, gpio_num_t pin);

// Returns true if button is currently pressed (pin reads LOW)
bool button_is_pressed(button_t* btn);
