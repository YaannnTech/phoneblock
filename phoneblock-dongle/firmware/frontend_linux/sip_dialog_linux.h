#pragma once

#include <stddef.h>

int pb_linux_build_sdp_answer(const char *local_ip, int rtp_port,
                              char *out, size_t capacity);

int pb_linux_call_id_matches(const char *expected, const char *actual);