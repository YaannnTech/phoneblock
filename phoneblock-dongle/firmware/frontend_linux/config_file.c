#include "config_file.h"

#include <stdio.h>
#include <string.h>

static char *trim(char *text)
{
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
        text++;
    }
    char *end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t'
                          || end[-1] == '\r' || end[-1] == '\n')) {
        --end;
        *end = '\0';
    }
    return text;
}

int pb_config_file_load(const char *path, pb_config_file_t *config)
{
    if (!path || !config) return -1;
    memset(config, 0, sizeof(*config));

    FILE *input = fopen(path, "r");
    if (!input) return -1;

    char line[PB_CONFIG_KEY_CAP + PB_CONFIG_VALUE_CAP + 4];
    int result = 0;
    while (fgets(line, sizeof(line), input)) {
        char *text = trim(line);
        if (!text[0] || text[0] == '#' || text[0] == ';') continue;

        char *separator = strchr(text, '=');
        if (!separator) {
            result = -1;
            break;
        }
        *separator = '\0';
        char *key = trim(text);
        char *value = trim(separator + 1);
        if (!key[0] || strlen(key) >= PB_CONFIG_KEY_CAP
                || strlen(value) >= PB_CONFIG_VALUE_CAP) {
            result = -1;
            break;
        }

        size_t entry_index;
        for (entry_index = 0; entry_index < config->count; entry_index++) {
            if (strcmp(config->entries[entry_index].key, key) == 0) break;
        }
        if (entry_index == config->count) {
            if (config->count == PB_CONFIG_MAX_ENTRIES) {
                result = -1;
                break;
            }
            config->count++;
        }
        strcpy(config->entries[entry_index].key, key);
        strcpy(config->entries[entry_index].value, value);
    }
    if (ferror(input)) result = -1;
    fclose(input);
    if (result != 0) memset(config, 0, sizeof(*config));
    return result;
}

int pb_config_file_get(const pb_config_file_t *config, const char *key,
                       char *out, size_t cap)
{
    if (!config || !key || !out || cap == 0) return -1;
    out[0] = '\0';
    for (size_t entry_index = 0; entry_index < config->count; entry_index++) {
        if (strcmp(config->entries[entry_index].key, key) != 0) continue;
        size_t value_length = strlen(config->entries[entry_index].value);
        if (value_length >= cap) return -1;
        memcpy(out, config->entries[entry_index].value, value_length + 1);
        return 1;
    }
    return 0;
}