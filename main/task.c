#include <string.h>
#include <stdatomic.h>

#include "task.h"

#include "esp_log.h"
#include "esp_elf.h"

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

    int r;
    khint_t k = kh_put(ptable, process_table, (uintptr_t)task_info->handle, &r);
    if (r >= 0) {
        kh_value(process_table, k) = task_info;
    }

    xSemaphoreGive(process_table_lock);
}

void process_table_remove_task(task_info_t* task_info) {
    if (xSemaphoreTake(process_table_lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to get process table mutex");
        abort();
    }

    khint_t k = kh_get(ptable, process_table, (uintptr_t)task_info->handle);
    if (k != kh_end(process_table)) {
        kh_del(ptable, process_table, k);
    } else {
        ESP_LOGE(TAG, "Attempted to remove non-existant program from process table");
    }

    xSemaphoreGive(process_table_lock);
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
    task_info->file_handles[0].is_stdin = true;
    task_info->file_handles[1].is_open = true;
    task_info->file_handles[1].is_stdout = true;
    task_info->file_handles[2].is_open = true;
    task_info->file_handles[2].is_stderr = true;

    process_table_add_task(task_info);
    vTaskSetThreadLocalStoragePointerAndDelCallback(handle, 0, task_info, task_killed);
}

void task_info_delete(task_info_t* task_info) {
    free(task_info->term);
    free(task_info);
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
                            ESP_LOGI(TAG, "Deleting killed task %p", task_info->handle);
                            kh_del(ptable, process_table, k);
                            task_info_delete(task_info);
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
    ESP_LOGI(TAG, "Initializing tasking");

    // TODO hack
    uintptr_t x = elf_find_sym("strdup");
    (void)x;

    process_table = kh_init(ptable);
    process_table_lock = xSemaphoreCreateMutex();
    xTaskCreate(cleanup_thread, "Task Cleanup Thread", 2048, NULL, 10, NULL);
}
