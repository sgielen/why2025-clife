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
#include "drivers/st7703.h"
#include "elf_symbols.h"
#include "esp_debug_helpers.h"
#include "esp_log.h"
#include "esp_mmu_map.h"
#include "esp_private/panic_internal.h"
#include "esp_psram.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/cache_hal.h"
#include "hal/cache_ll.h"
#include "hal/cache_types.h"
#include "hal/mmu_hal.h"
#include "hal/mmu_ll.h"
#include "hal/mmu_types.h"
#include "logical_names.h"
#include "memory.h"
#include "task.h"

#include "png.h"

#include <string.h>
#include <unistd.h>
#include <errno.h>

extern void        spi_flash_enable_interrupts_caches_and_other_cpu(void);
extern void        spi_flash_disable_interrupts_caches_and_other_cpu(void);
extern void        __real_esp_panic_handler(panic_info_t *info);
static void const *__keep_symbol __attribute__((used)) = &elf_find_sym;
static char const *TAG                                 = "why2025_main";

#if 0
extern uint8_t const test_elf_a_start[] asm("_binary_test_basic_a_elf_start");
extern uint8_t const test_elf_a_end[] asm("_binary_test_basic_a_elf_end");
extern uint8_t const test_elf_b_start[] asm("_binary_test_basic_b_elf_start");
extern uint8_t const test_elf_b_end[] asm("_binary_test_basic_b_elf_end");
extern uint8_t const test_elf_c_start[] asm("_binary_test_basic_c_elf_start");
extern uint8_t const test_elf_c_end[] asm("_binary_test_basic_c_elf_end");
extern uint8_t const test_elf_shell_start[] asm("_binary_test_shell_elf_start");
extern uint8_t const test_elf_shell_end[] asm("_binary_test_shell_elf_end");
#endif
extern uint8_t const test_elf_bench_a_start[] asm("_binary_bench_basic_a_elf_start");
extern uint8_t const test_elf_bench_a_end[] asm("_binary_bench_basic_a_elf_end");
extern uint8_t const test_elf_bench_b_start[] asm("_binary_bench_basic_b_elf_start");
extern uint8_t const test_elf_bench_b_end[] asm("_binary_bench_basic_b_elf_end");

extern FILE * why_fopen(const char *pathname, const char *mode);
extern int why_fseek(FILE *stream, long offset, int whence);
extern void why_rewind(FILE *stream);
extern size_t why_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
extern int why_fclose(FILE *f);
extern long why_ftell(FILE *stream);

void IRAM_ATTR __wrap_esp_panic_handler(panic_info_t *info) {
    task_info_t *task_info = get_task_info();
    if (task_info) {
        esp_rom_printf("Crashing in task: %u\n", task_info->pid);
    } else {
        esp_rom_printf("Crashing in BadgeVMS\n");
    }

    __real_esp_panic_handler(info);
}

int app_main(void) {
    printf("BadgeVMS Initializing...\n");

    memory_init();
    task_init();
    device_init();
    logical_names_system_init();

    device_register("PANEL0", st7703_create());
    device_register("TT01", tty_create(true, true));
    device_register("FLASH0", fatfs_create_spi("FLASH0", "storage", true));
    logical_name_set("SEARCH", "FLASH0:[SUBDIR], FLASH0:[SUBDIR.ANOTHER]", false);

    printf("BadgeVMS is ready\n");

    //    xTaskCreate(run_elf, "Task1", 16384, test_elf_a_start, 5, &elf_a);
    //
    png_image image;
    memset(&image, 0, (sizeof image));
    image.version = PNG_IMAGE_VERSION;

    int buf_size = 720 * 720 * 3;
    // png_bytep pixels = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    lcd_device_t *lcd_device = (lcd_device_t*)device_get("PANEL0");
    void *pixels;
    lcd_device->_getfb(lcd_device, &pixels);
    memset(pixels, 0, buf_size);
    for (int i = 0; i < buf_size; i += 3) {
        ((char*)pixels)[i] = 0xff;
    }
    lcd_device->_draw(lcd_device, 0, 0, 720, 720, pixels);

#if 0
    FILE *f = why_fopen("FLASH0:BADGEVMS.PNG", "r");
    if (f) {
        why_fseek(f, 0, SEEK_END);
        long size = why_ftell(f);
        if (size > 0) {
            why_fseek(f, 0, SEEK_SET);
            void *imgbuf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM); 
            if (imgbuf) {
                ESP_LOGW(TAG, "Got buffer at %p-%pof size %lu", imgbuf, imgbuf + size, size);
                size_t s = why_fread(imgbuf, size, 1, f);
                if (s) {
                    image.format = PNG_FORMAT_RGB;
                    png_image_begin_read_from_memory(&image, imgbuf, size);
                    int stride = PNG_IMAGE_ROW_STRIDE(image);
                    int buf_size = PNG_IMAGE_SIZE(image);
                    png_bytep pixels = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
                    ESP_LOGW(TAG, "Decompressed image is %u bytes", PNG_IMAGE_SIZE(image));
                    png_image_finish_read(&image, NULL, pixels, stride, NULL);
                    memset(pixels, 0, buf_size);
                    for (int i = 0; i < buf_size; i += 3) {
                        pixels[i] = 0xff;
                    }
                    free(imgbuf);
                    why_fclose(f);
                    lcd_device_t *lcd_device = (lcd_device_t*)device_get("PANEL0");
                    lcd_device->_draw(lcd_device, 0, 0, 720, 720, pixels);
                } else {
                    ESP_LOGW(TAG, "Read failed: %s", strerror(errno));
                } 
            } else {
                ESP_LOGW(TAG, "Malloc(%lu) failed: %s", size, strerror(errno));
            }
        } else {
            ESP_LOGW(TAG, "fseek/ftell failed");
        }
    } else {
        ESP_LOGW(TAG, "Couldn't open image");
    }
#endif

    char **argv = malloc(sizeof(char *) * 2);
    argv[0]     = strdup("test_elf_c");
    argv[1]     = strdup("argv[xxx]");

    while (1) {
        for (int i = 1; i < 3; ++i) {
            sprintf(argv[1], "argv[%d]", 0);
            // pid_t pida = run_task(test_elf_bench_a_start, 4096, TASK_TYPE_ELF_ROM, 2, argv);
            // ESP_LOGI(TAG, "Started task with pid %i", pida);
            pid_t pidb = run_task(test_elf_bench_b_start, 4096, TASK_TYPE_ELF_ROM, 2, argv);
            ESP_LOGI(TAG, "Started task with pid %i", pidb);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
    // pid_t pidb = run_task(test_elf_b_start, 4096, TASK_TYPE_ELF_ROM, 0, NULL);
    // ESP_LOGI(TAG, "Started task with pid %i", pidb);
    //    xTaskCreate(run_elf, "Task1", 16384, test_elf_shell_start, 5, &elf_a);
    //    xTaskCreate(run_elf, "Task2", 4096, test_elf_b_start, 5, &elf_b);

    vTaskDelay(30000 / portTICK_PERIOD_MS);
    esp_restart();
    // while (1) {
    //     fprintf(stderr, ".");
    //     fflush(stderr);
    //     vTaskDelay(5000 / portTICK_PERIOD_MS);
    // };

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
