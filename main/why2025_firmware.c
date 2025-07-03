#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_elf.h"
#include "khash.h"

static const char *TAG = "elf_loader";

extern const uint8_t test_elf_a_start[] asm("_binary_test_basic_a_elf_start");
extern const uint8_t test_elf_a_end[] asm("_binary_test_basic_a_elf_end");
extern const uint8_t test_elf_b_start[] asm("_binary_test_basic_b_elf_start");
extern const uint8_t test_elf_b_end[] asm("_binary_test_basic_b_elf_end");

KHASH_MAP_INIT_INT64(ptable, void*);
khash_t(ptable) *process_table;

typedef struct {
   bool is_open : 1;
   const char* file;
} file_handle_t;

typedef struct {
    size_t max_memory;
    size_t current_memory;
    size_t max_files;
    size_t current_files;
    file_handle_t file_handles[128];
} task_info_t;

void *why_malloc(size_t size) {
    TaskHandle_t h = xTaskGetCurrentTaskHandle();
    task_info_t *task_info = NULL;

    khint_t k = kh_get(ptable, process_table, (uintptr_t)h);
    if (k != kh_end(process_table)) {
       task_info = kh_value(process_table, k);
    }

    printf("Got process_handle %p == %p\n", h, task_info);
    ESP_LOGI("why_malloc", "Calling malloc from task %p", h);
    ESP_LOGI("why_malloc", "0 is_open: %i", task_info->file_handles[0].is_open);
    ESP_LOGI("why_malloc", "4 is_open: %i", task_info->file_handles[4].is_open);
    return malloc(size);
}

ssize_t why_write(int fd, const void *buf, size_t count) {
    TaskHandle_t h = xTaskGetCurrentTaskHandle();
    ESP_LOGI("why_write", "Calling write from task %p", h);
    return write(fd, buf, count);
}

void run_elf(void *buffer) {
    int argc = 0;
    char **argv = NULL;
    int ret;

    task_info_t task_info;
    memset(&task_info, 0, sizeof(task_info));
    task_info.current_files = 3;
    task_info.file_handles[0].is_open = true;
    task_info.file_handles[1].is_open = true;
    task_info.file_handles[2].is_open = true;

    TaskHandle_t h = xTaskGetCurrentTaskHandle();
    printf("Setting process_handle to %p == %p\n", h, &task_info);
    int r;
    khint_t k = kh_put(ptable, process_table, (uintptr_t)h, &r);
    if (r >= 0) {
        kh_value(process_table, k) = &task_info;
    }

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
    process_table = kh_init(ptable);
    TaskHandle_t elf_a, elf_b;
    printf("Hello ESP32P4 firmware\n");

    xTaskCreate(run_elf, "Task1", 4096, test_elf_a_start, 5, &elf_a); 
    xTaskCreate(run_elf, "Task2", 4096, test_elf_b_start, 5, &elf_b); 

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    printf("Suspending worker task A\n");
    vTaskSuspend(elf_a);

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    printf("Restarting worker task A\n");
    vTaskResume(elf_a);

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
