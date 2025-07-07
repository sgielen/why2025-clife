#include "device.h"

#include "freertos/FreeRTOS.h"

#include "hash_helper.h"
#include "esp_log.h"

static const char *TAG = "device";

khash_t(devtable) *device_table;
SemaphoreHandle_t device_table_lock = NULL;

int device_register(const char *name, device_t *device) {
    if (xSemaphoreTake(device_table_lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to get device table mutex");
        abort();
    }

    khash_insert_unique_str(devtable, device_table, name, device, "The device already exists");

    xSemaphoreGive(device_table_lock);

    return 0;
}

device_t* device_get(const char *name) {
    if (xSemaphoreTake(device_table_lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to get device table mutex");
        abort();
    }

    khash_get_str(device_t*, device, devtable, device_table, name, "The device does not exist");

    xSemaphoreGive(device_table_lock);

    return device;
}

void device_init() {
    ESP_LOGI(TAG, "Initializing");

    device_table = kh_init(devtable);
    device_table_lock = xSemaphoreCreateMutex();
}

