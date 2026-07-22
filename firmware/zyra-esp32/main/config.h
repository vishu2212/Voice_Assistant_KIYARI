#pragma once

#include "driver/gpio.h"

#define ZYRA_NAME "JARVIS"
#define ZYRA_VERSION "1.0.0"

#define SAMPLE_RATE 16000
#define CHANNELS 1
#define BITS_PER_SAMPLE 16

// INMP441 Microphone (I2S Port 0)
#define MIC_WS_GPIO GPIO_NUM_4
#define MIC_SCK_GPIO GPIO_NUM_5
#define MIC_SD_GPIO GPIO_NUM_6

// MAX98357A Speaker (I2S Port 1)
#define SPK_DOUT_GPIO GPIO_NUM_7
#define SPK_BCLK_GPIO GPIO_NUM_15
#define SPK_LRC_GPIO GPIO_NUM_16

// OLED Display (I2C)
#define OLED_SDA_GPIO GPIO_NUM_8
#define OLED_SCL_GPIO GPIO_NUM_9
#define OLED_I2C_ADDR 0x3C

// Push Button Input
#define BOOT_BUTTON_PIN GPIO_NUM_0

// Status Indicator LED
#define STATUS_LED_PIN GPIO_NUM_48

// Wi-Fi Connections
// Wi-Fi Connections
#define WIFI_SSID "Lucifer " // Update to your 2.4GHz Wi-Fi name (case-sensitive)
#define WIFI_PASS "12345678"
#define BACKEND_BASE_URL "http://10.170.183.164:8001"     // Updated to new IP
#define BACKEND_WS_URL "ws://10.170.183.164:8001/ws/chat" // Updated to new IP
// Audio Buffer
#define RECORD_DURATION_SEC 5
#define RECORD_BUFFER_SIZE (SAMPLE_RATE * 2 * RECORD_DURATION_SEC) // 160,000 Bytes
