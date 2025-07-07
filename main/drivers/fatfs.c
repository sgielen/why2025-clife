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

char *vms_path_to_unix(fatfs_device_t *device, const char *path) {
    size_t len = strlen(path);
    // VMS paths can be a little shorter than unix paths
    // DEVICE:FILE is shorter than /DEVICE/FILE by 1
    char *unixpath = calloc(len + 2, 1);
    strcpy(unixpath, device->base_path);
    size_t o = strlen(device->base_path);

    bool in_path = false;
    bool in_file = false;
    for (size_t i = 0; i < len; ++i) {
        char c = path[i];

        if (c == ':') {
            if (path[i + 1] == '[') {
                ++i;
                unixpath[o++] = '/';
                in_path = true;
            } else {
                in_file = true;
                unixpath[o++] = '/';
            }
            continue;
        }

        if (in_path) {
            if (c == '.') c = '/';
            if (c == ']') {
                in_path = false;
                in_file = true;
                unixpath[o++] = '/';
                continue;
            } 
        }

        if (in_path || in_file) unixpath[o++] = c;
    }

    unixpath[o++] = '\0';
    printf("vms_path_to_unix: %s == %s\n", path, unixpath);
    return unixpath;
}

int fatfs_open(void *dev, const char *path, int flags, mode_t mode) {
    fatfs_device_t *device = dev;
    char *unixpath = vms_path_to_unix(device, path);

    int ret = open(unixpath, flags, mode);
    free(unixpath);

    return ret;
}

int fatfs_close(void *dev, int fd) {
    return close(fd);
}

ssize_t fatfs_write(void *dev, int fd, const void *buf, size_t count) {
    return write(fd, buf, count);
}

ssize_t fatfs_read(void *dev, int fd, const void *buf, size_t count) {
    return read(fd, buf, count);
}

ssize_t fatfs_lseek(void *dev, int fd, off_t offset, int whence) {
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
