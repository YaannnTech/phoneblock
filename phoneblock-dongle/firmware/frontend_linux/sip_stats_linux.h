#pragma once

#include <stdint.h>

typedef struct {
    uint64_t calls;
    uint64_t spam_blocked;
    uint64_t calls_passed;
    uint64_t classification_errors;
} pb_linux_sip_stats_t;

void pb_linux_sip_stats_reset(void);
void pb_linux_sip_stats_call(void);
void pb_linux_sip_stats_spam(void);
void pb_linux_sip_stats_passed(void);
void pb_linux_sip_stats_error(void);
void pb_linux_sip_stats_read(pb_linux_sip_stats_t *out);
