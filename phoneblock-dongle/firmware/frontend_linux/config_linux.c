#include "config_linux.h"

#include "config_file.h"

#include <stdio.h>
#include <string.h>

static void copy_value(char *destination, size_t capacity, const char *value)
{
    snprintf(destination, capacity, "%s", value ? value : "");
}

void pb_linux_config_defaults(pb_linux_config_t *config)
{
    if (!config) return;
    memset(config, 0, sizeof(*config));
    config->sip_port = 5060;
    config->sip_expires = 3600;
    config->sip_local_port = 15060;
    config->rtp_port = 16000;
    copy_value(config->phoneblock_base_url,
               sizeof(config->phoneblock_base_url),
               "https://phoneblock.net/phoneblock");
}

int pb_linux_config_load(const char *path, pb_linux_config_t *config)
{
    if (!path || !config) return -1;
    pb_linux_config_defaults(config);

    pb_config_file_t file;
    if (pb_config_file_load(path, &file) != 0) {
        FILE *input = fopen(path, "r");
        if (!input) return 0;
        fclose(input);
        return -1;
    }

    char value[PB_CONFIG_VALUE_CAP];
    if (pb_config_file_get(&file, "sip_host", value, sizeof(value)) == 1) {
        copy_value(config->sip_host, sizeof(config->sip_host), value);
    }
    if (pb_config_file_get(&file, "sip_user", value, sizeof(value)) == 1) {
        copy_value(config->sip_user, sizeof(config->sip_user), value);
    }
    if (pb_config_file_get(&file, "sip_pass", value, sizeof(value)) == 1) {
        copy_value(config->sip_pass, sizeof(config->sip_pass), value);
    }
    if (pb_config_file_get(&file, "phoneblock_base_url", value,
                           sizeof(value)) == 1) {
        copy_value(config->phoneblock_base_url,
                   sizeof(config->phoneblock_base_url), value);
    }
    if (pb_config_file_get(&file, "phoneblock_token", value,
                           sizeof(value)) == 1) {
        copy_value(config->phoneblock_token,
                   sizeof(config->phoneblock_token), value);
    }
    config->sip_port = pb_config_file_get_int(&file, "sip_port",
                                               config->sip_port, 1, 65535);
    config->sip_expires = pb_config_file_get_int(&file, "sip_expires",
                                                  config->sip_expires, 60, 86400);
    config->sip_local_port = pb_config_file_get_int(&file, "sip_local_port",
                                                     config->sip_local_port,
                                                     1, 65535);
    config->rtp_port = pb_config_file_get_int(&file, "rtp_port",
                                               config->rtp_port, 1, 65535);
    return 0;
}

int pb_linux_config_save(const char *path, const pb_linux_config_t *config)
{
    if (!path || !config) return -1;

    pb_config_file_t file;
    if (pb_config_file_load(path, &file) != 0) {
        FILE *input = fopen(path, "r");
        if (input) {
            fclose(input);
            return -1;
        }
        memset(&file, 0, sizeof(file));
    }

    char number[24];
    if (pb_config_file_set(&file, "sip_host", config->sip_host) != 0
            || pb_config_file_set(&file, "sip_user", config->sip_user) != 0
            || pb_config_file_set(&file, "sip_pass", config->sip_pass) != 0
            || pb_config_file_set(&file, "phoneblock_base_url",
                      config->phoneblock_base_url) != 0
            || pb_config_file_set(&file, "phoneblock_token",
                      config->phoneblock_token) != 0) {
        return -1;
    }
    snprintf(number, sizeof(number), "%d", config->sip_port);
    if (pb_config_file_set(&file, "sip_port", number) != 0) return -1;
    snprintf(number, sizeof(number), "%d", config->sip_expires);
    if (pb_config_file_set(&file, "sip_expires", number) != 0) return -1;
    snprintf(number, sizeof(number), "%d", config->sip_local_port);
    if (pb_config_file_set(&file, "sip_local_port", number) != 0) return -1;
    snprintf(number, sizeof(number), "%d", config->rtp_port);
    if (pb_config_file_set(&file, "rtp_port", number) != 0) return -1;
    return pb_config_file_save(path, &file);
}