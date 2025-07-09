#include "device.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "khash.h"

#include <stdatomic.h>
#include <stdbool.h>

#include <time.h> // For task_info_t

KHASH_MAP_INIT_INT(ptable, void *);
KHASH_MAP_INIT_INT(restable, int);

#define MAXFD           128
#define STRERROR_BUFLEN 128
#define NUM_PIDS        256
#define MAX_PID         255

// 255 pids, but we want negative numbers for error conditions
#define PID_T_TYPE int16_t
typedef PID_T_TYPE why_pid_t;

typedef enum { RES_MALLOC, RES_ICONV_OPEN, RES_REGCOMP, RES_OPEN, RES_RESOURCE_TYPE_MAX } task_resource_type_t;

typedef enum {
    TASK_TYPE_ELF,
    TASK_TYPE_ELF_ROM,
} task_type_t;

typedef struct {
    bool      is_open;
    int       dev_fd;
    device_t *device;
} file_handle_t;

typedef struct task_parameters {
    why_pid_t pid;
    char     *buffer;
    bool      buffer_in_rom;
    int       argc;
    char    **argv;
    void (*task_entry)(struct task_parameters *parameters);
} task_parameters_t;

typedef struct {
    why_pid_t          pid;
    atomic_bool        killed;
    TaskHandle_t       handle;
    task_parameters_t *task_parameters;
    khash_t(restable) * resources[RES_RESOURCE_TYPE_MAX];

    size_t        max_memory;
    size_t        current_memory;
    size_t        max_files;
    size_t        current_files;
    file_handle_t file_handles[MAXFD];

    int          _errno;
    char        *term;
    char         strerror_buf[STRERROR_BUFLEN];
    unsigned int seed;
    char        *strtok_saveptr;
    char         asctime_buf[26];
    char         ctime_buf[26];
    struct tm    gmtime_tm;
    struct tm    localtime_tm;
} task_info_t;

void         task_init();
task_info_t *get_task_info();
why_pid_t    run_task(void *buffer, int stack_size, task_type_t type, int argc, char *argv[]);
void         task_record_resource_alloc(task_resource_type_t type, void *ptr);
void         task_record_resource_free(task_resource_type_t type, void *ptr);
