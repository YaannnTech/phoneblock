#pragma once

#include <stddef.h>
#include <stdint.h>

void pb_log_info(const char *tag, const char *fmt, ...);
void pb_log_warn(const char *tag, const char *fmt, ...);
void pb_log_err(const char *tag, const char *fmt, ...);

uint64_t pb_monotonic_us(void);
uint32_t pb_random_u32(void);

typedef struct pb_task pb_task_t;
typedef struct pb_mutex pb_mutex_t;

pb_task_t *pb_task_create(void (*fn)(void *), void *arg,
                          const char *name, size_t stack_bytes);
void pb_task_sleep_ms(uint32_t ms);
void pb_task_yield(void);

pb_mutex_t *pb_mutex_create(void);
void pb_mutex_lock(pb_mutex_t *mutex);
void pb_mutex_unlock(pb_mutex_t *mutex);

int pb_get_mac(uint8_t mac[6]);