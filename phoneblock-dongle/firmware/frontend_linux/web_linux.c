#include "web_linux.h"

#include "config_linux.h"
#include "platform.h"
#include "sip_state_linux.h"
#include "sip_stats_linux.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define ANNOUNCEMENT_MAX_BYTES (240U * 1024U)
#define ANNOUNCEMENT_PRESET_DIR "/usr/share/phoneblock/announcements"

typedef struct {
    const char *code;
    const char *label;
} announcement_preset_t;

static const announcement_preset_t ANNOUNCEMENT_PRESETS[] = {
    { "ar", "Arabic" },
    { "de", "German" },
    { "en", "English" },
    { "es", "Spanish" },
    { "fr", "French" },
    { "it", "Italian" },
    { "uk", "Ukrainian" },
    { "zh", "Chinese" },
};

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
static int form_value(const char *body, const char *key,
                      char *out, size_t capacity);
static int form_int(const char *body, const char *key, int current);
static const char *dashboard_html(void);

static int upload_announcement(int client, const char *request,
                               ssize_t request_length, const char *path)
{
    const char *header_end = strstr(request, "\r\n\r\n");
    const char *content_length_header = strstr(request, "Content-Length:");
    if (!header_end || !content_length_header
            || content_length_header > header_end) {
        return send_response(client, 400, "Bad Request",
                             "text/plain; charset=utf-8",
                             "Content-Length is required\n");
    }

    unsigned long content_length = strtoul(content_length_header + 15, NULL, 10);
    if (content_length == 0 || content_length > ANNOUNCEMENT_MAX_BYTES) {
        return send_response(client, 400, "Bad Request",
                             "text/plain; charset=utf-8",
                             "Announcement is empty or too large\n");
    }

    char temporary_path[512];
    int path_length = snprintf(temporary_path, sizeof(temporary_path),
                               "%s.tmp", path);
    if (path_length < 0 || (size_t)path_length >= sizeof(temporary_path)) {
        return send_response(client, 400, "Bad Request",
                             "text/plain; charset=utf-8", "Path is too long\n");
    }

    FILE *output = fopen(temporary_path, "wb");
    if (!output) {
        return send_response(client, 500, "Internal Server Error",
                             "text/plain; charset=utf-8",
                             "Could not open announcement file\n");
    }

    size_t header_length = (size_t)(header_end + 4 - request);
    size_t received = request_length > (ssize_t)header_length
        ? (size_t)request_length - header_length : 0;
    if (received > content_length) received = content_length;
    if (received > 0
            && fwrite(request + header_length, 1, received, output) != received) {
        fclose(output);
        unlink(temporary_path);
        return send_response(client, 500, "Internal Server Error",
                             "text/plain; charset=utf-8", "Write failed\n");
    }

    char buffer[8192];
    while (received < content_length) {
        size_t wanted = content_length - received;
        if (wanted > sizeof(buffer)) wanted = sizeof(buffer);
        ssize_t count = recv(client, buffer, wanted, 0);
        if (count <= 0 || fwrite(buffer, 1, (size_t)count, output) != (size_t)count) {
            fclose(output);
            unlink(temporary_path);
            return send_response(client, 400, "Bad Request",
                                 "text/plain; charset=utf-8",
                                 "Upload interrupted\n");
        }
        received += (size_t)count;
    }

    if (fclose(output) != 0 || rename(temporary_path, path) != 0) {
        unlink(temporary_path);
        return send_response(client, 500, "Internal Server Error",
                             "text/plain; charset=utf-8", "Could not save announcement\n");
    }
    return send_response(client, 200, "OK", "application/json",
                         "{\"saved\":true}\n");
}

static const announcement_preset_t *find_announcement_preset(const char *code)
{
    for (size_t index = 0;
         index < sizeof(ANNOUNCEMENT_PRESETS) / sizeof(ANNOUNCEMENT_PRESETS[0]);
         index++) {
        if (strcmp(code, ANNOUNCEMENT_PRESETS[index].code) == 0)
            return &ANNOUNCEMENT_PRESETS[index];
    }
    return NULL;
}

static int announcement_preset_path(const char *code, char *path, size_t capacity)
{
    if (!find_announcement_preset(code)) return -1;
    int length = snprintf(path, capacity, "%s/announcement-%s.alaw",
                          ANNOUNCEMENT_PRESET_DIR, code);
    return length < 0 || (size_t)length >= capacity ? -1 : 0;
}

static const char *announcement_preset_for_path(const char *path)
{
    static char preset_path[256];
    for (size_t index = 0;
         index < sizeof(ANNOUNCEMENT_PRESETS) / sizeof(ANNOUNCEMENT_PRESETS[0]);
         index++) {
        if (announcement_preset_path(ANNOUNCEMENT_PRESETS[index].code,
                                     preset_path, sizeof(preset_path)) == 0
                && strcmp(path, preset_path) == 0) {
            return ANNOUNCEMENT_PRESETS[index].code;
        }
    }
    return "custom";
}

static int send_file_response(int client, const char *path,
                              const char *content_type)
{
    FILE *input = fopen(path, "rb");
    if (!input) {
        return send_response(client, 404, "Not Found",
                             "text/plain; charset=utf-8", "Audio not found\n");
    }
    if (fseek(input, 0, SEEK_END) != 0) {
        fclose(input);
        return -1;
    }
    long file_length = ftell(input);
    if (file_length < 0 || fseek(input, 0, SEEK_SET) != 0) {
        fclose(input);
        return -1;
    }
    char header[256];
    int header_length = snprintf(header, sizeof(header),
                                 "HTTP/1.1 200 OK\r\n"
                                 "Content-Type: %s\r\n"
                                 "Content-Length: %ld\r\n"
                                 "Connection: close\r\n\r\n",
                                 content_type, file_length);
    if (header_length < 0 || (size_t)header_length >= sizeof(header)
            || send(client, header, (size_t)header_length, 0) != header_length) {
        fclose(input);
        return -1;
    }
    char buffer[8192];
    size_t count;
    while ((count = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        if (send(client, buffer, count, 0) != (ssize_t)count) {
            fclose(input);
            return -1;
        }
    }
    fclose(input);
    return 0;
}

static int query_value(const char *request, const char *key,
                       char *value, size_t capacity)
{
    const char *start = strstr(request, key);
    if (!start) return 0;
    start += strlen(key);
    size_t length = 0;
    while (start[length] && start[length] != '&' && start[length] != ' '
            && length + 1 < capacity) length++;
    memcpy(value, start, length);
    value[length] = '\0';
    return length > 0;
}

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
            pb_linux_config_t config;
            pb_linux_sip_stats_t stats;
            pb_linux_sip_stats_read(&stats);
            if (pb_linux_config_load(config_path, &config) != 0) {
                send_response(client, 500, "Internal Server Error",
                              "text/plain; charset=utf-8", "config load failed\n");
                close(client);
                continue;
            }
            char body[1024];
            snprintf(body, sizeof(body),
                     "{\"sipHost\":\"%s\",\"sipPort\":%d,\"service\":\"linux\","
                     "\"configured\":%s,\"registered\":%s,\"sipUser\":\"%s\",\"sipPassSet\":%s,"
                     "\"phoneblockTokenSet\":%s,\"localSipPort\":%d,\"rtpPort\":%d,"
                     "\"phoneblockBaseUrl\":\"%s\","
                     "\"calls\":%llu,\"spamBlocked\":%llu,"
                     "\"callsPassed\":%llu,\"classificationErrors\":%llu}\n",
                     sip_host, sip_port,
                     (config.sip_host[0] && config.sip_user[0]) ? "true" : "false",
                     pb_linux_sip_state_is_registered() ? "true" : "false",
                     config.sip_user, config.sip_pass[0] ? "true" : "false",
                     config.phoneblock_token[0] ? "true" : "false",
                     config.sip_local_port, config.rtp_port,
                     config.phoneblock_base_url,
                     (unsigned long long)stats.calls,
                     (unsigned long long)stats.spam_blocked,
                     (unsigned long long)stats.calls_passed,
                     (unsigned long long)stats.classification_errors);
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
                         "\"sip_local_port\":%d,\"rtp_port\":%d,\"phoneblock_base_url\":\"%s\","
                         "\"contact_host\":\"%s\",\"contact_port\":%d,"
                         "\"announcement_path\":\"%s\",\"announcement_enabled\":%d,"
                         "\"announcement_preset\":\"%s\"}\n",
                         config.sip_host, config.sip_port, config.sip_user,
                         config.sip_local_port, config.rtp_port,
                         config.phoneblock_base_url,
                         config.contact_host, config.contact_port,
                         config.announcement_path, config.announcement_enabled,
                         announcement_preset_for_path(config.announcement_path));
                send_response(client, 200, "OK", "application/json", body);
            }
        } else if (strncmp(request, "GET /api/announcement/catalog ", 30) == 0) {
            send_response(client, 200, "OK", "application/json",
                          "{\"presets\":["
                          "{\"code\":\"ar\",\"label\":\"Arabic\"},"
                          "{\"code\":\"de\",\"label\":\"German\"},"
                          "{\"code\":\"en\",\"label\":\"English\"},"
                          "{\"code\":\"es\",\"label\":\"Spanish\"},"
                          "{\"code\":\"fr\",\"label\":\"French\"},"
                          "{\"code\":\"it\",\"label\":\"Italian\"},"
                          "{\"code\":\"uk\",\"label\":\"Ukrainian\"},"
                          "{\"code\":\"zh\",\"label\":\"Chinese\"}]}\n");
        } else if (strncmp(request, "GET /api/announcement/preset?", 29) == 0) {
            char code[8];
            char path[256];
            if (!query_value(request, "name=", code, sizeof(code))
                    || announcement_preset_path(code, path, sizeof(path)) != 0) {
                send_response(client, 400, "Bad Request",
                              "text/plain; charset=utf-8", "Unknown preset\n");
            } else {
                send_file_response(client, path, "application/octet-stream");
            }
        } else if (strncmp(request, "POST /api/announcement/select ", 30) == 0) {
            char *body = strstr(request, "\r\n\r\n");
            pb_linux_config_t config;
            char code[8];
            char preset_path[256];
            if (!body || pb_linux_config_load(config_path, &config) != 0) {
                send_response(client, 400, "Bad Request",
                              "text/plain; charset=utf-8", "Invalid request\n");
            } else {
                body += 4;
                if (!form_value(body, "name", code, sizeof(code))
                        || announcement_preset_path(code, preset_path,
                                                    sizeof(preset_path)) != 0) {
                    send_response(client, 400, "Bad Request",
                                  "text/plain; charset=utf-8",
                                  "Could not select preset\n");
                } else {
                    snprintf(config.announcement_path,
                             sizeof(config.announcement_path), "%s", preset_path);
                    config.announcement_enabled = 1;
                    if (pb_linux_config_save(config_path, &config) != 0) {
                        send_response(client, 500, "Internal Server Error",
                                      "text/plain; charset=utf-8",
                                      "Could not save announcement setting\n");
                    } else {
                        send_response(client, 200, "OK", "application/json",
                                      "{\"saved\":true}\n");
                    }
                }
            }
        } else if (strncmp(request, "POST /api/announcement/custom ", 30) == 0) {
            pb_linux_config_t config;
            if (pb_linux_config_load(config_path, &config) != 0
                    || access(config.announcement_custom_path, R_OK) != 0) {
                send_response(client, 400, "Bad Request",
                              "text/plain; charset=utf-8",
                              "No custom announcement is available\n");
            } else {
                snprintf(config.announcement_path,
                         sizeof(config.announcement_path), "%s",
                         config.announcement_custom_path);
                config.announcement_enabled = 1;
                if (pb_linux_config_save(config_path, &config) != 0) {
                    send_response(client, 500, "Internal Server Error",
                                  "text/plain; charset=utf-8",
                                  "Could not save announcement setting\n");
                } else {
                    send_response(client, 200, "OK", "application/json",
                                  "{\"saved\":true}\n");
                }
            }
        } else if (strncmp(request, "POST /api/announcement ", 23) == 0) {
            pb_linux_config_t config;
            if (pb_linux_config_load(config_path, &config) != 0
                    || !config.announcement_custom_path[0]) {
                send_response(client, 500, "Internal Server Error",
                              "text/plain; charset=utf-8",
                              "Custom announcement path is not configured\n");
            } else {
                int result = upload_announcement(client, request, length,
                                                 config.announcement_custom_path);
                if (result == 0) {
                    snprintf(config.announcement_path,
                             sizeof(config.announcement_path), "%s",
                             config.announcement_custom_path);
                    config.announcement_enabled = 1;
                    pb_linux_config_save(config_path, &config);
                }
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
                char new_password[128] = "";
                char new_token[256] = "";
                copy_form_string(body, "sip_pass", new_password,
                                 sizeof(new_password));
                copy_form_string(body, "phoneblock_base_url", config.phoneblock_base_url,
                                 sizeof(config.phoneblock_base_url));
                copy_form_string(body, "phoneblock_token", new_token,
                                 sizeof(new_token));
                copy_form_string(body, "contact_host", config.contact_host,
                                 sizeof(config.contact_host));
                copy_form_string(body, "announcement_path", config.announcement_path,
                                 sizeof(config.announcement_path));
                config.announcement_enabled = form_int(
                    body, "announcement_enabled", config.announcement_enabled);
                config.sip_port = form_int(body, "sip_port", config.sip_port);
                config.sip_local_port = form_int(body, "sip_local_port", config.sip_local_port);
                config.rtp_port = form_int(body, "rtp_port", config.rtp_port);
                config.contact_port = form_int(body, "contact_port", config.contact_port);
                if (new_password[0]) {
                    strncpy(config.sip_pass, new_password, sizeof(config.sip_pass) - 1);
                    config.sip_pass[sizeof(config.sip_pass) - 1] = '\0';
                }
                if (new_token[0]) {
                    strncpy(config.phoneblock_token, new_token,
                            sizeof(config.phoneblock_token) - 1);
                    config.phoneblock_token[sizeof(config.phoneblock_token) - 1] = '\0';
                }
                const char *error = NULL;
                if (!config.sip_host[0]) error = "SIP registrar host is required";
                else if (!config.sip_user[0]) error = "SIP username is required";
                else if (config.sip_port < 1 || config.sip_port > 65535)
                    error = "SIP registrar port must be between 1 and 65535";
                else if (config.sip_local_port < 1 || config.sip_local_port > 65535)
                    error = "Local SIP port must be between 1 and 65535";
                else if (config.rtp_port < 1 || config.rtp_port > 65535)
                    error = "RTP port must be between 1 and 65535";
                else if (config.contact_port < 0 || config.contact_port > 65535)
                    error = "Advertised port must be between 0 and 65535";
                else if (strncmp(config.phoneblock_base_url, "http://", 7) != 0
                         && strncmp(config.phoneblock_base_url, "https://", 8) != 0)
                    error = "PhoneBlock API URL must start with http:// or https://";
                if (error) {
                    send_response(client, 400, "Bad Request", "text/plain; charset=utf-8",
                                  error);
                } else if (pb_linux_config_save(config_path, &config) != 0) {
                    send_response(client, 500, "Internal Server Error",
                                  "text/plain; charset=utf-8", "Could not save configuration");
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
        "<section><h2>Status</h2><p id=\"status\" class=\"muted\">Loading...</p>"
        "<div id=\"details\" class=\"muted\"></div>"
        "<p><a href=\"api/status\">View raw status</a></p></section>"
        "<section><h2>Statistics since service start</h2><div id=\"stats\" class=\"muted\">Loading...</div></section>"
        "<section><h2>Configuration</h2><form id=\"form\">"
        "<label>Registrar host<input name=\"sip_host\" required></label>"
        "<label>Registrar port<input name=\"sip_port\" type=\"number\" min=\"1\" max=\"65535\"></label>"
        "<label>SIP username<input name=\"sip_user\" required></label>"
        "<label>SIP password<input name=\"sip_pass\" type=\"password\" placeholder=\"unchanged\"></label>"
        "<label>Local SIP port<input name=\"sip_local_port\" type=\"number\"></label>"
        "<label>RTP port<input name=\"rtp_port\" type=\"number\"></label>"
        "<label class=\"wide\">PhoneBlock API URL<input name=\"phoneblock_base_url\"></label>"
        "<label class=\"wide\">PhoneBlock token<input name=\"phoneblock_token\" type=\"password\" placeholder=\"unchanged\"></label>"
        "<label class=\"wide\">Announcement path<input name=\"announcement_path\" placeholder=\"/data/announcement.alaw\"></label>"
        "<label><input name=\"announcement_enabled\" type=\"checkbox\" value=\"1\"> Enable announcement playback</label>"
        "<label>Advertised host (NAT/routed setups)<input name=\"contact_host\" placeholder=\"leave empty unless behind NAT\"></label>"
        "<label>Advertised port<input name=\"contact_port\" type=\"number\" min=\"0\" max=\"65535\" placeholder=\"leave empty unless behind NAT\"></label>"
        "<div class=\"wide\"><button type=\"submit\">Save configuration</button>"
        "<span id=\"message\" class=\"muted\"></span></div></form></section>"
        "<section><h2>Pre-recorded announcements</h2><p class=\"muted\">Choose a localized message to play. Your uploaded message remains available.</p>"
        "<select id=\"announcementPreset\"></select>"
        "<button id=\"announcementPresetListen\" type=\"button\">Listen</button>"
        "<button id=\"announcementPresetUse\" type=\"button\">Use selected message</button>"
        "<span id=\"announcementPresetMessage\" class=\"muted\"></span></section>"
        "<section><h2>Custom announcement</h2><p class=\"muted\">Upload your own raw 8 kHz mono G.711 A-law audio (maximum 30 seconds), or switch back to it later.</p>"
        "<input id=\"announcementFile\" type=\"file\" accept=\".alaw,audio/basic\">"
        "<button id=\"announcementListen\" type=\"button\">Listen to custom file</button>"
        "<button id=\"announcementUpload\" type=\"button\">Upload announcement</button>"
        "<button id=\"announcementCustomUse\" type=\"button\">Use my uploaded message</button>"
        "<span id=\"announcementMessage\" class=\"muted\"></span></section>"
        "<p class=\"muted\">Restart the add-on after changing SIP or announcement settings.</p>"
        "<script>const q=s=>document.querySelector(s);let selectedAnnouncementPreset='';async function load(){"
        "const c=await fetch('api/config').then(r=>r.json());for(const [k,v] of Object.entries(c)){"
        "if(k==='announcement_preset')selectedAnnouncementPreset=v;const e=q('[name=\\\"'+k+'\\\"]');if(e){if(e.type==='checkbox')e.checked=!!v;else e.value=v||'';}}const s=await fetch('api/status').then(r=>r.json());"
        "q('#status').textContent=s.registered?'SIP registration: registered':'SIP registration: not registered';"
        "q('#status').className=s.registered?'ok':'error';q('#details').innerHTML="
        "'Registrar: '+s.sipHost+':'+s.sipPort+'<br>SIP user: '+s.sipUser+"
        "'<br>PhoneBlock token: '+(s.phoneblockTokenSet?'configured':'not configured')+"
        "'<br>Local SIP port: '+s.localSipPort+' | RTP port: '+s.rtpPort;"
        "q('#stats').innerHTML='Calls: '+s.calls+'<br>Spam blocked: '+s.spamBlocked+"
        "'<br>Calls passed: '+s.callsPassed+'<br>Classification errors: '+s.classificationErrors; }"
        "q('#form').onsubmit=async e=>{e.preventDefault();const formData=new FormData(e.target);"
        "formData.set('announcement_enabled',q('[name=announcement_enabled]').checked?'1':'0');"
        "const r=await fetch('api/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},"
        "body:new URLSearchParams(formData)});"
        "const text=await r.text();const m=q('#message');m.textContent=r.ok?"
        "'Saved. Restart the add-on to apply changes.':(text||'Save failed');"
        "m.className=r.ok?'ok':'error';};"
        "q('#announcementUpload').onclick=async()=>{const f=q('#announcementFile').files[0],m=q('#announcementMessage');"
        "if(!f){m.textContent='Choose an .alaw file first';return;}if(f.size>245760){m.textContent='File is larger than 30 seconds';return;}"
        "m.textContent='Uploading...';const r=await fetch('api/announcement',{method:'POST',headers:{'Content-Type':'application/octet-stream'},body:await f.arrayBuffer()});"
        "m.textContent=r.ok?'Uploaded and selected for playback.':'Upload failed';};"
        "q('#announcementFile').addEventListener('change',()=>q('#announcementMessage').textContent='');"
        "function playAlaw(bytes){const context=new (window.AudioContext||window.webkitAudioContext)();"
        "const buffer=context.createBuffer(1,bytes.length,8000),samples=buffer.getChannelData(0);"
        "for(let i=0;i<bytes.length;i++){let a=bytes[i]^85,s=a&128,e=(a>>4)&7,m=a&15;"
        "let v=e===0?(m<<4)+8:((m<<4)+264)<<(e-1);samples[i]=(s?-v:v)/32768;}"
        "const source=context.createBufferSource();source.buffer=buffer;source.connect(context.destination);"
        "source.onended=()=>context.close();source.start();}"
        "async function loadPresets(){const d=await fetch('api/announcement/catalog').then(r=>r.json()),s=q('#announcementPreset');"
        "for(const p of d.presets){const o=document.createElement('option');o.value=p.code;o.textContent=p.label;s.appendChild(o);}"
        "if(selectedAnnouncementPreset&&selectedAnnouncementPreset!=='custom')s.value=selectedAnnouncementPreset;}"
        "q('#announcementPresetListen').onclick=async()=>{const c=q('#announcementPreset').value;"
        "playAlaw(new Uint8Array(await (await fetch('api/announcement/preset?name='+c)).arrayBuffer()));};"
        "q('#announcementPresetUse').onclick=async()=>{const c=q('#announcementPreset').value,m=q('#announcementPresetMessage');"
        "const r=await fetch('api/announcement/select',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'name='+c});"
        "m.textContent=r.ok?'Selected. Your custom message is preserved. Restart the add-on to apply it.':'Selection failed';};"
        "q('#announcementCustomUse').onclick=async()=>{const m=q('#announcementMessage');"
        "const r=await fetch('api/announcement/custom',{method:'POST'});"
        "m.textContent=r.ok?'Your uploaded message is selected. Restart the add-on to apply it.':'No custom message available';};"
        "q('#announcementListen').onclick=async()=>{const f=q('#announcementFile').files[0];if(f)playAlaw(new Uint8Array(await f.arrayBuffer()));};"
        "loadPresets().catch(()=>{});"
        "load().catch(()=>q('#status').textContent='Unable to load status');</script>"
        "</body></html>";
}