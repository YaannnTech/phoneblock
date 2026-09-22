#include "sip_register_linux.h"

#include "platform.h"
#include "sip_auth.h"
#include "sip_parse.h"
#include "sip_transport.h"

#include <openssl/evp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void md5_hex(const char *input, char output[33])
{
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_length = 0;
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    if (!context || EVP_DigestInit_ex(context, EVP_md5(), NULL) != 1
            || EVP_DigestUpdate(context, input, strlen(input)) != 1
            || EVP_DigestFinal_ex(context, digest, &digest_length) != 1) {
        EVP_MD_CTX_free(context);
        output[0] = '\0';
        return;
    }
    EVP_MD_CTX_free(context);
    static const char hex[] = "0123456789abcdef";
    for (unsigned int index = 0; index < digest_length; index++) {
        output[index * 2] = hex[digest[index] >> 4];
        output[index * 2 + 1] = hex[digest[index] & 0xf];
    }
    output[32] = '\0';
}

static int build_register(const sip_transport_t *transport, const char *host,
                          const char *user, const char *authorization,
                          char *out, size_t capacity)
{
    uint32_t random_value = pb_random_u32();
    return snprintf(out, capacity,
        "REGISTER sip:%s SIP/2.0\r\n"
        "Via: SIP/2.0/UDP %s:%d;branch=z9hG4bK%08x\r\n"
        "Max-Forwards: 70\r\n"
        "From: <sip:%s@%s>;tag=%08x\r\n"
        "To: <sip:%s@%s>\r\n"
        "Call-ID: %08x@%s\r\n"
        "CSeq: 1 REGISTER\r\n"
        "Contact: <sip:%s@%s:%d>\r\n"
        "Expires: 3600\r\n"
        "%s"
        "User-Agent: PhoneBlock-Dongle/Linux\r\n"
        "Content-Length: 0\r\n\r\n",
        host, sip_transport_local_ip(transport), sip_transport_local_port(transport),
        random_value, user, host, random_value ^ 0x13579bdfu,
        user, host, random_value, host, user,
        sip_transport_local_ip(transport), sip_transport_local_port(transport),
        authorization ? authorization : "");
}

static void build_digest_authorization(const char *host, int port,
                                       const char *user, const char *password,
                                       const auth_challenge_t *challenge,
                                       char *out, size_t capacity)
{
    char uri[256], input[512], ha1[33], ha2[33], response[33];
    snprintf(uri, sizeof(uri), "sip:%s:%d", host, port);
    snprintf(input, sizeof(input), "%s:%s:%s", user, challenge->realm, password);
    md5_hex(input, ha1);
    snprintf(input, sizeof(input), "REGISTER:%s", uri);
    md5_hex(input, ha2);
    snprintf(input, sizeof(input), "%s:%s:%s", ha1, challenge->nonce, ha2);
    md5_hex(input, response);
    snprintf(out, capacity,
             "Authorization: Digest username=\"%s\", realm=\"%s\", "
             "nonce=\"%s\", uri=\"%s\", response=\"%s\", algorithm=MD5\r\n",
             user, challenge->realm, challenge->nonce, uri, response);
}

int pb_linux_sip_register_probe(const char *host, int port,
                                const char *user, const char *password,
                                int local_port, int *status,
                                char *challenge, int challenge_cap)
{
    if (!host || !user || !password || !status || !challenge || challenge_cap <= 0) {
        return -1;
    }
    *status = 0;
    challenge[0] = '\0';
    sip_transport_t *transport = sip_transport_open("udp", host, port,
                                                     NULL, local_port);
    if (!transport) return -1;

    char request[2048];
    int request_length = build_register(transport, host, user, NULL,
                                        request, sizeof(request));
    if (request_length < 0 || (size_t)request_length >= sizeof(request)
            || sip_transport_send(transport, request, request_length) < 0) {
        sip_transport_close(transport);
        return -1;
    }

    char response[4096];
    struct sockaddr_in from;
    int response_length = sip_transport_recv(transport, 3000, response,
                                              sizeof(response) - 1, &from);
    if (response_length <= 0) {
        sip_transport_close(transport);
        return -1;
    }
    response[response_length] = '\0';
    *status = parse_status_code(response, response_length);
    if (*status == 401 || *status == 407) {
        const char *header = find_header(response, response_length,
                                         *status == 401
                                             ? "WWW-Authenticate"
                                             : "Proxy-Authenticate");
        if (!header) {
            sip_transport_close(transport);
            return -1;
        }
        header_value(header, response + response_length, challenge, challenge_cap);
        auth_challenge_t parsed;
        sip_auth_parse_challenge(challenge, &parsed);
        if (!parsed.valid) {
            sip_transport_close(transport);
            return -1;
        }
        char authorization[768];
        build_digest_authorization(host, port, user, password, &parsed,
                                   authorization, sizeof(authorization));
        request_length = build_register(transport, host, user, authorization,
                                        request, sizeof(request));
        if (request_length < 0 || (size_t)request_length >= sizeof(request)
                || sip_transport_send(transport, request, request_length) < 0) {
            sip_transport_close(transport);
            return -1;
        }
        response_length = sip_transport_recv(transport, 3000, response,
                                              sizeof(response) - 1, &from);
        if (response_length <= 0) {
            sip_transport_close(transport);
            return -1;
        }
        response[response_length] = '\0';
        *status = parse_status_code(response, response_length);
    }
    sip_transport_close(transport);
    return 0;
}
