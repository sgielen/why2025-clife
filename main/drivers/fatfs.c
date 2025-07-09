#include <stdbool.h>
#include <fcntl.h>
#include <stdio.h>

#include "esp_vfs.h"
#include "esp_vfs_fat.h"

#include "freertos/FreeRTOS.h"
#include "fatfs.h"

typedef struct {
    device_t device;
    wl_handle_t wl_handle;
    char *base_path;
} fatfs_device_t;

static int fatfs_open(void *dev, path_t *path, int flags, mode_t mode) {
    fatfs_device_t *device = dev;
    char *unixpath = path_to_unix(path);

    int ret = open(unixpath, flags, mode);
    return ret;
}

static int fatfs_close(void *dev, int fd) {
    return close(fd);
}

static ssize_t fatfs_write(void *dev, int fd, const void *buf, size_t count) {
    return write(fd, buf, count);
}

static ssize_t fatfs_read(void *dev, int fd, const void *buf, size_t count) {
    return read(fd, buf, count);
}

static ssize_t fatfs_lseek(void *dev, int fd, off_t offset, int whence) {
    return lseek(fd, offset, whence);
}

device_t *fatfs_create_spi(const char *devname, const char *partname, bool rw) {
    const esp_vfs_fat_mount_config_t mount_config = {
            .max_files = 4,
            .format_if_mount_failed = false,
            .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
            .use_one_fat = false,
    };

    fatfs_device_t *dev = malloc(sizeof(fatfs_device_t));
    dev->base_path = malloc(strlen(devname) + 2);
    dev->base_path[0] = '/';
    strcpy(dev->base_path + 1, devname);

    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(dev->base_path, partname, &mount_config, &dev->wl_handle);
    dev->device._open = fatfs_open;
    dev->device._close = fatfs_close;
    dev->device._write = fatfs_write;
    dev->device._read = fatfs_read;
    dev->device._lseek = fatfs_lseek;

    return (device_t*)dev;
}
