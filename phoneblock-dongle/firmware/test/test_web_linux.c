#include <assert.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "web_linux.h"

static volatile sig_atomic_t stop_requested;
static int port;

static void *server_thread(void *unused)
{
    (void)unused;
    pb_linux_web_serve(port, "127.0.0.1", "fritz.box", 5060,
                       &stop_requested);
    return NULL;
}

static void request_path(const char *path, char *response, size_t capacity)
{
    int client = socket(AF_INET, SOCK_STREAM, 0);
    assert(client >= 0);
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
        .sin_port = htons((uint16_t)port),
    };
    assert(connect(client, (struct sockaddr *)&address, sizeof(address)) == 0);
    int request_length = snprintf(response, capacity, "GET %s HTTP/1.1\r\n\r\n", path);
    assert(send(client, response, (size_t)request_length, 0) == request_length);
    ssize_t length = recv(client, response, capacity - 1, 0);
    assert(length > 0);
    response[length] = '\0';
    close(client);
}

int main(void)
{
    int probe = socket(AF_INET, SOCK_STREAM, 0);
    assert(probe >= 0);
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
        .sin_port = 0,
    };
    assert(bind(probe, (struct sockaddr *)&address, sizeof(address)) == 0);
    socklen_t address_length = sizeof(address);
    assert(getsockname(probe, (struct sockaddr *)&address, &address_length) == 0);
    port = ntohs(address.sin_port);
    close(probe);

    pthread_t thread;
    assert(pthread_create(&thread, NULL, server_thread, NULL) == 0);
    char response[2048];
    request_path("/health", response, sizeof(response));
    assert(strstr(response, "200 OK") != NULL);
    assert(strstr(response, "ok\n") != NULL);
    request_path("/", response, sizeof(response));
    assert(strstr(response, "200 OK") != NULL);
    assert(strstr(response, "PhoneBlock Dongle") != NULL);
    request_path("/api/status", response, sizeof(response));
    assert(strstr(response, "\"sipHost\":\"fritz.box\"") != NULL);
    request_path("/missing", response, sizeof(response));
    assert(strstr(response, "404 Not Found") != NULL);
    stop_requested = 1;
    pthread_join(thread, NULL);
    puts("test_web_linux: all tests passed");
    return 0;
}