#include <stdbool.h>
#include <time.h> // For task_info_t

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "device.h"
#include "khash.h"

KHASH_MAP_INIT_INT(ptable, void*);
KHASH_MAP_INIT_INT(restable, int);

extern khash_t(ptable) *process_table;

#define MAXFD 128
#define STRERROR_BUFLEN 128

enum task_resource_type {
    RES_MALLOC,
    RES_ICONV_OPEN,
    RES_REGCOMP,
    RES_OPEN,
    RES_RESOURCE_TYPE_MAX
};

typedef struct {
    bool is_open;
    int dev_fd;
    device_t *device;
} file_handle_t;

typedef struct {
    bool killed;
    TaskHandle_t handle;
    khash_t(restable) *resources[RES_RESOURCE_TYPE_MAX];

    size_t max_memory;
    size_t current_memory;
    size_t max_files;
    size_t current_files;
    file_handle_t file_handles[MAXFD];

    int _errno;
    char *term;
    char strerror_buf[STRERROR_BUFLEN];
    unsigned int seed;
    char *strtok_saveptr;
    char asctime_buf[26];
    char ctime_buf[26];
    struct tm gmtime_tm;
    struct tm localtime_tm;
} task_info_t;


void task_init();
task_info_t *get_task_info();
void run_elf(void *buffer);
void task_record_resource_alloc(enum task_resource_type type, void *ptr);
void task_record_resource_free(enum task_resource_type type, void *ptr);
