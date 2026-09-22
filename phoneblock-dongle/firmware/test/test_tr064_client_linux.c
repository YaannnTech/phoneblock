#include <assert.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "tr064_client_linux.h"

static int server_socket;
static int server_result;

static void *server_thread(void *unused)
{
    (void)unused;
    int client = accept(server_socket, NULL, NULL);
    if (client < 0) return NULL;
    char request[4096] = { 0 };
    ssize_t length = recv(client, request, sizeof(request) - 1, 0);
    const char *action = strstr(request, "SOAPAction: \"urn:test:service#Ping\"");
    const char *body = strstr(request, "<u:Ping xmlns:u=\"urn:test:service\"><Value>ok</Value>");
    server_result = length > 0 && action && body;
    const char response[] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    send(client, response, sizeof(response) - 1, 0);
    close(client);
    return NULL;
}

int main(void)
{
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
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
    assert(listen(server_socket, 1) == 0);

    pthread_t thread;
    assert(pthread_create(&thread, NULL, server_thread, NULL) == 0);
    long status;
    char *response;
    size_t response_length;
    assert(pb_tr064_soap_post("127.0.0.1", ntohs(address.sin_port),
                              "urn:test:service", "Ping",
                              "<Value>ok</Value>", 1024, &status,
                              &response, &response_length) == 0);
    assert(status == 200);
    assert(response_length == 2);
    assert(strcmp(response, "ok") == 0);
    free(response);
    pthread_join(thread, NULL);
    assert(server_result);
    close(server_socket);
    puts("test_tr064_client_linux: all tests passed");
    return 0;
}