#include "esp_attr.h"
#include "task.h"

__attribute__((always_inline)) static inline IRAM_ATTR struct malloc_state *get_malloc_state() {
    task_info_t *task_info = get_task_info();
    return &task_info->malloc_state;
}

//__attribute__((always_inline)) static inline struct IRAM_ATTR malloc_params *get_malloc_params() {
static IRAM_ATTR struct malloc_params *get_malloc_params() {
    task_info_t *task_info = get_task_info();
    return &task_info->malloc_params;
}
