#include <assert.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "sip_transport.h"

int main(void)
{
    int server = socket(AF_INET, SOCK_DGRAM, 0);
    assert(server >= 0);
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
        .sin_port = 0,
    };
    assert(bind(server, (struct sockaddr *)&address, sizeof(address)) == 0);
    socklen_t address_length = sizeof(address);
    assert(getsockname(server, (struct sockaddr *)&address, &address_length) == 0);

    sip_transport_t *transport = sip_transport_open(
        "udp", "127.0.0.1", ntohs(address.sin_port), NULL, 0);
    assert(transport != NULL);
    assert(sip_transport_local_port(transport) != 0);

    const char message[] = "SIP ping";
    assert(sip_transport_send(transport, message, sizeof(message))
           == (int)sizeof(message));
    char received[32];
    assert(recv(server, received, sizeof(received), 0) == (ssize_t)sizeof(message));
    assert(memcmp(received, message, sizeof(message)) == 0);

    sip_transport_close(transport);
    close(server);
    puts("test_sip_transport_linux: all tests passed");
    return 0;
}