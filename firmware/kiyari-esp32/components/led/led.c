#include "led.h"

void led_init(status_led_t* led, gpio_num_t pin) {
    if (!led) return;
    led->pin = pin;
    led->state = false;

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << led->pin);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    gpio_set_level(led->pin, 0);
}

void led_set(status_led_t* led, bool on) {
    if (!led) return;
    led->state = on;
    gpio_set_level(led->pin, on ? 1 : 0);
}

void led_toggle(status_led_t* led) {
    if (!led) return;
    led_set(led, !led->state);
}
