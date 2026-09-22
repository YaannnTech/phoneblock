#pragma once

#include <stddef.h>

#define PB_CONFIG_MAX_ENTRIES 64
#define PB_CONFIG_KEY_CAP      32
#define PB_CONFIG_VALUE_CAP   256

typedef struct {
    char key[PB_CONFIG_KEY_CAP];
    char value[PB_CONFIG_VALUE_CAP];
} pb_config_entry_t;

typedef struct {
    pb_config_entry_t entries[PB_CONFIG_MAX_ENTRIES];
    size_t count;
} pb_config_file_t;

// Load an INI-style key/value file. Blank lines and lines beginning with '#'
// or ';' are ignored. Values may contain '=' and are trimmed at both ends.
// Returns 0 on success, -1 on I/O, syntax, or capacity failure.
int pb_config_file_load(const char *path, pb_config_file_t *config);

// Find a key and copy its value into the caller's buffer. Returns 1 when the
// key exists, 0 when it does not, and -1 when the output buffer is too small.
int pb_config_file_get(const pb_config_file_t *config, const char *key,
                       char *out, size_t cap);