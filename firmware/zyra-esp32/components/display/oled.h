#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdbool.h>

typedef struct {
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
    uint8_t buffer[1024]; // 128x64 display buffer
    bool initialized;
} oled_display_t;

// Initializes the I2C bus and SH1106 display controller
esp_err_t oled_init(oled_display_t* dev, int sda_io, int scl_io, uint8_t address);

// Clears local buffer
void oled_clear(oled_display_t* dev);

// Flushes local buffer to physical display RAM
esp_err_t oled_refresh(oled_display_t* dev);

// Draws a single pixel in the frame buffer
void oled_draw_pixel(oled_display_t* dev, int x, int y, bool color);

// Prints a single character
void oled_print_char(oled_display_t* dev, int x, int page, char c);

// Prints a null-terminated string
void oled_print_string(oled_display_t* dev, int x, int page, const char* str);
