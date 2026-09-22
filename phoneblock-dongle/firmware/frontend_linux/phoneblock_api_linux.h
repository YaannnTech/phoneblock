#pragma once

#include <stddef.h>

#include "api.h"

// Classify one /api/check-prefix JSON response using the same direct/range
// thresholds and personalization precedence as the ESP32 implementation.
// Returns 0 for a complete response, -1 for scanner or argument failure.
int pb_linux_classify_check(const char *phone_number, const char *json,
                            size_t json_length, int minimum_direct,
                            int minimum_range, pb_check_result_t *out);

// Query the Linux PhoneBlock API adapter. `base_url` is the site root without
// a trailing slash and `token` may be empty for unauthenticated testing.
int pb_linux_phoneblock_check(const char *base_url, const char *token,
                              const char *phone_number, int minimum_direct,
                              int minimum_range, pb_check_result_t *out);