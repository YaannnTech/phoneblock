#pragma once

#include <stddef.h>

typedef struct {
    char sip_user[32];
    char sip_pass[48];
    char internal_number[16];
} pb_tr064_sip_credentials_t;

// Provision or overwrite a PhoneBlock IP-phone entry on a Fritz!Box.
// This first Linux implementation supports the normal non-2FA flow.
// Returns 0 on success and -1 on transport, authentication, or SOAP errors.
int pb_tr064_provision_sip(const char *host, int port,
                           const char *admin_user, const char *admin_pass,
                           const char *phone_name,
                           pb_tr064_sip_credentials_t *out);