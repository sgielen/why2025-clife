#ifndef ESP_LOGE
#define ESP_LOGE(tag, ...) do { fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
#endif

#define TAG logical_names

#include "khash.h"
#include "hash_helper.h"
#include "logical_names.h"

#define MAX_DIR_DEPTH 25
#define RESOLVE_MAX_DEPTH 15

#include <stdio.h>

typedef struct {
    char *target;
    bool terminal;
} logical_name_target_t;

typedef struct {
    char *pointer;
    size_t len;
} raw_string_t;

static const raw_string_t raw_null = {
    .pointer = NULL,
    .len = 0,
};

typedef struct {
    raw_string_t unparsable;
    raw_string_t device;
    raw_string_t dir_components[MAX_DIR_DEPTH];
    int dir_count;
    raw_string_t filename;
} parsed_components_t;

static const parsed_components_t parsed_components_null = {
    raw_null,
    raw_null,
    { raw_null },
    0,
    raw_null
};

KHASH_MAP_INIT_STR(lnametable, logical_name_target_t);
khash_t(lnametable) * logical_name_table;

static inline bool raw_cmp(raw_string_t *l, raw_string_t *r) {
    if (l->pointer != r->pointer) return false;
    if (l->len != r->len) return false;

    return true;
}

static inline raw_string_t raw_from_cstr(char *cstr) {
    raw_string_t res = {
        .pointer = cstr,
        .len = strlen(cstr),
    };

    return res;
}

static inline raw_string_t raw_from_ptr(char *pointer, int len) {
    raw_string_t res = {
        .pointer = pointer,
        .len = len,
    };

    return res;
}

#ifndef RUN_TEST
static inline void raw_print(raw_string_t r) {
    if (r.len == 0 || r.pointer == NULL) { printf("len: %zi '(null)'", r.len); return; }
    char b = r.pointer[r.len];
    r.pointer[r.len] = '\0';
    printf("len: %zi '%s'", r.len, r.pointer);
    r.pointer[r.len] = b;
}

static void parsed_components_dump(parsed_components_t components) {
    printf("Components dump: \n");
    printf("    Unparsable: "); raw_print(components.unparsable); printf("\n");
    printf("    Device    : "); raw_print(components.device); printf("\n");
    printf("    Filename  : "); raw_print(components.filename); printf("\n");
    for (int i = 0; i < components.dir_count; ++i) {
        printf("    Dir[%i]   : ", i); raw_print(components.dir_components[i]); printf("\n");
    }
}
#endif

static char *parsed_components_serialize(parsed_components_t components) {
    if (components.unparsable.len == 0 && components.device.len == 0) {
        // Null value
        return NULL;
    }

    if (components.unparsable.len != 0) {
        // Just copy the junk into a fresh c string
        char *res = malloc(components.unparsable.len + 1);
        memcpy(res, components.unparsable.pointer, components.unparsable.len);
        res[components.unparsable.len] = '\0';
        return res;
    }

    size_t result_len = components.device.len;
    // If our device name doesn't already have a ':' we will need to add it
    if (components.device.pointer[components.device.len - 1] != ':') result_len++;

    result_len += components.filename.len;
    for (int i = 0; i < components.dir_count; ++i) {
        result_len += components.dir_components[i].len;
    }

    // Directories + '[]'
    if (components.dir_count) result_len += 2;
    // One '.' per subdir
    if (components.dir_count > 1) result_len += components.dir_count - 1;

    char *res = malloc(result_len + 1);
    res[result_len] = '\0';

    size_t offset = 0;
    memcpy(res, components.device.pointer, components.device.len);
    offset += components.device.len;

    // Our device doesn't already have a ':', add it.
    if (components.device.pointer[components.device.len - 1] != ':') res[offset++] = ':';

    if (components.dir_count) {
        res[offset++] = '[';
        for (int i = 0; i < components.dir_count; ++i) {
            // Separator dot for subdirs
            if (i) res[offset++] = '.';
            memcpy(res + offset, components.dir_components[i].pointer, components.dir_components[i].len);
            offset += components.dir_components[i].len;
        }
        res[offset++] = ']';
    }

    if (components.filename.len) {
        memcpy(res + offset, components.filename.pointer, components.filename.len);
    }
    
    return res;
}

static inline bool path_cmp(parsed_components_t *l, parsed_components_t *r) {
    if (l->unparsable.len != r->unparsable.len) return false;
    if (l->unparsable.len && r->unparsable.len) {
        if (raw_cmp(&l->unparsable, &r->unparsable)) return true;
    }

    if (l->dir_count != r->dir_count) return false;
    if (!raw_cmp(&l->device, &r->device)) return false;
    if (!raw_cmp(&l->filename, &r->filename)) return false;

    for (int i = 0; i < l->dir_count; ++i) {
        if (!raw_cmp(&l->dir_components[i], &r->dir_components[i])) return false;
    }

    return true;
}

static inline parsed_components_t parse_string(raw_string_t str) {
    parsed_components_t res = parsed_components_null;

    char *device_separator = NULL;
    char *dir_start = NULL;
    char *dir_end = NULL;
    char *last_dir = NULL;

    size_t string_size = str.len;
    for (size_t i = 0; i < string_size; ++i) {
        char c = str.pointer[i];

        if (c == ':') {
            // Device separator
            if (device_separator) goto error;
            device_separator = str.pointer + i;
            res.device = raw_from_ptr(str.pointer, i);
            continue;
        }

        if (c == '[') {
            // Dir start
            if (dir_start) goto error;
            dir_start = str.pointer + i;
            last_dir = str.pointer + i + 1;
            continue;
        }

        if (c == ']') {
            // Dir end
            if (dir_end) goto error;
            dir_end = str.pointer + i;
            res.dir_components[res.dir_count] = raw_from_ptr(last_dir, (uintptr_t)(str.pointer + i) - (uintptr_t)last_dir);
            res.dir_count++;
            continue;
        }

        if (c == '.' && dir_start && !dir_end) {
            // We're parsing directories
            res.dir_components[res.dir_count] = raw_from_ptr(last_dir, (uintptr_t)(str.pointer + i) - (uintptr_t)last_dir);
            res.dir_count++;
            last_dir = str.pointer + i + 1;
        }
    }

    // No device separator
    if (!device_separator) goto error;
    // Opening '[' but no closing ']'
    if (dir_start && !dir_end) goto error;

    if (dir_end) {
        if (dir_end + 1 < str.pointer + string_size) {
            // We have a filename after a directory
            res.filename = raw_from_ptr(dir_end + 1, string_size - ((uintptr_t)(dir_end + 1) - (uintptr_t)str.pointer));
        }
        // There was no file part
    } else if (device_separator + 1 < str.pointer + string_size) {
        // We have a filename directly after the device
        res.filename = raw_from_ptr(device_separator + 1, string_size - ((uintptr_t)(device_separator + 1) - (uintptr_t)str.pointer));
    }

    return res;

error:
    res.unparsable = str;
    return res;
}

static inline parsed_components_t parse_cstring(char *cstr) {
    raw_string_t str = raw_from_cstr(cstr);
    return parse_string(str);
}

static raw_string_t resolve_string(raw_string_t string, int depth) {
    if (depth > RESOLVE_MAX_DEPTH || string.len == 0) {
        return raw_null;
    }

    // This is safe because we are always pointing into a cstr
    // the next character is, at worst, already a '\0'
    char char_bak = string.pointer[string.len];
    string.pointer[string.len] = '\0';

    logical_name_target_t *name;
    khint_t k = kh_get(lnametable, logical_name_table, string.pointer);
    if (k == kh_end(logical_name_table)) {
        string.pointer[string.len] = char_bak;
        return string;
    } else {
        name = &kh_val(logical_name_table, k);
        string.pointer[string.len] = char_bak;
        return resolve_string(raw_from_cstr((char*)name->target), ++depth);
    }
}

static raw_string_t resolve_device_string(raw_string_t string, int depth) {
    if (depth > RESOLVE_MAX_DEPTH || string.len == 0) {
        return raw_null;
    }

    // Special case for devices. Once we have a valid device path we don't
    // actually know whether the logical name is for DEVICE or DEVICE:
    // so we need to try both. We can rely on the fact that the input string
    // must have contained a ':' at some point, so there is definitely enough
    // space.

    char char_bak = string.pointer[string.len];
    string.pointer[string.len] = ':';
    string.len += 1;

    raw_string_t new_string = resolve_string(string, depth);

    if (raw_cmp(&string, &new_string)) {
        // This didn't work. Try without the ':'
        string.len -= 1;
        string.pointer[string.len] = char_bak;
        return resolve_string(string, depth);
    }

    // Don't strip any trailing ':', this avoids trouble if both DEVICE
    // and DEVICE: are defined
    return new_string;
}

static parsed_components_t _logical_name_resolve(parsed_components_t path, int depth) {
    if (depth > RESOLVE_MAX_DEPTH) {
        return parsed_components_null;
    }

    if (path.unparsable.len) {
        // Just a string
        raw_string_t res = resolve_string(path.unparsable, depth + 1);

        // 1) If we looped or otherwise hit the max depth don't do anything
        // 2) If there was no change we are done
        if (!res.len || raw_cmp(&res, &path.unparsable)) {
            return path;
        }
        // We might have a path now. Try again.
        parsed_components_t new_path = parse_string(res);
        return(_logical_name_resolve(new_path, depth + 1));
    }

    parsed_components_t orig_path = path;

    // Actual path of some kind
    raw_string_t new_device = resolve_device_string(path.device, depth + 1);
    if (!raw_cmp(&new_device, &path.device)) {
        // Our device might have expanded into a bigger path
        parsed_components_t device_path = parse_string(new_device);
        if (device_path.unparsable.len) {
            path.device = new_device;
        } else {
            if (device_path.dir_count) {
                // Our device expanded to something with directories
                // insert them to the left of our existing directories
                if (path.dir_count) {
                    // Move our existing directories to the right
                    memmove(&path.dir_components[device_path.dir_count], path.dir_components, path.dir_count * sizeof(raw_string_t));
                }
                path.dir_count += device_path.dir_count;
                
                // Insert our new dir components
                memcpy(path.dir_components, device_path.dir_components, device_path.dir_count * sizeof(raw_string_t));
            }

            if (device_path.filename.len) {
                path.filename = device_path.filename;
            }

            if (device_path.device.len) {
                path.device = device_path.device;
            }
        }
    }
    
    path.filename = resolve_string(path.filename, depth + 1);
    for (int i = 0; i < path.dir_count; ++i) {
        path.dir_components[i] = resolve_string(path.dir_components[i], depth + 1);
    }

    if (path_cmp(&orig_path, &path)) {
        return path;
    }

    return _logical_name_resolve(path, depth + 1);
}

void logical_names_system_init() {
    logical_name_table = kh_init(lnametable);
}

int logical_name_set(const char *logical_name, const char *target, bool is_terminal) {
    logical_name_target_t name;
    // Copy the string so we won't have to copy it later when resolving
    // and add 1 more byte because we might need to resolve it as a device
    name.target = malloc(strlen(target) + 2);
    strcpy(name.target, target);
    name.terminal = is_terminal;
    khash_insert_str(lnametable, logical_name_table, logical_name, name, const char*);

    return 0;
}

void logical_name_del(const char *logical_name) {
    khash_del_str(lnametable, logical_name_table, logical_name, "Logical name did not exist");
}


char *logical_name_resolve(char *logical_name) {
    if (!logical_name || !strlen(logical_name)) {
        // Don't try and parse empty strings or NULL
        return strdup("");
    }
    parsed_components_t parsed = _logical_name_resolve(parse_cstring(logical_name), 0);
    return parsed_components_serialize(parsed);
}

char *logical_name_resolve_const(const char *logical_name) {
    char *tmp = strdup(logical_name);
    char *res = logical_name_resolve(tmp);
    free(tmp);
    return res;
}

#ifdef RUN_TEST

typedef struct {
    const char *in;
    const char *expect;
} test_t;

test_t tests[] = {
    // Undefined strings should pass right through
    { "STRING", "STRING" },
    { "DEVICE:", "DEVICE:" },
    { "DEVICE:filename.ext", "DEVICE:filename.ext" },
    { "DEVICE:[dira]filename.ext", "DEVICE:[dira]filename.ext" },
    { "DEVICE:[dira.dirb.dirc]filename.ext", "DEVICE:[dira.dirb.dirc]filename.ext" },

    // Simple substitutions
    { "SIMPLE", "STRING" },
    { "SIMPLEDEV:", "MY_SIMPLEDEV:" },

    { "USER:", "MYFLASH:[dira]" },
    { "FLASH0:", "MYFLASH:" },
    { "FLASH0", "MYFLASH" },
    { "USER:file.txt", "MYFLASH:[dira]file.txt" },
    { "USER:[dirb.dirc]file.txt", "MYFLASH:[dira.dirb.dirc]file.txt" },
    { "TEST1:", "DRIVE0:" },
    { "TEST2:", "DRIVE0:" },
    { "TEST3:", "DRIVE0:[dira]" },
    { "TEST4:", "DRIVE0:[dira.dirb]" },
    { "TEST5", "DRIVE0:[dira.dirb]filename.ext" },

    // Directory name substitions
    { "USER:[DIR1]", "MYFLASH:[dira.SUBST1]"},
    { "USER:[DIR1.DIR2]", "MYFLASH:[dira.SUBST1.SUBST2]"},
    { "USER:[DIR1.DIR2.DIR3]", "MYFLASH:[dira.SUBST1.SUBST2.STRING]"},

    // File name substititions
    { "USER:[DIR1]FILE", "MYFLASH:[dira.SUBST1]FILE" },
    { "USER:[DIR1]FILE1", "MYFLASH:[dira.SUBST1]FILENAME.EXT" },
    { "USER:[DIR1]FILE2", "MYFLASH:[dira.SUBST1]INDIRECT.EXT" },

    // Error checks
    // Return original for loops
    { "CIRC1", "CIRC1" },
    { "CIRC2", "CIRC2" },

    // Bad path, treat as string
    { "BAD:[unclosed", "BAD:[unclosed" },
    { "DOUBLE::COLON", "DOUBLE::COLON" }, 

    { "", "" },

    { NULL, NULL},
};

int main() {
    logical_names_system_init();

    logical_name_set("SIMPLE", "STRING", false);
    logical_name_set("DIR1", "SUBST1", false);
    logical_name_set("DIR2", "SUBST2", false);
    logical_name_set("DIR3", "SIMPLE", false);
    logical_name_set("FILE1", "FILENAME.EXT", false);
    logical_name_set("FILE2", "FILE3", false);
    logical_name_set("FILE3", "INDIRECT.EXT", false);
    logical_name_set("SIMPLEDEV:", "MY_SIMPLEDEV:", false);
    logical_name_set("USER", "FLASH0:[dira]", false);
    logical_name_set("FLASH0", "MYFLASH", false);
    logical_name_set("TEST1", "DRIVE0", false);
    logical_name_set("TEST2", "TEST1:", false);
    logical_name_set("TEST3", "TEST2:[dira]", false);
    logical_name_set("TEST4", "TEST3:[dirb]", false);
    logical_name_set("TEST5", "TEST4:filename.ext", false);

    logical_name_set("CIRC1", "CIRC2", false);
    logical_name_set("CIRC2", "CIRC1", false);

    bool error = false;
    char *res;

    test_t *test = tests;
    while(test->in) {
        printf("=== Running test for '%s' ('%s') === \n", test->in, test->expect);
        res = logical_name_resolve_const(test->in);
        bool fail = false;
        if (res && test->expect && strcmp(test->expect, res) != 0) fail = true;
        if (res && !test->expect) fail = true;
        if (!res && test->expect) fail = true;

        if (fail) {
            printf("\033[31mTestcase '%s' failed.\n\tExpected: '%s'\n\tActual:   '%s'\033[0m\n", test->in, test->expect, res);
            error = true;
        }
        printf("=== End     test for %s === \n\n", test->in);
        free(res);
        ++test;
    }

    if (!error) {
        printf("\033[32mAll tests passed\033[0m\n");
    }

    logical_name_target_t x;
    kh_foreach_value(logical_name_table, x, free((void*)x.target));
    kh_destroy(lnametable, logical_name_table);
}
#endif
