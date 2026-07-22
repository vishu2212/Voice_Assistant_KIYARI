#include "button.h"

void button_init(button_t* btn, gpio_num_t pin) {
    if (!btn) return;
    btn->pin = pin;

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << btn->pin);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
}

bool button_is_pressed(button_t* btn) {
    if (!btn) return false;
    return gpio_get_level(btn->pin) == 0;
}
