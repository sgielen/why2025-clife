#include <wchar.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <iconv.h>

#include "esp_log.h"
#include "rom/uart.h"

#include "task.h"

static const char *TAG = "wrapped_functions";

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
    void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (ptr) {
        task_record_resource_alloc(RES_MALLOC, ptr);
    }
    return ptr;
}

void *why_calloc(size_t nmemb, size_t size) {
    void *ptr = heap_caps_calloc(nmemb, size, MALLOC_CAP_SPIRAM);
    if (ptr) {
        task_record_resource_alloc(RES_MALLOC, ptr);
    }
    return ptr;
}

void *why_realloc(void *_Nullable ptr, size_t size) {
    void *new_ptr = heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM);
    if (new_ptr != ptr) {
        if (ptr) {
            task_record_resource_free(RES_MALLOC, ptr);
        }
        if (new_ptr) {
            task_record_resource_alloc(RES_MALLOC, new_ptr);
        }
    }
    return new_ptr;
}

void *why_reallocarray(void *_Nullable ptr, size_t nmemb, size_t size) {
    void *new_ptr = heap_caps_realloc(ptr, nmemb * size, MALLOC_CAP_SPIRAM);
    if (new_ptr != ptr) {
        if (ptr) {
            task_record_resource_free(RES_MALLOC, ptr);
        }
        if (new_ptr) {
            task_record_resource_alloc(RES_MALLOC, new_ptr);
        }
    }
    return new_ptr;
}

void why_free(void *_Nullable ptr) {
    if (ptr) {
        task_record_resource_free(RES_MALLOC, ptr);
    }
    heap_caps_free(ptr);
}

iconv_t why_iconv_open(const char *tocode, const char *fromcode) {
    iconv_t res = iconv_open(tocode, fromcode);
    if (res != (iconv_t) -1) {
        task_record_resource_alloc(RES_ICONV_OPEN, res);
    }
    return res;
}

int why_iconv_close(iconv_t cd) {
    task_record_resource_free(RES_ICONV_OPEN, cd);
    return iconv_close(cd);
}

int why_regcomp(regex_t *restrict preg, const char *restrict regex, int cflags) {
    int res = regcomp(preg, regex, cflags);
    if (res == 0) {
        task_record_resource_alloc(RES_REGCOMP, preg);
    }
    return res;
}

void why_regfree(regex_t *preg) {
    task_record_resource_free(RES_REGCOMP, preg);
    return regfree(preg);
}

int why_tcgetattr(int fd, struct termios *termios_p) {
    return 0;
}

int why_tcsetattr(int fd, int optional_actions, const struct termios *termios_p) {
    return 0;
}

wchar_t *why_wcsdup(const wchar_t *s) {
    wchar_t *ptr = wcsdup(s);
    if (ptr) {
        task_record_resource_alloc(RES_MALLOC, ptr);
    }
    return ptr;
}

char *why_strdup(const char *s) {
    char *ptr = strdup(s);
    if (ptr) {
        task_record_resource_alloc(RES_MALLOC, ptr);
    }
    return ptr;
}

char *why_strndup(const char *s, size_t n) {
    char *ptr = strndup(s, n);
    if (ptr) {
        task_record_resource_alloc(RES_MALLOC, ptr);
    }
    return ptr;
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

int why_puts(const char *str) {
    return puts(str);
}

int why_open(const char *pathname, int flags, mode_t mode) {
    task_info_t *task_info = get_task_info();
    ESP_LOGI("why_open", "Calling open from task %p for path %s", task_info->handle, pathname);
    int fd = -1;

    for (int i = 0; i < MAXFD; ++i) {
        if (task_info->file_handles[i].is_open == false) {
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

