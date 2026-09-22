#pragma once

#include <signal.h>

// Stream a raw 8 kHz mono G.711 A-law file as RTP payload type 8. The file is
// sent in 20 ms frames (160 bytes). Returns 0 on success, -1 on I/O/socket
// failure, and stops early when stop_requested becomes non-zero.
int pb_linux_rtp_stream_alaw(const char *destination_host, int destination_port,
                             const char *audio_path, int local_port,
                             volatile sig_atomic_t *stop_requested);