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
    sip_transport_t *transport;
    struct sockaddr_in peer;
    char request_uri[256];
    char from_header[512];
    char to_header[512];
    char call_id[256];
} rtp_stream_args_t;

static int build_bye(const rtp_stream_args_t *args, char *out, size_t capacity)
{
    return snprintf(out, capacity,
        "BYE %s SIP/2.0\r\n"
        "Via: SIP/2.0/UDP %s:%d;branch=z9hG4bK%08x\r\n"
        "Max-Forwards: 70\r\n"
        "From: %s\r\n"
        "To: %s\r\n"
        "Call-ID: %s\r\n"
        "CSeq: 2 BYE\r\n"
        "Content-Length: 0\r\n\r\n",
        args->request_uri, sip_transport_local_ip(args->transport),
        sip_transport_local_port(args->transport), pb_random_u32(),
        args->to_header, args->from_header, args->call_id);
}

static void *rtp_stream_thread(void *opaque)
{
    rtp_stream_args_t *args = opaque;
    pb_linux_rtp_stream_alaw(args->host, args->port, args->path,
                             args->stop_requested);
    char bye[2048];
    int bye_length = build_bye(args, bye, sizeof(bye));
    if (bye_length > 0 && (size_t)bye_length < sizeof(bye)) {
        sip_transport_send_to(args->transport, &args->peer, bye, bye_length);
    }
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
    rtp_stream_args_t *pending_rtp = NULL;
    while (!*stop_requested) {
        int length = sip_transport_recv(transport, 500, packet,
                                         sizeof(packet) - 1, &peer);
        if (length <= 0) continue;
        packet[length] = '\0';
        char method[16];
        parse_method(packet, length, method, sizeof(method));
        if (strcmp(method, "ACK") == 0) {
            char ack_call_id[256];
            parse_call_id(packet, length, ack_call_id, sizeof(ack_call_id));
            bool matching_ack = pending_rtp && ack_call_id[0]
                && strcmp(ack_call_id, pending_rtp->call_id) == 0;
            pb_log_info("sip", "ACK received%s",
                        matching_ack ? " for active dialog" : " for unknown dialog");
            if (matching_ack) {
                pthread_t thread;
                if (pthread_create(&thread, NULL, rtp_stream_thread, pending_rtp) == 0) {
                    pthread_detach(thread);
                    pending_rtp = NULL;
                } else {
                    free(pending_rtp);
                    pending_rtp = NULL;
                }
            }
            continue;
        }
        if (strcmp(method, "BYE") == 0) {
            char bye_call_id[256];
            parse_call_id(packet, length, bye_call_id, sizeof(bye_call_id));
            bool matching_bye = pending_rtp && bye_call_id[0]
                && strcmp(bye_call_id, pending_rtp->call_id) == 0;
            int bye_response_length = sip_response_build(
                packet, length, matching_bye ? 200 : 481,
                matching_bye ? "OK" : "Call/Transaction Does Not Exist",
                "linux", NULL, user,
                sip_transport_local_ip(transport),
                sip_transport_local_port(transport), response, sizeof(response));
            if (bye_response_length > 0) {
                sip_transport_send_to(transport, &peer, response, bye_response_length);
            }
            if (matching_bye) {
                free(pending_rtp);
                pending_rtp = NULL;
            }
            continue;
        }
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
                free(pending_rtp);
                pending_rtp = NULL;
                rtp_stream_args_t *args = calloc(1, sizeof(*args));
                if (args) {
                    snprintf(args->host, sizeof(args->host), "%s", remote_rtp_ip);
                    args->port = remote_rtp_port;
                    snprintf(args->path, sizeof(args->path), "%s", announcement_path);
                    args->stop_requested = stop_requested;
                    args->transport = transport;
                    args->peer = peer;
                    const char *to = find_header(packet, length, "To");
                    const char *from = find_header(packet, length, "From");
                    const char *call_id = find_header(packet, length, "Call-ID");
                    char value[512];
                    if (to) {
                        header_value(to, packet + length, value, sizeof(value));
                        snprintf(args->to_header, sizeof(args->to_header), "%s", value);
                        parse_uri(value, (int)strlen(value), args->request_uri,
                                  sizeof(args->request_uri));
                    }
                    if (from) {
                        header_value(from, packet + length, value, sizeof(value));
                        snprintf(args->from_header, sizeof(args->from_header), "%s", value);
                    }
                    if (call_id) {
                        header_value(call_id, packet + length, value, sizeof(value));
                        snprintf(args->call_id, sizeof(args->call_id), "%s", value);
                    }
                    pending_rtp = args;
                }
            }
        }
    }
    free(pending_rtp);
    sip_transport_close(transport);
    return 0;
}