/* This file is part of BadgeVMS
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "device.h"
#include "dlmalloc.h"
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
#define NUM_PIDS        128
#define MAX_PID         127
#define MIN_STACK_SIZE  8192

// We want negative numbers for error conditions
#define PID_T_TYPE int16_t
typedef PID_T_TYPE why_pid_t;

typedef enum { RES_ICONV_OPEN, RES_REGCOMP, RES_OPEN, RES_RESOURCE_TYPE_MAX } task_resource_type_t;

typedef enum {
    TASK_TYPE_ELF,
    TASK_TYPE_ELF_ROM,
} task_type_t;

typedef struct {
    bool      is_open;
    int       dev_fd;
    device_t *device;
} file_handle_t;

typedef struct task_info {
    why_pid_t    pid;
    TaskHandle_t handle;
    khash_t(restable) * resources[RES_RESOURCE_TYPE_MAX];

    struct malloc_state malloc_state;
    uintptr_t           heap_start;
    uintptr_t           heap_end;
    size_t              heap_size;

    void       *data;
    task_type_t type;
    char       *buffer;
    bool        buffer_in_rom;
    int         argc;
    char      **argv;
    char      **argv_back;
    size_t      argv_size;
    void (*task_entry)(struct task_info *task_info);

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

__attribute__((always_inline)) inline static task_info_t *get_task_info() {
    task_info_t *task_info = pvTaskGetThreadLocalStoragePointer(NULL, 0);

    return task_info;
}

void      task_init();
why_pid_t run_task(void *buffer, int stack_size, task_type_t type, int argc, char *argv[]);
void      task_record_resource_alloc(task_resource_type_t type, void *ptr);
void      task_record_resource_free(task_resource_type_t type, void *ptr);
