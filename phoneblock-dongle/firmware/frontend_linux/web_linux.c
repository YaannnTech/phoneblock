#include "web_linux.h"

#include "platform.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

static int send_response(int client, int status, const char *reason,
                         const char *content_type, const char *body)
{
    char response[2048];
    int body_length = (int)strlen(body);
    int length = snprintf(response, sizeof(response),
                          "HTTP/1.1 %d %s\r\n"
                          "Content-Type: %s\r\n"
                          "Content-Length: %d\r\n"
                          "Connection: close\r\n\r\n%s",
                          status, reason, content_type, body_length, body);
    if (length < 0 || (size_t)length >= sizeof(response)) return -1;
    return send(client, response, (size_t)length, 0) == length ? 0 : -1;
}

int pb_linux_web_serve(int port, const char *bind_host,
                       const char *sip_host, int sip_port,
                       volatile sig_atomic_t *stop_requested)
{
    if (!bind_host || !sip_host || !stop_requested || port <= 0 || port > 65535) {
        return -1;
    }
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) return -1;
    int reuse = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    struct sockaddr_in address = { .sin_family = AF_INET, .sin_port = htons(port) };
    if (inet_pton(AF_INET, bind_host, &address.sin_addr) != 1
            || bind(listener, (struct sockaddr *)&address, sizeof(address)) != 0
            || listen(listener, 8) != 0) {
        close(listener);
        return -1;
    }

    while (!*stop_requested) {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(listener, &readable);
        struct timeval timeout = { .tv_sec = 0, .tv_usec = 500000 };
        if (select(listener + 1, &readable, NULL, NULL, &timeout) <= 0) continue;
        int client = accept(listener, NULL, NULL);
        if (client < 0) continue;
        char request[1024] = { 0 };
        ssize_t length = recv(client, request, sizeof(request) - 1, 0);
        if (length <= 0) {
            close(client);
            continue;
        }
        if (strncmp(request, "GET /health ", 12) == 0) {
            send_response(client, 200, "OK", "text/plain; charset=utf-8", "ok\n");
        } else if (strncmp(request, "GET / ", 6) == 0
                   || strncmp(request, "GET /index.html ", 16) == 0) {
            const char *body =
                "<!doctype html><html><head><meta charset=\"utf-8\">"
                "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                "<title>PhoneBlock Dongle</title></head><body>"
                "<h1>PhoneBlock Dongle</h1>"
                "<p>SIP service is running.</p>"
                "<p><a href=\"/api/status\">View status</a></p>"
                "</body></html>\n";
            send_response(client, 200, "OK", "text/html; charset=utf-8", body);
        } else if (strncmp(request, "GET /api/status ", 16) == 0) {
            char body[512];
            snprintf(body, sizeof(body),
                     "{\"sipHost\":\"%s\",\"sipPort\":%d,\"service\":\"linux\"}\n",
                     sip_host, sip_port);
            send_response(client, 200, "OK", "application/json", body);
        } else {
            send_response(client, 404, "Not Found", "text/plain; charset=utf-8",
                          "not found\n");
        }
        close(client);
    }
    close(listener);
    return 0;
}