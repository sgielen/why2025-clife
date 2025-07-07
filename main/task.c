#include <string.h>
#include <stdatomic.h>
#include <iconv.h>
#include <regex.h>

#include "task.h"
#include "khash.h"

#include "esp_log.h"
#include "esp_elf.h"

#include "hash_helper.h"

// Hack to prevent elf_find_sym being deleted from our project
extern uintptr_t elf_find_sym(const char *sym_name);

// We are not allowed to block at all in the TLS cleanup callback
// so we have a separate thread taking care of that.
// we just check the status of the bool periodically to see if we should
// clean anything up.
static atomic_flag everything_clean = ATOMIC_FLAG_INIT;

static const char *TAG = "task";

khash_t(ptable) *process_table;
SemaphoreHandle_t process_table_lock = NULL;

void process_table_add_task(task_info_t* task_info) {
    if (xSemaphoreTake(process_table_lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to get process table mutex");
        abort();
    }

    khash_insert_ptr(ptable, process_table, task_info->handle, task_info, "task_info");

    xSemaphoreGive(process_table_lock);
}

// Must be called under lock!
void process_table_remove_task(task_info_t* task_info) {
    khash_del_ptr(ptable, process_table, task_info->handle, "Attempted to remove non-existant program from process table");
}

static void task_killed(int idx, void* ti) {
    // We cannot do any logging or locking here.
    task_info_t *task_info = ti;
    task_info->killed = true;
    atomic_flag_clear(&everything_clean);
}

void task_info_init() {
    TaskHandle_t handle = xTaskGetCurrentTaskHandle();
    task_info_t *task_info = malloc(sizeof(task_info_t));

    ESP_LOGI(TAG, "Creating task_info %p for task %p", task_info, handle);

    memset(task_info, 0, sizeof(task_info_t));

    task_info->term = strdup("dumb");
    task_info->current_files = 3;
    task_info->handle = handle;
    task_info->file_handles[0].is_open = true;
    task_info->file_handles[0].device = device_get("TT01:");
    task_info->file_handles[1].is_open = true;
    task_info->file_handles[1].device = device_get("TT01:");
    task_info->file_handles[2].is_open = true;
    task_info->file_handles[2].device = device_get("TT01:");

    for (int i = 0; i < RES_RESOURCE_TYPE_MAX; ++i) {
        task_info->resources[i] = kh_init(restable);
    }

    process_table_add_task(task_info);
    vTaskSetThreadLocalStoragePointerAndDelCallback(handle, 0, task_info, task_killed);
}

void task_info_delete(task_info_t* task_info) {
    ESP_LOGI(TAG, "Deleting task_info %p", task_info);

    for (int i = 0; i < RES_RESOURCE_TYPE_MAX; ++i) {
        for (khiter_t k = kh_begin(task_info->resources[i]); k != kh_end(task_info->resources[i]); ++k) {
            if (kh_exist(task_info->resources[i], k)) {
                void *ptr = (void*)kh_key(task_info->resources[i], k);
                enum task_resource_type type = kh_value(task_info->resources[i], k);

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
                        // Nothing to do here, handled later
                        break;
                    default:
                        ESP_LOGE(TAG, "Unknown resource type %i in task_info_delete", type);
                }
            }
        }

        kh_destroy(restable, task_info->resources[i]);
    }
    free(task_info->term);
    free(task_info);
}

void task_record_resource_alloc(enum task_resource_type type, void *ptr) {
    task_info_t *task_info = get_task_info();

    khash_insert_unique_ptr(restable, task_info->resources[type], ptr, type, "Attempted allocate already allocated resource");
}

void task_record_resource_free(enum task_resource_type type, void *ptr) {
    task_info_t *task_info = get_task_info();

    khash_del_ptr(restable, task_info->resources[type], ptr, "Attempted to free already freed resource");
}

void run_elf(void *buffer) {
    int argc = 0;
    char **argv = NULL;
    int ret;

    esp_elf_t elf;

    task_info_init();

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

    ESP_LOGI(TAG, "Start ELF file entrypoint");

    esp_elf_request(&elf, 0, argc, argv);

    ESP_LOGI(TAG, "Successfully exited from ELF file");

    esp_elf_deinit(&elf);
    vTaskDelete(NULL);
}

task_info_t *get_task_info() {
    task_info_t *task_info = pvTaskGetThreadLocalStoragePointer(NULL, 0);

    ESP_LOGD("get_task_info", "Got process_handle %p == %p\n", task_info->handle, task_info);

    return task_info;
}

void cleanup_thread(void *ignored) {
    while (1) {
        if (! atomic_flag_test_and_set(&everything_clean)) {
            if (xSemaphoreTake(process_table_lock, portMAX_DELAY) != pdTRUE) {
                ESP_LOGE(TAG, "Failed to get process table mutex");
                abort();
            }

            do {
                for (khiter_t k = kh_begin(process_table); k != kh_end(process_table); ++k) {
                    if (kh_exist(process_table, k)) {
                        task_info_t *task_info = kh_value(process_table, k);
                        if (task_info->killed) {
                            void *ptr = task_info->handle;
                            ESP_LOGI(TAG, "Deleting killed task %p", ptr);
                            // We are under lock
                            process_table_remove_task(task_info);
                            kh_del(ptable, process_table, k);
                            task_info_delete(task_info);
                            ESP_LOGI(TAG, "Killed task %p deleted", ptr);
                        }
                    }
                }
            } while (! atomic_flag_test_and_set(&everything_clean));

            xSemaphoreGive(process_table_lock);
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void task_init() {
    ESP_LOGI(TAG, "Initializing");

    // TODO hack
    uintptr_t x = elf_find_sym("strdup");
    (void)x;

    process_table = kh_init(ptable);
    process_table_lock = xSemaphoreCreateMutex();
    xTaskCreate(cleanup_thread, "Task Cleanup Thread", 2048, NULL, 10, NULL);
}
