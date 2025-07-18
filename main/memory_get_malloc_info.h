#include "esp_attr.h"
#include "task.h"

__attribute__((always_inline)) static inline IRAM_ATTR struct malloc_state *get_malloc_state() {
    task_info_t *task_info = get_task_info();
    if (!task_info) {
        ESP_DRAM_LOGE(DRAM_STR("get_malloc_state"), "called without task_info");
        return NULL;
    }
    return &task_info->psram->malloc_state;
}

__attribute__((always_inline)) static inline IRAM_ATTR struct malloc_params *get_malloc_params() {
    task_info_t *task_info = get_task_info();
    if (!task_info) {
        ESP_DRAM_LOGE(DRAM_STR("get_malloc_state"), "called without task_info");
        return NULL;
    }
    return &task_info->psram->malloc_params;
}
