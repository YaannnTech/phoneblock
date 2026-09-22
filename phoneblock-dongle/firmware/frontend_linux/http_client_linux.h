#pragma once

#include <stddef.h>

typedef struct {
    long status;
    char *body;
    size_t length;
} pb_http_response_t;

// Perform an HTTPS GET with an optional bearer token. TLS certificate and
// hostname verification remain enabled through libcurl's defaults. Returns
// 0 for a completed request, -1 for setup, transport, or size failures.
int pb_http_get(const char *url, const char *bearer_token,
                size_t maximum_body, pb_http_response_t *response);

void pb_http_response_free(pb_http_response_t *response);