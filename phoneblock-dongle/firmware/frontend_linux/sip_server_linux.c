#include "sip_server_linux.h"

#include "platform.h"
#include "sip_parse.h"
#include "sip_response.h"
#include "sip_transport.h"
#include "phoneblock_api_linux.h"
#include "rtp_linux.h"

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char host[64];
    int port;
    char path[256];
    volatile sig_atomic_t *stop_requested;
} rtp_stream_args_t;

static void *rtp_stream_thread(void *opaque)
{
    rtp_stream_args_t *args = opaque;
    pb_linux_rtp_stream_alaw(args->host, args->port, args->path,
                             args->stop_requested);
    free(args);
    return NULL;
}

int pb_linux_sip_listen(const char *host, int port, const char *user,
                        int local_port, const char *phoneblock_base_url,
                        const char *phoneblock_token,
                        const char *announcement_path,
                        volatile sig_atomic_t *stop_requested)
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
        int response_status = 200;
        const char *response_reason = "OK";
        int spam_call = 0;
        char remote_rtp_ip[64] = "";
        int remote_rtp_port = 0;
        if (strcmp(method, "INVITE") == 0) {
            const char *from = find_header(packet, length, "From");
            char from_value[512];
            char uri[256];
            char number[128];
            int phone_result = -1;
            if (from) {
                header_value(from, packet + length, from_value, sizeof(from_value));
                parse_uri(from_value, (int)strlen(from_value), uri, sizeof(uri));
                user_from_uri(uri, number, sizeof(number));
                normalize_e164(number, number, sizeof(number), "+49");
                pb_check_result_t check;
                phone_result = pb_linux_phoneblock_check(
                    phoneblock_base_url, phoneblock_token, number, 4, 10, &check);
                if (phone_result == 0 && check.verdict != VERDICT_SPAM) {
                    response_status = 486;
                    response_reason = "Busy Here";
                } else if (phone_result == 0 && check.verdict == VERDICT_SPAM) {
                    spam_call = 1;
                    parse_sdp_connection_ip(packet, length, remote_rtp_ip,
                                             sizeof(remote_rtp_ip));
                    remote_rtp_port = parse_sdp_audio_port(packet, length);
                }
                pb_log_info("sip", "INVITE %s classified as %s",
                            number, phone_result == 0
                                && check.verdict == VERDICT_SPAM ? "SPAM" : "not SPAM");
            } else {
                response_status = 486;
                response_reason = "Busy Here";
            }
        }
        int response_length = sip_response_build(
            packet, length, response_status, response_reason, "linux", NULL, user,
            sip_transport_local_ip(transport),
            sip_transport_local_port(transport), response, sizeof(response));
        if (response_length > 0) {
            sip_transport_send_to(transport, &peer, response, response_length);
            if (spam_call && remote_rtp_ip[0] && remote_rtp_port > 0
                    && announcement_path && announcement_path[0]) {
                rtp_stream_args_t *args = calloc(1, sizeof(*args));
                if (args) {
                    snprintf(args->host, sizeof(args->host), "%s", remote_rtp_ip);
                    args->port = remote_rtp_port;
                    snprintf(args->path, sizeof(args->path), "%s", announcement_path);
                    args->stop_requested = stop_requested;
                    pthread_t thread;
                    if (pthread_create(&thread, NULL, rtp_stream_thread, args) == 0) {
                        pthread_detach(thread);
                    } else {
                        free(args);
                    }
                }
            }
        }
    }
    sip_transport_close(transport);
    return 0;
}