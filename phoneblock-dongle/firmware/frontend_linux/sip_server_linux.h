#pragma once

#include <signal.h>

int pb_linux_sip_listen(const char *host, int port, const char *user,
                        const char *password,
                        const char *auth_user, const char *realm,
                        int local_port, const char *phoneblock_base_url,
                        const char *phoneblock_token,
                        const char *announcement_path, int rtp_port,
                        volatile sig_atomic_t *stop_requested);