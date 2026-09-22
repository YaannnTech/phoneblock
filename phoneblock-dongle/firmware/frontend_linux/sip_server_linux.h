#pragma once

#include <signal.h>

int pb_linux_sip_listen(const char *host, int port, const char *user,
                        int local_port, volatile sig_atomic_t *stop_requested);