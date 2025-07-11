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

#include "task.h"

#include "esp_elf.h"
#include "esp_log.h"
#include "hash_helper.h"
#include "khash.h"

#include <stdatomic.h>

#include <iconv.h>
#include <regex.h>
#include <string.h>

#define bitset(word, bit)   ((word) |= (1 << (bit)))
#define bitclear(word, bit) ((word) &= ~(1 << (bit)))
#define bitflip(word, bit)  ((word) ^= (1 << (bit)))
#define bitcheck(word, bit) ((word) & (1 << (bit)))

static char const *TAG = "task";

// Tracks free PIDs. Note that free PIDs are marked as 1, not 0!
static uint32_t          pid_free_bitmap[NUM_PIDS / 32];
static SemaphoreHandle_t pid_free_bitmap_lock = NULL;
static why_pid_t         next_pid;

// Process table, each process gets a PID, which we track here.
static task_info_t      *process_table[NUM_PIDS];
static SemaphoreHandle_t process_table_lock = NULL;

static TaskHandle_t  cleanup_task_handle;
static QueueHandle_t cleanup_task_queue;

static why_pid_t pid_allocate() {
    if (xSemaphoreTake(pid_free_bitmap_lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to get pid table mutex");
        abort();
    }

    // First see if our next pid is free
    why_pid_t pid      = next_pid;
    uint32_t  word_idx = pid / 32;
    uint32_t  bit_idx  = pid % 32;

    if (bitcheck(pid_free_bitmap[word_idx], bit_idx)) {
        // next_pid is free. claim it.
        bitclear(pid_free_bitmap[word_idx], bit_idx);
        goto out;
    }

    // No, we will need to scan our bitmap
    for (int i = 0; i < (sizeof(pid_free_bitmap) / 4); ++i) {
        // We want to loop through all of our bitmap, but we want to start at
        // where we expect the next free pid to be
        int check_word_idx = (i + word_idx) % (sizeof(pid_free_bitmap) / 4);

        if (!pid_free_bitmap[check_word_idx]) {
            // all zeros no need to check
            continue;
        }

        for (int k = 0; k < 32; ++k) {
            if (bitcheck(pid_free_bitmap[check_word_idx], k)) {
                bitclear(pid_free_bitmap[check_word_idx], k);
                pid = check_word_idx * 32 + k;
                goto out;
            }
        }
    }

    // No free pids
    pid = -1;
    goto error;

out:
    next_pid = pid + 1;

    if (next_pid > MAX_PID) {
        // PIDs looped, start over
        next_pid = 1;
    }

error:

    xSemaphoreGive(pid_free_bitmap_lock);
    return pid;
}

static void pid_free(why_pid_t pid) {
    uint32_t word_idx = pid / 32;
    uint32_t bit_idx  = pid % 32;

    if (xSemaphoreTake(pid_free_bitmap_lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to get pid table mutex");
        abort();
    }

    bitset(pid_free_bitmap[word_idx], bit_idx);

    xSemaphoreGive(pid_free_bitmap_lock);
}

static void process_table_add_task(task_info_t *task_info) {
    if (xSemaphoreTake(process_table_lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to get process table mutex");
        abort();
    }

    process_table[task_info->pid] = task_info;

    xSemaphoreGive(process_table_lock);
}

static void process_table_remove_task(task_info_t *task_info) {
    if (xSemaphoreTake(process_table_lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to get process table mutex");
        abort();
    }

    process_table[task_info->pid] = NULL;

    xSemaphoreGive(process_table_lock);
}

static void task_killed(int idx, void *ti) {
    task_info_t *task_info = ti;

    BaseType_t higher_priority_task_woken = pdFALSE;
    why_pid_t  pid                        = task_info->pid;
    xQueueSendFromISR(cleanup_task_queue, &pid, &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

static task_info_t *task_info_init() {
    task_info_t *task_info = malloc(sizeof(task_info_t));

    ESP_LOGI(TAG, "Creating task_info %p for task %d", task_info, task_info->pid);

    memset(task_info, 0, sizeof(task_info_t));

    task_info->term                    = strdup("dumb");
    task_info->current_files           = 3;
    task_info->file_handles[0].is_open = true;
    task_info->file_handles[0].device  = device_get("TT01");
    task_info->file_handles[1].is_open = true;
    task_info->file_handles[1].device  = device_get("TT01");
    task_info->file_handles[2].is_open = true;
    task_info->file_handles[2].device  = device_get("TT01");

    for (int i = 0; i < RES_RESOURCE_TYPE_MAX; ++i) {
        task_info->resources[i] = kh_init(restable);
    }


    return task_info;
}

static void task_info_delete(task_info_t *task_info) {
    ESP_LOGI(TAG, "Deleting task_info %pi for pid %i", task_info, task_info->pid);

    for (int i = 0; i < MAXFD; ++i) {
        // We sadly can't reuse the why_close code as it must be ran from inside the user task
        if (task_info->file_handles[i].is_open) {
            ESP_LOGI(TAG, "Cleaning up open filehandle %i", i);
            if (task_info->file_handles[i].device->_close) {
                task_info->file_handles[i].device->_close(
                    task_info->file_handles[i].device,
                    task_info->file_handles[i].dev_fd
                );
            }
        }
    }

    for (int i = 0; i < RES_RESOURCE_TYPE_MAX; ++i) {
        for (khiter_t k = kh_begin(task_info->resources[i]); k != kh_end(task_info->resources[i]); ++k) {
            if (kh_exist(task_info->resources[i], k)) {
                void                *ptr  = (void *)kh_key(task_info->resources[i], k);
                task_resource_type_t type = kh_value(task_info->resources[i], k);

                switch (type) {
                    case RES_MALLOC:
                        ESP_LOGI(TAG, "Cleaning up malloc %p", ptr);
                        heap_caps_free(ptr);
                        break;
                    case RES_ICONV_OPEN:
                        ESP_LOGI(TAG, "Cleaning up iconv %p", ptr);
                        iconv_close(ptr);
                        break;
                    case RES_REGCOMP:
                        ESP_LOGI(TAG, "Cleaning up regcomp %p", ptr);
                        regfree(ptr);
                        break;
                    case RES_OPEN:
                        // Already handled above
                        break;
                    default: ESP_LOGE(TAG, "Unknown resource type %i in task_info_delete", type);
                }
            }
        }

        kh_destroy(restable, task_info->resources[i]);
    }

    free(task_info->argv_back);
    free(task_info->term);
    free(task_info);
}

static void elf_task(task_info_t *task_info) {
    int        ret;
    esp_elf_t *elf  = malloc(sizeof(esp_elf_t));
    task_info->data = elf;

    ret = esp_elf_init(elf);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to initialize ELF file errno=%d", ret);
        return;
    }

    ret = esp_elf_relocate(elf, (uint8_t const *)task_info->buffer);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to relocate ELF file errno=%d", ret);
        goto out;
    }

    if (!task_info->buffer_in_rom) {
        free(task_info->buffer);
    }

    ESP_LOGI(TAG, "Start ELF file entrypoint");

    esp_elf_request(elf, 0, task_info->argc, task_info->argv);

    ESP_LOGI(TAG, "Successfully exited from ELF file");
    return;

out:
    esp_elf_deinit(elf);
    free(elf);
}

// This is the function that runs inside the Task
static void generic_task(void *ti) {
    // Final setup to be done inside of the task context before we launch our entrypoint
    task_info_t *task_info = ti;
    TaskHandle_t handle    = xTaskGetCurrentTaskHandle();
    task_info->handle      = handle;
    process_table_add_task(task_info);
    vTaskSetThreadLocalStoragePointerAndDelCallback(handle, 0, task_info, task_killed);

    // YOLO
    task_info->task_entry(task_info);

    vTaskDelete(NULL);
}

static void cleanup_task(void *ignored) {
    why_pid_t dead_pid;

    while (1) {
        // Block until a task dies
        if (xQueueReceive(cleanup_task_queue, &dead_pid, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Cleaning up dead task PID %d", dead_pid);

            task_info_t *task_info = process_table[dead_pid];
            if (task_info) {
                switch (task_info->type) {
                    case TASK_TYPE_ELF:
                    case TASK_TYPE_ELF_ROM:
                        // Free ELF structure
                        esp_elf_deinit(((esp_elf_t *)task_info->data));
                        free(task_info->data);

                        break;
                    default: ESP_LOGE(TAG, "Unknown task type %i", task_info->type);
                }

                process_table_remove_task(task_info);
                task_info_delete(task_info);

                // Don't free our PID until the last moment
                pid_free(dead_pid);
                ESP_LOGI(TAG, "Task %d cleaned up", dead_pid);
            } else {
                ESP_LOGE(TAG, "Task %d has no task info?", dead_pid);
            }
        }
    }
}

void task_record_resource_alloc(task_resource_type_t type, void *ptr) {
    task_info_t *task_info = get_task_info();

    khash_insert_unique_ptr(
        restable,
        task_info->resources[type],
        ptr,
        type,
        "Attempted allocate already allocated resource"
    );
}

void task_record_resource_free(task_resource_type_t type, void *ptr) {
    task_info_t *task_info = get_task_info();

    khash_del_ptr(restable, task_info->resources[type], ptr, "Attempted to free already freed resource");
}

why_pid_t run_task(void *buffer, int stack_size, task_type_t type, int argc, char *argv[]) {
    why_pid_t pid = pid_allocate();
    if (pid <= 0) {
        ESP_LOGW(TAG, "Cannot allocate PID for new task");
        return -1;
    }

    task_info_t *task_info = task_info_init();

    task_info->type          = type;
    task_info->pid           = pid;
    task_info->buffer        = buffer;
    task_info->buffer_in_rom = false;
    task_info->argc          = argc;

    // Pack up argv in a nice compact list
    size_t argv_size = argc * sizeof(char *);
    for (int i = 0; i < argc; ++i) {
        argv_size += strlen(argv[i]) + 1;
    }

    task_info->argv      = malloc(argv_size);
    // In case someone tries something clever
    task_info->argv_back = task_info->argv;
    task_info->argv_size = argv_size;

    size_t offset = argc * sizeof(char *);
    for (int i = 0; i < argc; ++i) {
        char *arg_address  = ((void *)task_info->argv) + offset;
        task_info->argv[i] = arg_address;
        strcpy(arg_address, argv[i]);
        offset += strlen(argv[i]) + 1;
    }

    switch (type) {
        case TASK_TYPE_ELF: task_info->task_entry = elf_task; break;
        case TASK_TYPE_ELF_ROM:
            task_info->task_entry    = elf_task;
            task_info->buffer_in_rom = true;
            break;
        default:
            ESP_LOGE(TAG, "Unknown task type %i", type);
            free(task_info);
            return -1;
    }

#if (NUM_PIDS > 999)
#error "If you hit this assertion change the allocation for the task name to match the new size"
#endif

    // "Task " + "255" + \0
    char task_name[5 + 3 + 1];
    sprintf(task_name, "Task %i", (uint8_t)pid);

    xTaskCreate(generic_task, task_name, stack_size, (void *)task_info, 5, NULL);
    return pid;
}

task_info_t *get_task_info() {
    task_info_t *task_info = pvTaskGetThreadLocalStoragePointer(NULL, 0);

    ESP_LOGD("get_task_info", "Got process_handle %p == %p", task_info->handle, task_info);

    return task_info;
}

void task_init() {
    ESP_LOGI(TAG, "Initializing");

    pid_free_bitmap_lock = xSemaphoreCreateMutex();
    memset(pid_free_bitmap, 0xFF, sizeof(pid_free_bitmap));
    pid_free_bitmap[0] = UINT32_MAX - 1; // PID 0 is the kernel

    process_table_lock = xSemaphoreCreateMutex();
    memset(process_table, 0, sizeof(process_table));

    cleanup_task_queue = xQueueCreate(16, sizeof(why_pid_t));
    xTaskCreate(cleanup_task, "Task Cleanup Task", 2048, NULL, 10, &cleanup_task_handle);

    next_pid = 1;
}
