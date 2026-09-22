#include "sip_stats_linux.h"

#include <stdatomic.h>
#include <string.h>

static atomic_uint_fast64_t s_calls;
static atomic_uint_fast64_t s_spam_blocked;
static atomic_uint_fast64_t s_calls_passed;
static atomic_uint_fast64_t s_classification_errors;

void pb_linux_sip_stats_reset(void)
{
    atomic_store(&s_calls, 0);
    atomic_store(&s_spam_blocked, 0);
    atomic_store(&s_calls_passed, 0);
    atomic_store(&s_classification_errors, 0);
}

void pb_linux_sip_stats_call(void) { atomic_fetch_add(&s_calls, 1); }
void pb_linux_sip_stats_spam(void) { atomic_fetch_add(&s_spam_blocked, 1); }
void pb_linux_sip_stats_passed(void) { atomic_fetch_add(&s_calls_passed, 1); }
void pb_linux_sip_stats_error(void)
{
    atomic_fetch_add(&s_classification_errors, 1);
}

void pb_linux_sip_stats_read(pb_linux_sip_stats_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->calls = atomic_load(&s_calls);
    out->spam_blocked = atomic_load(&s_spam_blocked);
    out->calls_passed = atomic_load(&s_calls_passed);
    out->classification_errors = atomic_load(&s_classification_errors);
}
