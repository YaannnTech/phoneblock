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

// Set or replace a key in memory. Returns 0 on success and -1 when the key
// or value exceeds the configured bounds, or when the table is full.
int pb_config_file_set(pb_config_file_t *config, const char *key,
                       const char *value);

// Persist the current configuration atomically. The temporary file is created
// beside `path`, then renamed over it so readers never observe a partial file.
int pb_config_file_save(const char *path, const pb_config_file_t *config);

// Read a decimal integer. Missing, malformed, or out-of-range values return
// the supplied default.
int pb_config_file_get_int(const pb_config_file_t *config, const char *key,
                           int fallback, int minimum, int maximum);

// Read a boolean written as 1/0, true/false, yes/no, or on/off. Missing or
// unrecognized values return the supplied default.
int pb_config_file_get_bool(const pb_config_file_t *config, const char *key,
                            int fallback);