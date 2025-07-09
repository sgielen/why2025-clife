#include "task.h"

#include "esp_elf.h"
#include "esp_log.h"
#include "hash_helper.h"
#include "khash.h"

#include <stdatomic.h>

#include <iconv.h>
#include <regex.h>
#include <string.h>

// Hack to prevent elf_find_sym being deleted from our project
extern uintptr_t   elf_find_sym(char const *sym_name);
static char const *TAG = "task";

// Tracks free PIDs. Note that free PIDs are marked as 1, not 0!
static uint32_t   pid_free_bitmap[NUM_PIDS / 32];
SemaphoreHandle_t pid_free_bitmap_lock = NULL;

// Process table, each process gets a PID, which we track here.
static task_info_t *process_table[NUM_PIDS];
SemaphoreHandle_t   process_table_lock = NULL;

TaskHandle_t cleanup_task_handle;

static why_pid_t pid_allocate() {
    if (xSemaphoreTake(pid_free_bitmap_lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to get pid table mutex");
        abort();
    }

    why_pid_t pid = 0;

    for (int i = 0; i < sizeof(pid_free_bitmap) / 4; ++i) {
        int idx = __builtin_ffs(pid_free_bitmap[i]);
        if (!idx)
            continue; // this bitmap is full

        pid                 = i * 32 + (idx - 1); // ffs is 1-based
        pid_free_bitmap[i] &= ~(1U << (idx - 1));
        break;
    }

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

    pid_free_bitmap[word_idx] &= ~(1U << bit_idx);

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
    // We cannot do any logging or locking here.
    task_info_t *task_info = ti;
    atomic_store(&task_info->killed, true);
    xTaskNotifyGiveIndexed(cleanup_task_handle, 0);
}

static task_info_t *task_info_init() {
    TaskHandle_t handle    = xTaskGetCurrentTaskHandle();
    task_info_t *task_info = malloc(sizeof(task_info_t));

    ESP_LOGI(TAG, "Creating task_info %p for task %p", task_info, handle);

    memset(task_info, 0, sizeof(task_info_t));

    task_info->term                    = strdup("dumb");
    task_info->killed                  = ATOMIC_VAR_INIT(false);
    task_info->current_files           = 3;
    task_info->handle                  = handle;
    task_info->file_handles[0].is_open = true;
    task_info->file_handles[0].device  = device_get("TT01");
    task_info->file_handles[1].is_open = true;
    task_info->file_handles[1].device  = device_get("TT01");
    task_info->file_handles[2].is_open = true;
    task_info->file_handles[2].device  = device_get("TT01");

    for (int i = 0; i < RES_RESOURCE_TYPE_MAX; ++i) {
        task_info->resources[i] = kh_init(restable);
    }

    vTaskSetThreadLocalStoragePointerAndDelCallback(handle, 0, task_info, task_killed);

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

    free(task_info->term);
    free(task_info);
}

static void elf_task(task_parameters_t *p) {
    esp_elf_t elf;
    int       ret;

    ret = esp_elf_init(&elf);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to initialize ELF file errno=%d", ret);
        return;
    }

    ret = esp_elf_relocate(&elf, (uint8_t const *)p->buffer);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to relocate ELF file errno=%d", ret);
        goto out;
    }

    if (!p->buffer_in_rom) {
        free(p->buffer);
    }

    ESP_LOGI(TAG, "Start ELF file entrypoint");

    esp_elf_request(&elf, 0, p->argc, p->argv);

    ESP_LOGI(TAG, "Successfully exited from ELF file");

out:
    esp_elf_deinit(&elf);
}

// This is the function that runs inside the Task
static void generic_task(void *tp) {
    task_parameters_t *p         = tp;
    task_info_t       *task_info = task_info_init();
    task_info->pid               = p->pid;
    process_table_add_task(task_info);
    p->task_entry(p);
    vTaskDelete(NULL);
}

static void cleanup_task(void *ignored) {
    while (1) {
        // For each notification run once. There is most likely just one
        // task to clean up. If there are more we'll just start over.
        ulTaskNotifyTakeIndexed(0, pdFALSE, portMAX_DELAY);
        ESP_LOGI(TAG, "Cleanup task woke up");
        // Skip pid 0
        for (int i = 1; i < NUM_PIDS; ++i) {
            task_info_t *task_info = process_table[i];
            if (task_info && atomic_load(&task_info->killed)) {
                why_pid_t pid = task_info->pid;
                ESP_LOGI(TAG, "Deleting killed task %i", pid);
                // We are under lock.
                process_table_remove_task(task_info);
                task_info_delete(task_info);
                pid_free(pid);
                ESP_LOGI(TAG, "Killed task %i deleted", pid);
                // There's probably only one task to clean up, stop iterating
                // and drop the lock.
                break;
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
    if (!pid) {
        ESP_LOGW(TAG, "Cannot allocate PID for new task");
        return -1;
    }

    task_parameters_t *parameters = malloc(sizeof(task_parameters_t));
    parameters->pid               = pid;
    parameters->buffer            = buffer;
    parameters->buffer_in_rom     = false;
    parameters->argc              = argc;
    parameters->argv              = argv;

    switch (type) {
        case TASK_TYPE_ELF: parameters->task_entry = elf_task; break;
        case TASK_TYPE_ELF_ROM:
            parameters->task_entry    = elf_task;
            parameters->buffer_in_rom = true;
            break;
        default:
            ESP_LOGE(TAG, "Unknown task type %i", type);
            free(parameters);
            return -1;
    }

    // "Task " + "255" + \0
    char task_name[5 + 3 + 1];
    sprintf(task_name, "Task %i", (uint8_t)pid);

    xTaskCreate(generic_task, task_name, stack_size, (void *)parameters, 5, NULL);
    return pid;
}

task_info_t *get_task_info() {
    task_info_t *task_info = pvTaskGetThreadLocalStoragePointer(NULL, 0);

    ESP_LOGD("get_task_info", "Got process_handle %p == %p", task_info->handle, task_info);

    return task_info;
}

void task_init() {
    ESP_LOGI(TAG, "Initializing");

    // If you hit this assertion change the allocation for the task name to match the new size
    assert(NUM_PIDS == 256);
    // TODO hack
    uintptr_t x = elf_find_sym("strdup");
    (void)x;

    pid_free_bitmap_lock = xSemaphoreCreateMutex();
    memset(pid_free_bitmap, 0xFF, sizeof(pid_free_bitmap));
    pid_free_bitmap[0] = UINT32_MAX - 1; // PID 0 is the kernel

    process_table_lock = xSemaphoreCreateMutex();
    memset(process_table, 0, sizeof(process_table));

    xTaskCreate(cleanup_task, "Task Cleanup Task", 2048, NULL, 10, &cleanup_task_handle);
}
