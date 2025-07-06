#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "rom/uart.h"

#include "task.h"

static const char *TAG = "why2025_main";

extern const uint8_t test_elf_a_start[] asm("_binary_test_basic_a_elf_start");
extern const uint8_t test_elf_a_end[] asm("_binary_test_basic_a_elf_end");
extern const uint8_t test_elf_b_start[] asm("_binary_test_basic_b_elf_start");
extern const uint8_t test_elf_b_end[] asm("_binary_test_basic_b_elf_end");
extern const uint8_t test_elf_shell_start[] asm("_binary_test_shell_elf_start");
extern const uint8_t test_elf_shell_end[] asm("_binary_test_shell_elf_end");

int app_main(void) {
    task_init();

    TaskHandle_t elf_a, elf_b;
    printf("Hello ESP32P4 firmware\n");

    xTaskCreate(run_elf, "Task1", 16384, test_elf_a_start, 5, &elf_a); 
//    xTaskCreate(run_elf, "Task1", 16384, test_elf_shell_start, 5, &elf_a); 
    xTaskCreate(run_elf, "Task2", 4096, test_elf_b_start, 5, &elf_b); 


    while(1) {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    };

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    printf("Suspending worker task A\n");
    vTaskSuspend(elf_a);

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    printf("Restarting worker task A\n");
    vTaskResume(elf_a);

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    printf("Killing worker task A\n");
    vTaskDelete(elf_a);

 //   vTaskDelay(5000 / portTICK_PERIOD_MS);
 //   printf("Killing worker task B\n");
 //   vTaskDelete(elf_b);

    printf("Sleeping for a bit\n");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    printf("Bye cruel workd!\n");

    return 0;
}
