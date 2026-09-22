#include "sip_register_linux.h"

#include "platform.h"
#include "sip_auth.h"
#include "sip_parse.h"
#include "sip_transport.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int build_register(const sip_transport_t *transport, const char *host,
                          const char *user, char *out, size_t capacity)
{
    uint32_t random_value = pb_random_u32();
    return snprintf(out, capacity,
        "REGISTER sip:%s SIP/2.0\r\n"
        "Via: SIP/2.0/UDP %s:%d;branch=z9hG4bK%08x\r\n"
        "Max-Forwards: 70\r\n"
        "From: <sip:%s@%s>;tag=%08x\r\n"
        "To: <sip:%s@%s>\r\n"
        "Call-ID: %08x@%s\r\n"
        "CSeq: 1 REGISTER\r\n"
        "Contact: <sip:%s@%s:%d>\r\n"
        "Expires: 3600\r\n"
        "User-Agent: PhoneBlock-Dongle/Linux\r\n"
        "Content-Length: 0\r\n\r\n",
        host, sip_transport_local_ip(transport), sip_transport_local_port(transport),
        random_value, user, host, random_value ^ 0x13579bdfu,
        user, host, random_value, host, user,
        sip_transport_local_ip(transport), sip_transport_local_port(transport));
}

int pb_linux_sip_register_probe(const char *host, int port,
                                const char *user, int local_port,
                                int *status, char *challenge,
                                int challenge_cap)
{
    if (!host || !user || !status || !challenge || challenge_cap <= 0) return -1;
    *status = 0;
    challenge[0] = '\0';
    sip_transport_t *transport = sip_transport_open("udp", host, port,
                                                     NULL, local_port);
    if (!transport) return -1;

    char request[2048];
    int request_length = build_register(transport, host, user,
                                        request, sizeof(request));
    if (request_length < 0 || (size_t)request_length >= sizeof(request)
            || sip_transport_send(transport, request, request_length) < 0) {
        sip_transport_close(transport);
        return -1;
    }

    char response[4096];
    struct sockaddr_in from;
    int response_length = sip_transport_recv(transport, 3000, response,
                                              sizeof(response) - 1, &from);
    sip_transport_close(transport);
    if (response_length <= 0) return -1;
    response[response_length] = '\0';
    *status = parse_status_code(response, response_length);
    if (*status == 401 || *status == 407) {
        const char *header = find_header(response, response_length,
                                         *status == 401
                                             ? "WWW-Authenticate"
                                             : "Proxy-Authenticate");
        if (header) {
            const char *end = response + response_length;
            header_value(header, end, challenge, challenge_cap);
        }
    }
    return 0;
}