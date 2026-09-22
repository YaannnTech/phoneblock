#include "tr064_provision_linux.h"

#include "http_client_linux.h"
#include "platform.h"
#include "tr064_parse.h"

#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SERVICE "urn:dslforum-org:service:X_VoIP:1"
#define CONTROL "x_voip"

static void md5_hex(const char *input, char output[33])
{
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx || EVP_DigestInit_ex(ctx, EVP_md5(), NULL) != 1
            || EVP_DigestUpdate(ctx, input, strlen(input)) != 1
            || EVP_DigestFinal_ex(ctx, digest, &length) != 1) {
        EVP_MD_CTX_free(ctx);
        output[0] = '\0';
        return;
    }
    EVP_MD_CTX_free(ctx);
    static const char hex[] = "0123456789abcdef";
    for (unsigned int index = 0; index < length; index++) {
        output[index * 2] = hex[digest[index] >> 4];
        output[index * 2 + 1] = hex[digest[index] & 0xf];
    }
    output[length * 2] = '\0';
}

static int soap_post(const char *host, int port, const char *action,
                     const char *body, int allow_challenge, char **response)
{
    char url[256];
    snprintf(url, sizeof(url), "http://%s:%d/upnp/control/%s", host, port, CONTROL);
    char soap_action[256];
    snprintf(soap_action, sizeof(soap_action), "\"%s#%s\"", SERVICE, action);
    pb_http_response_t result;
    if (pb_http_post_xml(url, soap_action, body, 16384, &result) != 0
            || ((!allow_challenge && (result.status < 200 || result.status >= 300))
                || (allow_challenge && result.status != 503 && result.status != 200))) {
        pb_log_err("tr064", "SOAP %s failed: transport or HTTP status %ld",
                   action, result.status);
        pb_http_response_free(&result);
        return -1;
    }
    pb_log_info("tr064", "SOAP %s returned HTTP %ld (%zu bytes)",
                action, result.status, result.length);
    *response = result.body;
    return 0;
}

static int call_action(const char *host, int port, const char *user,
                       const char *password, const char *action,
                       const char *arguments, char **response)
{
    char init[4096];
    snprintf(init, sizeof(init),
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">"
        "<s:Header><h:InitChallenge xmlns:h=\"http://soap-authentication.org/digest/2001/10/\" s:mustUnderstand=\"1\"><UserID>%s</UserID></h:InitChallenge></s:Header>"
        "<s:Body><u:%s xmlns:u=\"%s\">%s</u:%s></s:Body></s:Envelope>",
        user, action, SERVICE, arguments ? arguments : "", action);
    char *challenge = NULL;
    if (soap_post(host, port, action, init, 1, &challenge) != 0) return -1;
    char nonce[128] = "", realm[128] = "";
    tr064_xml_find_text(challenge, "Nonce", nonce, sizeof(nonce));
    tr064_xml_find_text(challenge, "Realm", realm, sizeof(realm));
    if (!nonce[0] || !realm[0]) {
        pb_log_err("tr064", "InitChallenge missing Nonce/Realm; response: %.512s",
                   challenge);
    }
    free(challenge);
    if (!nonce[0] || !realm[0]) return -1;

    char input[512], secret[33], response_hash[33];
    snprintf(input, sizeof(input), "%s:%s:%s", user, realm, password);
    md5_hex(input, secret);
    snprintf(input, sizeof(input), "%s:%s", secret, nonce);
    md5_hex(input, response_hash);
    char auth[4096];
    snprintf(auth, sizeof(auth),
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">"
        "<s:Header><h:ClientAuth xmlns:h=\"http://soap-authentication.org/digest/2001/10/\" s:mustUnderstand=\"1\"><UserID>%s</UserID><Nonce>%s</Nonce><Auth>%s</Auth><Realm>%s</Realm></h:ClientAuth></s:Header>"
        "<s:Body><u:%s xmlns:u=\"%s\">%s</u:%s></s:Body></s:Envelope>",
        user, nonce, response_hash, realm, action, SERVICE,
        arguments ? arguments : "", action);
    return soap_post(host, port, action, auth, 0, response);
}

int pb_tr064_provision_sip(const char *host, int port,
                           const char *admin_user, const char *admin_pass,
                           const char *phone_name,
                           pb_tr064_sip_credentials_t *out)
{
    if (!host || !admin_user || !admin_pass || !out) return -1;
    memset(out, 0, sizeof(*out));
    char user[32];
    snprintf(user, sizeof(user), "phoneblock-%08x", (unsigned)port);
    char password[24];
    snprintf(password, sizeof(password), "PhoneBlock%08x", (unsigned)port);
    char args[1024];
    snprintf(args, sizeof(args),
        "<NewX_AVM-DE_ClientIndex>0</NewX_AVM-DE_ClientIndex>"
        "<NewX_AVM-DE_ClientPassword>%s</NewX_AVM-DE_ClientPassword>"
        "<NewX_AVM-DE_ClientUsername>%s</NewX_AVM-DE_ClientUsername>"
        "<NewX_AVM-DE_PhoneName>%s</NewX_AVM-DE_PhoneName>"
        "<NewX_AVM-DE_ClientId></NewX_AVM-DE_ClientId>"
        "<NewX_AVM-DE_OutGoingNumber></NewX_AVM-DE_OutGoingNumber>"
        "<NewX_AVM-DE_InComingNumbers></NewX_AVM-DE_InComingNumbers>",
        password, user, phone_name && phone_name[0] ? phone_name : "PhoneBlock");
    char *response = NULL;
    if (call_action(host, port, admin_user, admin_pass, "X_AVM-DE_SetClient4",
                    args, &response) != 0) {
        pb_log_err("tr064", "SetClient4 failed for Fritz!Box %s:%d as user '%s'",
                   host, port, admin_user);
        return -1;
    }
    tr064_xml_find_text(response, "NewX_AVM-DE_InternalNumber",
                        out->internal_number, sizeof(out->internal_number));
    free(response);
    snprintf(out->sip_user, sizeof(out->sip_user), "%s", user);
    snprintf(out->sip_pass, sizeof(out->sip_pass), "%s", password);
    return 0;
}