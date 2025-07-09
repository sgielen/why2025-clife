#pragma once

#include "khash.h"
#include "pathfuncs.h"

#include <stddef.h>
#include <stdint.h>

#include <sys/types.h>

KHASH_MAP_INIT_STR(devtable, void *);
extern khash_t(devtable) * device_table;

typedef struct device_s {
    int (*_open)(void *dev, path_t *path, int flags, mode_t mode);
    int (*_close)(void *dev, int fd);
    ssize_t (*_write)(void *dev, int fd, void const *buf, size_t count);
    ssize_t (*_read)(void *dev, int fd, void const *buf, size_t count);
    ssize_t (*_lseek)(void *dev, int fd, off_t offset, int whence);
} device_t;

void      device_init();
int       device_register(char const *name, device_t *device);
device_t *device_get(char const *name);
