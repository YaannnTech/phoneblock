#include "tr064_client_linux.h"

#include "http_client_linux.h"

#include <stdio.h>
#include <stdlib.h>

int pb_tr064_soap_post(const char *host, int port, const char *service,
                       const char *action, const char *arguments,
                       size_t maximum_body, long *status,
                       char **response, size_t *response_length)
{
    if (!host || !service || !action || !status || !response
            || !response_length) return -1;
    if (port <= 0 || port > 65535) return -1;
    if (!arguments) arguments = "";

    int url_length = snprintf(NULL, 0, "http://%s:%d/upnp/control/%s",
                              host, port, service);
    char *url = malloc((size_t)url_length + 1);
    if (!url) return -1;
    snprintf(url, (size_t)url_length + 1, "http://%s:%d/upnp/control/%s",
             host, port, service);

    int body_length = snprintf(NULL, 0,
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">"
        "<s:Body><u:%s xmlns:u=\"%s\">%s</u:%s></s:Body></s:Envelope>",
        action, service, arguments, action);
    char *body = malloc((size_t)body_length + 1);
    if (!body) {
        free(url);
        return -1;
    }
    snprintf(body, (size_t)body_length + 1,
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">"
        "<s:Body><u:%s xmlns:u=\"%s\">%s</u:%s></s:Body></s:Envelope>",
        action, service, arguments, action);

    char soap_action[512];
    snprintf(soap_action, sizeof(soap_action), "\"%s#%s\"", service, action);
    pb_http_response_t result;
    int request_result = pb_http_post_xml(url, soap_action, body,
                                          maximum_body, &result);
    free(body);
    free(url);
    if (request_result != 0) return -1;
    *status = result.status;
    *response = result.body;
    *response_length = result.length;
    return 0;
}