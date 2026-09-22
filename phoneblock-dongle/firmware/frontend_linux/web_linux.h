#pragma once

#include <signal.h>

int pb_linux_web_serve(int port, const char *bind_host,
                       const char *sip_host, int sip_port,
                       volatile sig_atomic_t *stop_requested);