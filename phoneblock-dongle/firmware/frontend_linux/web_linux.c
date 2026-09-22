#include "web_linux.h"

#include "config_linux.h"
#include "platform.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

static int send_response(int client, int status, const char *reason,
                         const char *content_type, const char *body)
{
    char response[16384];
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

static void copy_form_string(const char *body, const char *key,
                             char *destination, size_t capacity);
static int form_int(const char *body, const char *key, int current);
static const char *dashboard_html(void);

int pb_linux_web_serve(int port, const char *bind_host,
                       const char *sip_host, int sip_port,
                       const char *config_path,
                       volatile sig_atomic_t *stop_requested)
{
    if (!bind_host || !sip_host || !config_path || !stop_requested
            || port <= 0 || port > 65535) {
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
        char request[8192] = { 0 };
        ssize_t length = recv(client, request, sizeof(request) - 1, 0);
        if (length <= 0) {
            close(client);
            continue;
        }
        if (strncmp(request, "GET /health ", 12) == 0) {
            send_response(client, 200, "OK", "text/plain; charset=utf-8", "ok\n");
        } else if (strncmp(request, "GET / ", 6) == 0
                   || strncmp(request, "GET /index.html ", 16) == 0) {
            send_response(client, 200, "OK", "text/html; charset=utf-8",
                          dashboard_html());
        } else if (strncmp(request, "GET /api/status ", 16) == 0) {
            char body[512];
            snprintf(body, sizeof(body),
                     "{\"sipHost\":\"%s\",\"sipPort\":%d,\"service\":\"linux\"}\n",
                     sip_host, sip_port);
            send_response(client, 200, "OK", "application/json", body);
        } else if (strncmp(request, "GET /api/config ", 16) == 0) {
            pb_linux_config_t config;
            if (pb_linux_config_load(config_path, &config) != 0) {
                send_response(client, 500, "Internal Server Error",
                              "text/plain; charset=utf-8", "config load failed\n");
            } else {
                char body[2048];
                snprintf(body, sizeof(body),
                         "{\"sip_host\":\"%s\",\"sip_port\":%d,\"sip_user\":\"%s\","
                         "\"sip_local_port\":%d,\"rtp_port\":%d,\"phoneblock_base_url\":\"%s\"}\n",
                         config.sip_host, config.sip_port, config.sip_user,
                         config.sip_local_port, config.rtp_port,
                         config.phoneblock_base_url);
                send_response(client, 200, "OK", "application/json", body);
            }
        } else if (strncmp(request, "POST /api/config ", 17) == 0) {
            char *body = strstr(request, "\r\n\r\n");
            pb_linux_config_t config;
            if (!body || pb_linux_config_load(config_path, &config) != 0) {
                send_response(client, 400, "Bad Request", "text/plain; charset=utf-8",
                              "invalid configuration request\n");
            } else {
                body += 4;
                copy_form_string(body, "sip_host", config.sip_host,
                                 sizeof(config.sip_host));
                copy_form_string(body, "sip_user", config.sip_user,
                                 sizeof(config.sip_user));
                copy_form_string(body, "sip_pass", config.sip_pass,
                                 sizeof(config.sip_pass));
                copy_form_string(body, "phoneblock_base_url", config.phoneblock_base_url,
                                 sizeof(config.phoneblock_base_url));
                copy_form_string(body, "phoneblock_token", config.phoneblock_token,
                                 sizeof(config.phoneblock_token));
                config.sip_port = form_int(body, "sip_port", config.sip_port);
                config.sip_local_port = form_int(body, "sip_local_port", config.sip_local_port);
                config.rtp_port = form_int(body, "rtp_port", config.rtp_port);
                if (pb_linux_config_save(config_path, &config) != 0) {
                    send_response(client, 500, "Internal Server Error",
                                  "text/plain; charset=utf-8", "config save failed\n");
                } else {
                    send_response(client, 200, "OK", "application/json",
                                  "{\"saved\":true}\n");
                }
            }
        } else {
            send_response(client, 404, "Not Found", "text/plain; charset=utf-8",
                          "not found\n");
        }
        close(client);
    }
    close(listener);
    return 0;
}

static int hex_digit(char value)
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

static int form_value(const char *body, const char *key,
                      char *out, size_t capacity)
{
    size_t key_length = strlen(key);
    const char *cursor = body;
    while (cursor && *cursor) {
        if ((cursor == body || cursor[-1] == '&')
                && strncmp(cursor, key, key_length) == 0
                && cursor[key_length] == '=') {
            const char *value = cursor + key_length + 1;
            size_t length = 0;
            size_t written = 0;
            while (value[length] && value[length] != '&') length++;
            for (size_t index = 0; index < length && written + 1 < capacity;
                 index++) {
                if (value[index] == '%' && index + 2 < length) {
                    int high = hex_digit(value[index + 1]);
                    int low = hex_digit(value[index + 2]);
                    if (high >= 0 && low >= 0) {
                        out[written++] = (char)((high << 4) | low);
                        index += 2;
                        continue;
                    }
                }
                out[written++] = value[index] == '+' ? ' ' : value[index];
            }
            out[written] = '\0';
            return 1;
        }
        cursor = strchr(cursor, '&');
        if (cursor) cursor++;
    }
    return 0;
}

static void copy_form_string(const char *body, const char *key,
                             char *destination, size_t capacity)
{
    char value[512];
    if (form_value(body, key, value, sizeof(value)))
        snprintf(destination, capacity, "%s", value);
}

static int form_int(const char *body, const char *key, int current)
{
    char value[32];
    if (!form_value(body, key, value, sizeof(value)) || !value[0]) return current;
    return atoi(value);
}

static const char *dashboard_html(void)
{
    return "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>PhoneBlock Dongle</title><style>"
        ":root{font-family:system-ui,sans-serif;color:#20252b;background:#eef2f1}"
        "body{max-width:900px;margin:0 auto;padding:24px}header{background:#087f70;"
        "color:#fff;padding:22px 26px;border-radius:10px;margin-bottom:18px}"
        "section{background:#fff;padding:20px 24px;border-radius:10px;margin-bottom:18px;"
        "box-shadow:0 2px 8px #0001}h1{margin:0 0 4px}h2{font-size:1rem;"
        "text-transform:uppercase;color:#63716f;letter-spacing:.08em}form{display:grid;"
        "grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:14px}label{display:grid;"
        "gap:5px;color:#53615e;font-size:.9rem}input{padding:9px;border:1px solid #bbc7c4;"
        "border-radius:5px;font:inherit}button{background:#087f70;color:#fff;border:0;"
        "border-radius:5px;padding:10px 16px;font:inherit;cursor:pointer}"
        ".wide{grid-column:1/-1}.muted{color:#697572}.ok{color:#16734f}.error{color:#a22}"
        "</style></head><body><header><h1>PhoneBlock Dongle</h1>"
        "<div>SIP call screening service</div></header>"
        "<section><h2>Service status</h2><p id=\"status\" class=\"muted\">Loading...</p>"
        "<p><a href=\"/api/status\">View raw status</a></p></section>"
        "<section><h2>Configuration</h2><form id=\"form\">"
        "<label>Registrar host<input name=\"sip_host\" required></label>"
        "<label>Registrar port<input name=\"sip_port\" type=\"number\" min=\"1\" max=\"65535\"></label>"
        "<label>SIP username<input name=\"sip_user\" required></label>"
        "<label>SIP password<input name=\"sip_pass\" type=\"password\" placeholder=\"unchanged\"></label>"
        "<label>Local SIP port<input name=\"sip_local_port\" type=\"number\"></label>"
        "<label>RTP port<input name=\"rtp_port\" type=\"number\"></label>"
        "<label class=\"wide\">PhoneBlock API URL<input name=\"phoneblock_base_url\"></label>"
        "<label class=\"wide\">PhoneBlock token<input name=\"phoneblock_token\" type=\"password\" placeholder=\"unchanged\"></label>"
        "<div class=\"wide\"><button type=\"submit\">Save configuration</button>"
        "<span id=\"message\" class=\"muted\"></span></div></form></section>"
        "<p class=\"muted\">Restart the add-on after changing SIP settings.</p>"
        "<script>const q=s=>document.querySelector(s);async function load(){"
        "const c=await fetch('/api/config').then(r=>r.json());for(const [k,v] of Object.entries(c)){"
        "const e=q('[name=\\\"'+k+'\\\"]');if(e)e.value=v||'';}const s=await fetch('/api/status').then(r=>r.json());"
        "q('#status').textContent='SIP registrar '+s.sipHost+':'+s.sipPort+' | '+s.service;}"
        "q('#form').onsubmit=async e=>{e.preventDefault();const r=await fetch('/api/config',{method:'POST',"
        "headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(new FormData(e.target))});"
        "const m=q('#message');m.textContent=r.ok?'Saved. Restart the add-on to apply changes.':'Save failed';"
        "m.className=r.ok?'ok':'error';};load().catch(()=>q('#status').textContent='Unable to load status');</script>"
        "</body></html>";
}