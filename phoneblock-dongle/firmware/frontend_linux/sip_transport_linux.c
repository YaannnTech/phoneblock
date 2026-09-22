#include "sip_transport.h"

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

struct sip_transport {
    int socket;
    struct sockaddr_in registrar;
    char local_ip[INET_ADDRSTRLEN];
    int local_port;
};

static int resolve_ipv4(const char *host, int port, struct sockaddr_in *address)
{
    char port_text[12];
    snprintf(port_text, sizeof(port_text), "%d", port);
    struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_DGRAM };
    struct addrinfo *result = NULL;
    if (getaddrinfo(host, port_text, &hints, &result) != 0 || !result) return -1;
    memcpy(address, result->ai_addr, sizeof(*address));
    freeaddrinfo(result);
    return 0;
}

sip_transport_t *sip_transport_open(const char *transport,
                                    const char *registrar_host,
                                    int registrar_port,
                                    const char *tls_sni,
                                    int local_port)
{
    (void)tls_sni;
    if (!transport || strcmp(transport, "udp") != 0 || !registrar_host) {
        pb_log_err("sip_transport", "Linux backend supports UDP only");
        return NULL;
    }
    if (registrar_port == 0) registrar_port = 5060;

    sip_transport_t *transport_state = calloc(1, sizeof(*transport_state));
    if (!transport_state) return NULL;
    if (resolve_ipv4(registrar_host, registrar_port,
                     &transport_state->registrar) != 0) {
        pb_log_err("sip_transport", "DNS lookup failed for %s", registrar_host);
        free(transport_state);
        return NULL;
    }
    transport_state->socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (transport_state->socket < 0) {
        free(transport_state);
        return NULL;
    }
    struct sockaddr_in local = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons(local_port),
    };
    if (bind(transport_state->socket, (struct sockaddr *)&local,
             sizeof(local)) != 0
            || connect(transport_state->socket,
                       (struct sockaddr *)&transport_state->registrar,
                       sizeof(transport_state->registrar)) != 0) {
        close(transport_state->socket);
        free(transport_state);
        return NULL;
    }
    socklen_t local_length = sizeof(local);
    if (getsockname(transport_state->socket, (struct sockaddr *)&local,
                    &local_length) == 0) {
        transport_state->local_port = ntohs(local.sin_port);
        inet_ntop(AF_INET, &local.sin_addr, transport_state->local_ip,
                  sizeof(transport_state->local_ip));
    }
    return transport_state;
}

void sip_transport_close(sip_transport_t *transport)
{
    if (!transport) return;
    close(transport->socket);
    free(transport);
}

bool sip_transport_resolve(sip_transport_t *transport,
                           const char *registrar_host, int registrar_port,
                           const char *tls_sni)
{
    (void)tls_sni;
    if (!transport || !registrar_host || registrar_port <= 0) return false;
    return resolve_ipv4(registrar_host, registrar_port, &transport->registrar) == 0
        && connect(transport->socket, (struct sockaddr *)&transport->registrar,
                   sizeof(transport->registrar)) == 0;
}

int sip_transport_send(sip_transport_t *transport, const void *buffer, int length)
{
    if (!transport) return -1;
    return (int)send(transport->socket, buffer, (size_t)length, 0);
}

int sip_transport_send_to(sip_transport_t *transport,
                          const struct sockaddr_in *peer,
                          const void *buffer, int length)
{
    if (!transport || !peer) return -1;
    return (int)sendto(transport->socket, buffer, (size_t)length, 0,
                       (const struct sockaddr *)peer, sizeof(*peer));
}

int sip_transport_recv(sip_transport_t *transport, int timeout_ms,
                       void *buffer, int capacity, struct sockaddr_in *from)
{
    if (!transport || !buffer || capacity <= 0) return -1;
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(transport->socket, &readable);
    struct timeval timeout = {
        .tv_sec = timeout_ms < 0 ? 0 : timeout_ms / 1000,
        .tv_usec = timeout_ms < 0 ? 0 : (timeout_ms % 1000) * 1000,
    };
    int selected = select(transport->socket + 1, &readable, NULL, NULL,
                          timeout_ms < 0 ? NULL : &timeout);
    if (selected <= 0) return selected == 0 ? 0 : -1;
    socklen_t from_length = from ? sizeof(*from) : 0;
    return (int)recvfrom(transport->socket, buffer, (size_t)capacity, 0,
                         from ? (struct sockaddr *)from : NULL, &from_length);
}

const char *sip_transport_local_ip(const sip_transport_t *transport)
{
    return transport ? transport->local_ip : "";
}

int sip_transport_local_port(const sip_transport_t *transport)
{
    return transport ? transport->local_port : 0;
}

const char *sip_transport_via_token(const sip_transport_t *transport)
{
    (void)transport;
    return "UDP";
}

const char *sip_transport_uri_param(const sip_transport_t *transport)
{
    (void)transport;
    return "";
}

bool sip_transport_consume_reconnect(sip_transport_t *transport)
{
    (void)transport;
    return false;
}