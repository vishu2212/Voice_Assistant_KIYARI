#pragma once

#include "esp_err.h"

// Initializes default NVS flash partition. Handles erasing/retrying on corruption.
esp_err_t storage_init(void);
