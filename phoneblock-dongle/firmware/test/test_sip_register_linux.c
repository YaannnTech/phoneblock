#include <assert.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "sip_register_linux.h"

static int server_socket;
static int server_result;

static void *server_thread(void *unused)
{
    (void)unused;
    char request[4096] = { 0 };
    struct sockaddr_in peer;
    socklen_t peer_length = sizeof(peer);
    ssize_t length = recvfrom(server_socket, request, sizeof(request) - 1, 0,
                              (struct sockaddr *)&peer, &peer_length);
    server_result = length > 0 && strstr(request, "REGISTER sip:127.0.0.1")
        && strstr(request, "Content-Length: 0");
    const char response[] =
        "SIP/2.0 401 Unauthorized\r\n"
        "WWW-Authenticate: Digest realm=\"fritz.box\", nonce=\"abc123\"\r\n"
        "Content-Length: 0\r\n\r\n";
        sendto(server_socket, response, sizeof(response) - 1, 0,
            (struct sockaddr *)&peer, peer_length);
    return NULL;
}

int main(void)
{
    server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    assert(server_socket >= 0);
    int reuse = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
        .sin_port = 0,
    };
    assert(bind(server_socket, (struct sockaddr *)&address, sizeof(address)) == 0);
    socklen_t address_length = sizeof(address);
    assert(getsockname(server_socket, (struct sockaddr *)&address, &address_length) == 0);
    pthread_t thread;
    assert(pthread_create(&thread, NULL, server_thread, NULL) == 0);
    int status;
    char challenge[256];
    assert(pb_linux_sip_register_probe("127.0.0.1", ntohs(address.sin_port),
                                       "620", 0, &status, challenge,
                                       sizeof(challenge)) == 0);
    assert(status == 401);
    assert(strstr(challenge, "realm=\"fritz.box\"") != NULL);
    pthread_join(thread, NULL);
    assert(server_result);
    close(server_socket);
    puts("test_sip_register_linux: all tests passed");
    return 0;
}