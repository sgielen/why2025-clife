#include "device.h"
#include "drivers/fatfs.h"
#include "drivers/tty.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "task.h"

#include <string.h>

static char const *TAG = "why2025_main";

extern uint8_t const test_elf_a_start[] asm("_binary_test_basic_a_elf_start");
extern uint8_t const test_elf_a_end[] asm("_binary_test_basic_a_elf_end");
extern uint8_t const test_elf_b_start[] asm("_binary_test_basic_b_elf_start");
extern uint8_t const test_elf_b_end[] asm("_binary_test_basic_b_elf_end");
extern uint8_t const test_elf_shell_start[] asm("_binary_test_shell_elf_start");
extern uint8_t const test_elf_shell_end[] asm("_binary_test_shell_elf_end");

int app_main(void) {
    device_init();
    task_init();

    device_register("TT01", tty_create(true, true));
    device_register("FLASH0", fatfs_create_spi("FLASH0", "storage", true));

    printf("Hello ESP32P4 firmware\n");

    //    xTaskCreate(run_elf, "Task1", 16384, test_elf_a_start, 5, &elf_a);
    why_pid_t pida = run_task(test_elf_a_start, 4096, TASK_TYPE_ELF_ROM, 0, NULL);
    ESP_LOGI(TAG, "Started task with pid %i", pida);
    // why_pid_t pidb = run_task(test_elf_b_start, 4096, TASK_TYPE_ELF_ROM, 0, NULL);
    // ESP_LOGI(TAG, "Started task with pid %i", pidb);
    //    xTaskCreate(run_elf, "Task1", 16384, test_elf_shell_start, 5, &elf_a);
    //    xTaskCreate(run_elf, "Task2", 4096, test_elf_b_start, 5, &elf_b);


    vTaskDelay(5000 / portTICK_PERIOD_MS);
    while (1) {
        fprintf(stderr, ".");
        fflush(stderr);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    };

#if 0 
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
#endif

    return 0;
}
