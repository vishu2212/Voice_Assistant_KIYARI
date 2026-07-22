#include "storage.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char* TAG = "Storage";

esp_err_t storage_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS flash partition corrupted or version mismatch. Erasing partition...");
        esp_err_t err = nvs_flash_erase();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to erase NVS partition: %s", esp_err_to_name(err));
            return err;
        }
        ret = nvs_flash_init();
    }
    return ret;
}
