#include <sys/types.h>
#include <sys/errno.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_elf.h"
#include "rom/uart.h"
#include "khash.h"

#define MAXFD 128
#define STRERROR_BUFLEN 128

extern uintptr_t elf_find_sym(const char *sym_name);

static const char *TAG = "elf_loader";

extern const uint8_t test_elf_a_start[] asm("_binary_test_basic_a_elf_start");
extern const uint8_t test_elf_a_end[] asm("_binary_test_basic_a_elf_end");
extern const uint8_t test_elf_b_start[] asm("_binary_test_basic_b_elf_start");
extern const uint8_t test_elf_b_end[] asm("_binary_test_basic_b_elf_end");
extern const uint8_t test_elf_shell_start[] asm("_binary_test_shell_elf_start");
extern const uint8_t test_elf_shell_end[] asm("_binary_test_shell_elf_end");

KHASH_MAP_INIT_INT64(ptable, void*);
khash_t(ptable) *process_table;

typedef struct {
   bool is_open : 1;
   bool is_stdin : 1;
   bool is_stdout : 1;
   bool is_stderr : 1;
   char* file;
} file_handle_t;

typedef struct {
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

task_info_t *get_task_info() {
    TaskHandle_t h = xTaskGetCurrentTaskHandle();
    task_info_t *task_info = NULL;

    ESP_LOGD("get_task_info", "Got process_handle %p == %p\n", h, task_info);

    khint_t k = kh_get(ptable, process_table, (uintptr_t)h);
    if (k != kh_end(process_table)) {
       task_info = kh_value(process_table, k);
    }

    return task_info;
}

char *why_strerror(int errnum) {
    task_info_t *task_info = get_task_info();
    return strerror_r(errnum, task_info->strerror_buf, STRERROR_BUFLEN);
}

char *why_strtok(char *str, const char *delim) {
    task_info_t *task_info = get_task_info();
    return strtok_r(str, delim, &task_info->strtok_saveptr);
}

char *why_asctime(const struct tm *tm) {
    task_info_t *task_info = get_task_info();
    return asctime_r(tm, task_info->asctime_buf);
}

char *why_ctime(const time_t *timep) {
    task_info_t *task_info = get_task_info();
    return ctime_r(timep, task_info->ctime_buf);
}

struct tm *why_gmtime(const time_t *timep) {
    task_info_t *task_info = get_task_info();
    return gmtime_r(timep, &task_info->gmtime_tm);
}

struct tm *why_localtime(const time_t *timep) {
    task_info_t *task_info = get_task_info();
    return localtime_r(timep, &task_info->localtime_tm);
}

int why_rand() {
    task_info_t *task_info = get_task_info();
    return rand_r(&task_info->seed);
}

void why_srand(unsigned int seed) {
    task_info_t *task_info = get_task_info();
    task_info->seed = seed;
}

void why_srandom(unsigned int seed) {
    task_info_t *task_info = get_task_info();
    task_info->seed = seed;
}

long why_random() {
    task_info_t *task_info = get_task_info();
    return rand_r(&task_info->seed);
}

int *why_errno() {
    task_info_t *task_info = get_task_info();
    return &task_info->_errno;
}

int why_isatty(int fd) {
    task_info_t *task_info = get_task_info();
    ESP_LOGI("why_isatty", "Calling isatty from task %p", task_info->handle);
    return 1;
}

char *why_getenv(const char *name) {
    task_info_t *task_info = get_task_info();
    ESP_LOGW("why_getenv", "Calling getenv from task %p variable %s", task_info->handle, name);

    if (strcmp(name, "TERM") == 0 ) {
        return "line";
    }

    return NULL;
}

int why_atexit(void (*function)(void)) {
    task_info_t *task_info = get_task_info();
    ESP_LOGI("why_atexit", "Calling atexit from task %p", task_info->handle);

    return 0;
}

void *why_malloc(size_t size) {
    task_info_t *task_info = get_task_info();
    ESP_LOGI("why_malloc", "Calling malloc from task %p with size %zi", task_info->handle, size);

    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
}

int why_regcomp(regex_t *restrict preg, const char *restrict regex, int cflags) {
    task_info_t *task_info = get_task_info();
    ESP_LOGI("why_regcomp", "Calling regcomp from task %p", task_info->handle);

    return regcomp(preg, regex, cflags);
}

void why_regfree(regex_t *preg) {
    task_info_t *task_info = get_task_info();
    ESP_LOGI("why_regfree", "Calling regfree from task %p", task_info->handle);

    return regfree(preg);
}

int why_tcgetattr(int fd, struct termios *termios_p) {
    return 0;
}

int why_tcsetattr(int fd, int optional_actions, const struct termios *termios_p) {
    return 0;
}

char *why_strdup(const char *s) {
    task_info_t *task_info = get_task_info();
    ESP_LOGI("why_strdup", "Calling strdup from task %p", task_info->handle);

    return strdup(s);
}

char *why_strndup(const char *s, size_t n) {
    task_info_t *task_info = get_task_info();
    ESP_LOGI("why_strndup", "Calling strndup from task %p", task_info->handle);
    return strndup(s, n);
}

void why_free(void *_Nullable ptr) {
    task_info_t *task_info = get_task_info();
    ESP_LOGI("why_free", "Calling free from task %p", task_info->handle);
    heap_caps_free(ptr);
}

void *why_calloc(size_t nmemb, size_t size) {
    task_info_t *task_info = get_task_info();
    ESP_LOGI("why_calloc", "Calling calloc from task %p", task_info->handle);
    return calloc(nmemb, size);
}

void *why_realloc(void *_Nullable ptr, size_t size) {
    task_info_t *task_info = get_task_info();
    ESP_LOGI("why_realloc", "Calling realloc from task %p", task_info->handle);
    return realloc(ptr, size);
}

void *why_reallocarray(void *_Nullable ptr, size_t nmemb, size_t size) {
    task_info_t *task_info = get_task_info();
    ESP_LOGI("why_reallocarray", "Calling reallocarray from task %p", task_info->handle);
    return reallocarray(ptr, nmemb, size);
}

ssize_t why_write(int fd, const void *buf, size_t count) {
    task_info_t *task_info = get_task_info();

    ESP_LOGI("why_write", "Calling write from task %p fd = %i count = %zi", task_info->handle, fd, count);
    if (task_info->file_handles[fd].is_stdout == true) {
        ESP_LOGI("why_write", "Calling write from task %p fd = %i count = %zi is_stdout == true", task_info->handle, fd, count);
	    for (size_t i = 0; i < count; ++i) {
		    putchar(((const char*)buf)[i]);
	    }
	    return count;
    }
    return 0;
}

ssize_t why_read(int fd, void *buf, size_t count) {
    task_info_t *task_info = get_task_info();
    ESP_LOGI("why_read", "Calling read from task %p fd = %i count = %zi", task_info->handle, fd, count);

    if (task_info->file_handles[fd].is_stdin == true) {
        uint8_t c;
        while (1) {
            ETS_STATUS s = uart_rx_one_char(&c);
            if (s == ETS_OK) break;
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        ((char*)buf)[0] = c;
	    return 1;
    }

    return 0;
}

off_t why_lseek(int fd, off_t offset, int whence) {
    task_info_t *task_info = get_task_info();
    ESP_LOGI("why_lseek", "Calling lseek from task %p", task_info->handle);
    return 0;
}

int why_close(int fd) {
    task_info_t *task_info = get_task_info();
    ESP_LOGI("why_close", "Calling close from task %p", task_info->handle);

    if (fd > MAXFD) goto out;

    if (task_info->file_handles[fd].is_open) {
        free(task_info->file_handles[fd].file);
        memset(&task_info->file_handles[fd], 0, sizeof(file_handle_t));
        return 0;
    }

out:
    task_info->_errno = EBADF;
    return -1;
}

int why_puts(const char *str) {
    return puts(str);
}

int why_open(const char *pathname, int flags, mode_t mode) {
    task_info_t *task_info = get_task_info();
    ESP_LOGI("why_open", "Calling open from task %p for path %s", task_info->handle, pathname);
    int fd = -1;

    for (int i = 0; i < MAXFD; ++i) {
        if (task_info->file_handles[fd].is_open == false) {
            fd = i;
            break;
        }
    }

    if (fd == -1) {
        task_info->_errno = ENOMEM;
        goto out;
    }

    if (strcmp(pathname, "SYS$INPUT") == 0) {
        task_info->file_handles[fd].is_stdin = true;
    }

    if (strcmp(pathname, "SYS$OUTPUT") == 0) {
        task_info->file_handles[fd].is_stdout = true;
    }

    if (strcmp(pathname, "SYS$ERROR") == 0) {
        task_info->file_handles[fd].is_stderr = true;
    }

out:
    ESP_LOGI("why_open", "Calling open from task %p for path %s returning %i", task_info->handle, pathname, fd);
    return fd;
}

void run_elf(void *buffer) {
    int argc = 0;
    char **argv = NULL;
    int ret;

    TaskHandle_t h = xTaskGetCurrentTaskHandle();

    task_info_t task_info;
    memset(&task_info, 0, sizeof(task_info));

    task_info.term = strdup("dumb");
    task_info.current_files = 3;
    task_info.handle = h;
    task_info.file_handles[0].is_open = true;
    task_info.file_handles[0].is_stdin = true;
    task_info.file_handles[1].is_open = true;
    task_info.file_handles[1].is_stdout = true;
    task_info.file_handles[2].is_open = true;
    task_info.file_handles[2].is_stderr = true;

    printf("Setting process_handle to %p == %p\n", h, &task_info);
    int r;
    khint_t k = kh_put(ptable, process_table, (uintptr_t)h, &r);
    if (r >= 0) {
        kh_value(process_table, k) = &task_info;
    }

    esp_elf_t elf;

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

    ESP_LOGI(TAG, "Start to run ELF file");

    esp_elf_request(&elf, 0, argc, argv);

    ESP_LOGI(TAG, "Success to exit from ELF file");

    esp_elf_deinit(&elf);
}

int app_main(void) {
    // TODO Bit of a hack to prevent elf_find_sym from being removed.
    elf_find_sym("strdup");

    process_table = kh_init(ptable);
    TaskHandle_t elf_a, elf_b;
    printf("Hello ESP32P4 firmware\n");

    xTaskCreate(run_elf, "Task1", 16384, test_elf_shell_start, 5, &elf_a); 
//    xTaskCreate(run_elf, "Task2", 4096, test_elf_b_start, 5, &elf_b); 


    while(1) {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    };
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

    return 0;
}
