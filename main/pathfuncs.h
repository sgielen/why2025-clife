#pragma once

#include <stdbool.h>

#include <string.h>

typedef struct {
    char  *buffer;
    char  *device;
    char  *directory;
    char  *filename;
    char  *unixpath;
    size_t len;
} path_t;

typedef enum {
    PATH_PARSE_OK                  = 0,
    PATH_PARSE_EMPTY_DEVICE        = -1,
    PATH_PARSE_NO_DEVICE           = -2,
    PATH_PARSE_UNCLOSED_DIRECTORY  = -3,
    PATH_PARSE_INVALID_DEVICE_CHAR = -4,
    PATH_PARSE_INVALID_DIR_CHAR    = -5,
    PATH_PARSE_INVALID_FILE_CHAR   = -6,
    PATH_PARSE_EMPTY_PATH          = -7
} path_parse_result_t;

static inline bool is_valid_device_char(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
           c == '$';
}

static inline bool is_valid_path_char(char c) {
    return is_valid_device_char(c) || c == '.';
}

path_parse_result_t parse_path(char const *path, path_t *result);
char               *path_to_unix(path_t *path);
void                path_free(path_t *path);
