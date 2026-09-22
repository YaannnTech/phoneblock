#pragma once

#include <stddef.h>

typedef struct {
    char sip_host[64];
    int sip_port;
    char sip_user[32];
    char sip_pass[64];
    int sip_expires;
    int sip_local_port;
    int rtp_port;
    char phoneblock_base_url[128];
    char phoneblock_token[128];
} pb_linux_config_t;

void pb_linux_config_defaults(pb_linux_config_t *config);

// Load known settings from an INI file. Missing files are treated as an
// empty configuration and leave defaults in place.
int pb_linux_config_load(const char *path, pb_linux_config_t *config);

// Update the known settings while preserving unrelated keys in the file.
int pb_linux_config_save(const char *path, const pb_linux_config_t *config);