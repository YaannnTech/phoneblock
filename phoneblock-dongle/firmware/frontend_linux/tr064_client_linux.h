#pragma once

#include <stddef.h>

// Post one unauthenticated TR-064 SOAP action. Authentication challenges and
// action-specific response parsing remain separate layers.
int pb_tr064_soap_post(const char *host, int port, const char *service,
                       const char *action, const char *arguments,
                       size_t maximum_body, long *status,
                       char **response, size_t *response_length);