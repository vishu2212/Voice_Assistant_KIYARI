#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

// Initializes the netif stack and default station configs
esp_err_t wifi_manager_init(void);

// Connects to target Wi-Fi Access Point (blocks until connected or timeout)
esp_err_t wifi_manager_connect(const char* ssid, const char* password, uint32_t timeout_ms);

// Disconnects from current Access Point
esp_err_t wifi_manager_disconnect(void);

// Checks if the Wi-Fi is actively connected with allocated IP
bool wifi_manager_is_connected(void);

// Retrieves the allocated IPv4 address as a string
esp_err_t wifi_manager_get_ip(char* ip_str, size_t max_len);
