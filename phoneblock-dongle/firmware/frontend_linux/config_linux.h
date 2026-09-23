#pragma once

#include <stddef.h>

typedef struct {
    char sip_host[64];
    int sip_port;
    char sip_user[32];
    char sip_pass[64];
    char sip_authuser[32];
    char sip_realm[64];
    int sip_expires;
    int sip_local_port;
    int rtp_port;
    // Overrides for the address advertised to the registrar (REGISTER
    // Contact, INVITE response Contact, SDP media IP) when the discovered
    // local IP isn't reachable from it directly -- e.g. the dongle sits
    // behind a second router routed to the Fritz!Box, so it must advertise
    // that router's forwarded address/port instead of its own LAN address.
    // Empty/0 (the default) keeps today's behavior of advertising the
    // transport's own discovered local IP/port.
    char contact_host[64];
    int contact_port;
    char phoneblock_base_url[128];
    char phoneblock_token[128];
    char announcement_path[256];
    char fritzbox_host[128];
    int fritzbox_port;
    char fritzbox_admin_user[64];
    char fritzbox_admin_pass[128];
    char fritzbox_phone_name[64];
} pb_linux_config_t;

void pb_linux_config_defaults(pb_linux_config_t *config);

// Load known settings from an INI file. Missing files are treated as an
// empty configuration and leave defaults in place.
int pb_linux_config_load(const char *path, pb_linux_config_t *config);

// Update the known settings while preserving unrelated keys in the file.
int pb_linux_config_save(const char *path, const pb_linux_config_t *config);