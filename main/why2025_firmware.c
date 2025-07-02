#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_elf.h"

static const char *TAG = "elf_loader";

extern const uint8_t test_elf_a_start[] asm("_binary_test_basic_a_elf_start");
extern const uint8_t test_elf_a_end[] asm("_binary_test_basic_a_elf_end");
extern const uint8_t test_elf_b_start[] asm("_binary_test_basic_b_elf_start");
extern const uint8_t test_elf_b_end[] asm("_binary_test_basic_b_elf_end");

void *why_malloc(size_t size) {
    printf("WHY\n");
    TaskHandle_t h = xTaskGetCurrentTaskHandle();
    ESP_LOGI("why_malloc", "Calling malloc from task %p", h);
    return malloc(size);
}

void run_elf(void *buffer) {
    int argc = 0;
    char **argv = NULL;
    int ret;

    esp_elf_t elf;

    ret = esp_elf_init(&elf);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to initialize ELF file errno=%d", ret);
        return;
    }

    ret = esp_elf_relocate(&elf, (const uint8_t*)buffer);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to relocate ELF file errno=%d", ret);
        return;
    }

    ESP_LOGI(TAG, "Start to run ELF file");

    esp_elf_request(&elf, 0, argc, argv);

    ESP_LOGI(TAG, "Success to exit from ELF file");

    esp_elf_deinit(&elf);
}

int app_main(void) {
    TaskHandle_t elf_a, elf_b;
    printf("Hello ESP32P4 firmware\n");

    xTaskCreate(run_elf, "Task1", 2048, test_elf_a_start, 5, &elf_a); 
    xTaskCreate(run_elf, "Task2", 2048, test_elf_b_start, 5, &elf_b); 

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    printf("Killing worker task A\n");
    vTaskDelete(elf_a);

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    printf("Killing worker task B\n");
    vTaskDelete(elf_b);

    printf("Sleeping for a bit\n");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    printf("Bye cruel workd!\n");

    return 0;
}
