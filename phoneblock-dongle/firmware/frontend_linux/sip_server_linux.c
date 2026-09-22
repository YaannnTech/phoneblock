#include "sip_server_linux.h"

#include "sip_parse.h"
#include "sip_response.h"
#include "sip_transport.h"

#include <stdio.h>
#include <string.h>

int pb_linux_sip_listen(const char *host, int port, const char *user,
                        int local_port, volatile sig_atomic_t *stop_requested)
{
    if (!host || !user || !stop_requested) return -1;
    sip_transport_t *transport = sip_transport_open("udp", host, port,
                                                     NULL, local_port);
    if (!transport) return -1;

    char packet[8192];
    char response[4096];
    struct sockaddr_in peer;
    while (!*stop_requested) {
        int length = sip_transport_recv(transport, 500, packet,
                                         sizeof(packet) - 1, &peer);
        if (length <= 0) continue;
        packet[length] = '\0';
        char method[16];
        parse_method(packet, length, method, sizeof(method));
        if (strcmp(method, "OPTIONS") != 0 && strcmp(method, "INVITE") != 0) {
            continue;
        }
        int response_length = sip_response_build(
            packet, length, 200, "OK", "linux", NULL, user,
            sip_transport_local_ip(transport),
            sip_transport_local_port(transport), response, sizeof(response));
        if (response_length > 0) {
            sip_transport_send_to(transport, &peer, response, response_length);
        }
    }
    sip_transport_close(transport);
    return 0;
}