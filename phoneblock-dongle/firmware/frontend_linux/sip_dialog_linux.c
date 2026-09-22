#include "sip_dialog_linux.h"

#include <stdio.h>
#include <string.h>

int pb_linux_build_sdp_answer(const char *local_ip, int rtp_port,
                              char *out, size_t capacity)
{
    if (!local_ip || !out || capacity == 0 || rtp_port <= 0 || rtp_port > 65535) {
        return -1;
    }
    int length = snprintf(out, capacity,
        "v=0\r\n"
        "o=- 0 0 IN IP4 %s\r\n"
        "s=PhoneBlock\r\n"
        "c=IN IP4 %s\r\n"
        "t=0 0\r\n"
        "m=audio %d RTP/AVP 8\r\n"
        "a=rtpmap:8 PCMA/8000\r\n",
        local_ip, local_ip, rtp_port);
    return length >= 0 && (size_t)length < capacity ? length : -1;
}

int pb_linux_call_id_matches(const char *expected, const char *actual)
{
    return expected && actual && expected[0] && actual[0]
        && strcmp(expected, actual) == 0;
}