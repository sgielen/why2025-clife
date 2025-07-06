#include <stdbool.h>
#include <time.h> // For task_info_t

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "khash.h"

KHASH_MAP_INIT_INT64(ptable, void*);
extern khash_t(ptable) *process_table;

#define MAXFD 128
#define STRERROR_BUFLEN 128

typedef struct {
   bool is_open : 1;
   bool is_stdin : 1;
   bool is_stdout : 1;
   bool is_stderr : 1;
   char* file;
} file_handle_t;

typedef struct {
    bool killed;
    int _errno;
    TaskHandle_t handle;
    size_t max_memory;
    size_t current_memory;
    size_t max_files;
    size_t current_files;
    file_handle_t file_handles[MAXFD];
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
