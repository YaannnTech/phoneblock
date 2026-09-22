#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "sip_dialog_linux.h"

int main(void)
{
    char sdp[512];
    int length = pb_linux_build_sdp_answer("192.0.2.10", 16000,
                                           sdp, sizeof(sdp));
    assert(length > 0);
    assert(strstr(sdp, "c=IN IP4 192.0.2.10\r\n") != NULL);
    assert(strstr(sdp, "m=audio 16000 RTP/AVP 8\r\n") != NULL);
    assert(strstr(sdp, "a=rtpmap:8 PCMA/8000\r\n") != NULL);
    assert(pb_linux_build_sdp_answer("192.0.2.10", 16000, sdp, 8) == -1);
    assert(pb_linux_call_id_matches("abc", "abc"));
    assert(!pb_linux_call_id_matches("abc", "def"));
    assert(!pb_linux_call_id_matches("", "abc"));
    puts("test_sip_dialog_linux: all tests passed");
    return 0;
}