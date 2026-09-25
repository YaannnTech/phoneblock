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
    copy_value(config->announcement_path, sizeof(config->announcement_path),
               "/usr/share/phoneblock/announcement.alaw");
    copy_value(config->announcement_custom_path,
               sizeof(config->announcement_custom_path),
               "/var/lib/phoneblock/announcement.alaw");
    config->announcement_enabled = 1;
    copy_value(config->fritzbox_host, sizeof(config->fritzbox_host), "fritz.box");
    config->fritzbox_port = 49000;
    copy_value(config->fritzbox_phone_name, sizeof(config->fritzbox_phone_name),
               "PhoneBlock");
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
    if (pb_config_file_get(&file, "sip_authuser", value, sizeof(value)) == 1) {
        copy_value(config->sip_authuser, sizeof(config->sip_authuser), value);
    }
    if (pb_config_file_get(&file, "sip_realm", value, sizeof(value)) == 1) {
        copy_value(config->sip_realm, sizeof(config->sip_realm), value);
    }
    if (pb_config_file_get(&file, "contact_host", value, sizeof(value)) == 1) {
        copy_value(config->contact_host, sizeof(config->contact_host), value);
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
    if (pb_config_file_get(&file, "announcement_path", value,
                           sizeof(value)) == 1) {
        copy_value(config->announcement_path,
                   sizeof(config->announcement_path), value);
    }
    if (pb_config_file_get(&file, "announcement_custom_path", value,
                           sizeof(value)) == 1) {
        copy_value(config->announcement_custom_path,
                   sizeof(config->announcement_custom_path), value);
    }
    if (pb_config_file_get(&file, "fritzbox_host", value, sizeof(value)) == 1)
        copy_value(config->fritzbox_host, sizeof(config->fritzbox_host), value);
    if (pb_config_file_get(&file, "fritzbox_admin_user", value, sizeof(value)) == 1)
        copy_value(config->fritzbox_admin_user, sizeof(config->fritzbox_admin_user), value);
    if (pb_config_file_get(&file, "fritzbox_admin_pass", value, sizeof(value)) == 1)
        copy_value(config->fritzbox_admin_pass, sizeof(config->fritzbox_admin_pass), value);
    if (pb_config_file_get(&file, "fritzbox_phone_name", value, sizeof(value)) == 1)
        copy_value(config->fritzbox_phone_name, sizeof(config->fritzbox_phone_name), value);
    config->sip_port = pb_config_file_get_int(&file, "sip_port",
                                               config->sip_port, 1, 65535);
    config->sip_expires = pb_config_file_get_int(&file, "sip_expires",
                                                  config->sip_expires, 60, 86400);
    config->sip_local_port = pb_config_file_get_int(&file, "sip_local_port",
                                                     config->sip_local_port,
                                                     1, 65535);
    config->rtp_port = pb_config_file_get_int(&file, "rtp_port",
                                               config->rtp_port, 1, 65535);
    config->contact_port = pb_config_file_get_int(&file, "contact_port",
                                                   config->contact_port, 0, 65535);
    config->announcement_enabled = pb_config_file_get_int(
        &file, "announcement_enabled", config->announcement_enabled, 0, 1);
    config->fritzbox_port = pb_config_file_get_int(&file, "fritzbox_port",
                                                    config->fritzbox_port, 1, 65535);
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
            || pb_config_file_set(&file, "sip_authuser", config->sip_authuser) != 0
            || pb_config_file_set(&file, "sip_realm", config->sip_realm) != 0
            || pb_config_file_set(&file, "contact_host", config->contact_host) != 0
            || pb_config_file_set(&file, "phoneblock_base_url",
                      config->phoneblock_base_url) != 0
            || pb_config_file_set(&file, "phoneblock_token",
                                  config->phoneblock_token) != 0
            || pb_config_file_set(&file, "announcement_path",
                                  config->announcement_path) != 0
            || pb_config_file_set(&file, "announcement_custom_path",
                                  config->announcement_custom_path) != 0
            || pb_config_file_set(&file, "fritzbox_host", config->fritzbox_host) != 0
            || pb_config_file_set(&file, "fritzbox_admin_user", config->fritzbox_admin_user) != 0
            || pb_config_file_set(&file, "fritzbox_admin_pass", config->fritzbox_admin_pass) != 0
            || pb_config_file_set(&file, "fritzbox_phone_name", config->fritzbox_phone_name) != 0) {
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
    snprintf(number, sizeof(number), "%d", config->contact_port);
    if (pb_config_file_set(&file, "contact_port", number) != 0) return -1;
    snprintf(number, sizeof(number), "%d", config->announcement_enabled);
    if (pb_config_file_set(&file, "announcement_enabled", number) != 0) return -1;
    snprintf(number, sizeof(number), "%d", config->fritzbox_port);
    if (pb_config_file_set(&file, "fritzbox_port", number) != 0) return -1;
    return pb_config_file_save(path, &file);
}