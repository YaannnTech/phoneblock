#include "http_client_linux.h"

#include "platform.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
    size_t maximum;
    int overflow;
} response_buffer_t;

static size_t write_body(char *data, size_t size, size_t count, void *opaque)
{
    response_buffer_t *buffer = opaque;
    size_t incoming = size * count;
    if (incoming > buffer->maximum - buffer->length) {
        buffer->overflow = 1;
        return 0;
    }
    size_t required = buffer->length + incoming + 1;
    if (required > buffer->capacity) {
        size_t new_capacity = buffer->capacity ? buffer->capacity * 2 : 1024;
        while (new_capacity < required) new_capacity *= 2;
        char *new_data = realloc(buffer->data, new_capacity);
        if (!new_data) return 0;
        buffer->data = new_data;
        buffer->capacity = new_capacity;
    }
    memcpy(buffer->data + buffer->length, data, incoming);
    buffer->length += incoming;
    buffer->data[buffer->length] = '\0';
    return incoming;
}

int pb_http_get(const char *url, const char *bearer_token,
                size_t maximum_body, pb_http_response_t *response)
{
    if (!url || !response || maximum_body == 0) return -1;
    memset(response, 0, sizeof(*response));

    CURL *curl = curl_easy_init();
    if (!curl) return -1;
    response_buffer_t buffer = { .maximum = maximum_body };
    struct curl_slist *headers = NULL;
    if (bearer_token && bearer_token[0]) {
        size_t header_length = strlen(bearer_token) + 22;
        char *header = malloc(header_length);
        if (!header) {
            curl_easy_cleanup(curl);
            return -1;
        }
        snprintf(header, header_length, "Authorization: Bearer %s", bearer_token);
        headers = curl_slist_append(headers, header);
        free(header);
        if (!headers) {
            curl_easy_cleanup(curl);
            return -1;
        }
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 5000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 15000L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "PhoneBlock-Dongle/Linux");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode result = curl_easy_perform(curl);
    long status = 0;
    if (result == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK || buffer.overflow || !buffer.data) {
        pb_log_warn("http", "GET %s failed: %s%s", url,
                    curl_easy_strerror(result),
                    buffer.overflow ? " (response exceeded size limit)" : "");
        free(buffer.data);
        return -1;
    }
    response->status = status;
    response->body = buffer.data;
    response->length = buffer.length;
    return 0;
}

void pb_http_response_free(pb_http_response_t *response)
{
    if (!response) return;
    free(response->body);
    memset(response, 0, sizeof(*response));
}

int pb_http_post_xml(const char *url, const char *soap_action,
                     const char *body, size_t maximum_body,
                     pb_http_response_t *response)
{
    if (!url || !body || !response || maximum_body == 0) return -1;
    memset(response, 0, sizeof(*response));
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    response_buffer_t buffer = { .maximum = maximum_body };
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: text/xml; charset=\"utf-8\"");
    if (soap_action) {
        char action_header[512];
        snprintf(action_header, sizeof(action_header), "SOAPAction: %s", soap_action);
        headers = curl_slist_append(headers, action_header);
    }
    if (!headers) {
        curl_easy_cleanup(curl);
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(body));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 5000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 15000L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

    CURLcode result = curl_easy_perform(curl);
    long status = 0;
    if (result == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (result != CURLE_OK || buffer.overflow || !buffer.data) {
        pb_log_warn("http", "POST %s failed: %s%s", url,
                    curl_easy_strerror(result),
                    buffer.overflow ? " (response exceeded size limit)" : "");
        free(buffer.data);
        return -1;
    }
    response->status = status;
    response->body = buffer.data;
    response->length = buffer.length;
    return 0;
}