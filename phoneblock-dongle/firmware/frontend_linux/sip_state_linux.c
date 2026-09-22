#include "sip_state_linux.h"

#include <stdatomic.h>

static atomic_bool s_registered;

void pb_linux_sip_state_set_registered(bool registered)
{
    atomic_store_explicit(&s_registered, registered, memory_order_release);
}

bool pb_linux_sip_state_is_registered(void)
{
    return atomic_load_explicit(&s_registered, memory_order_acquire);
}
