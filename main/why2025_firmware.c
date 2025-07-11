/* This file is part of BadgeVMS
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "device.h"
#include "drivers/fatfs.h"
#include "drivers/tty.h"
#include "elf_symbols.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "logical_names.h"
#include "task.h"

#include <string.h>
static void const *__keep_symbol __attribute__((used)) = &elf_find_sym;

static char const *TAG = "why2025_main";

extern uint8_t const test_elf_a_start[] asm("_binary_test_basic_a_elf_start");
extern uint8_t const test_elf_a_end[] asm("_binary_test_basic_a_elf_end");
extern uint8_t const test_elf_b_start[] asm("_binary_test_basic_b_elf_start");
extern uint8_t const test_elf_b_end[] asm("_binary_test_basic_b_elf_end");
extern uint8_t const test_elf_c_start[] asm("_binary_test_basic_c_elf_start");
extern uint8_t const test_elf_c_end[] asm("_binary_test_basic_c_elf_end");
extern uint8_t const test_elf_shell_start[] asm("_binary_test_shell_elf_start");
extern uint8_t const test_elf_shell_end[] asm("_binary_test_shell_elf_end");

int app_main(void) {
    logical_names_system_init();
    device_init();
    task_init();

    device_register("TT01", tty_create(true, true));
    device_register("FLASH0", fatfs_create_spi("FLASH0", "storage", true));

    logical_name_set("SEARCH", "FLASH0:[SUBDIR], FLASH0:[SUBDIR.ANOTHER]", false);

    printf("Hello ESP32P4 firmware\n");

    //    xTaskCreate(run_elf, "Task1", 16384, test_elf_a_start, 5, &elf_a);

    char **argv = malloc(sizeof(char *) * 2);
    argv[0]     = strdup("test_elf_c");
    argv[1]     = strdup("argv[xxx]");

    // for (int i = 1; i < 270; ++i) {
    sprintf(argv[1], "argv[%d]", 0);
    why_pid_t pida = run_task(test_elf_a_start, 4096, TASK_TYPE_ELF_ROM, 2, argv);
    ESP_LOGI(TAG, "Started task with pid %i", pida);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    //}
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
