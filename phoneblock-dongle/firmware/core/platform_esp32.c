#include "platform.h"

#include <stdarg.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

struct pb_task {
    TaskHandle_t handle;
};

struct pb_mutex {
    SemaphoreHandle_t handle;
};

static void pb_log(esp_log_level_t level, const char *tag, const char *fmt,
                   va_list args)
{
    esp_log_writev(level, tag, fmt, args);
}

void pb_log_info(const char *tag, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    pb_log(ESP_LOG_INFO, tag, fmt, args);
    va_end(args);
}

void pb_log_warn(const char *tag, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    pb_log(ESP_LOG_WARN, tag, fmt, args);
    va_end(args);
}

void pb_log_err(const char *tag, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    pb_log(ESP_LOG_ERROR, tag, fmt, args);
    va_end(args);
}

uint64_t pb_monotonic_us(void)
{
    return (uint64_t)esp_timer_get_time();
}

uint32_t pb_random_u32(void)
{
    return esp_random();
}

int pb_watchdog_is_subscribed(void)
{
    return esp_task_wdt_status(NULL) == ESP_OK;
}

void pb_watchdog_subscribe(void)
{
    esp_task_wdt_add(NULL);
}

void pb_watchdog_reset(void)
{
    esp_task_wdt_reset();
}

void pb_task_sleep_ms(uint32_t milliseconds)
{
    vTaskDelay(pdMS_TO_TICKS(milliseconds));
}

void pb_task_delay_until_ms(uint64_t *deadline_us, uint32_t interval_ms)
{
    if (!deadline_us) return;
    uint64_t now = pb_monotonic_us();
    if (*deadline_us == 0) *deadline_us = now;
    *deadline_us += (uint64_t)interval_ms * 1000u;
    if (*deadline_us > now) {
        uint64_t delay_us = *deadline_us - now;
        pb_task_sleep_ms((uint32_t)((delay_us + 999u) / 1000u));
    } else {
        *deadline_us = now;
    }
}

void pb_task_yield(void)
{
    taskYIELD();
}

typedef struct {
    void (*fn)(void *);
    void *arg;
    pb_task_t *task;
} pb_task_start_t;

static void pb_task_start(void *opaque)
{
    pb_task_start_t start = *(pb_task_start_t *)opaque;
    free(opaque);
    start.fn(start.arg);
    free(start.task);
    vTaskDelete(NULL);
}

pb_task_t *pb_task_create(void (*fn)(void *), void *arg,
                          const char *name, size_t stack_bytes)
{
    if (!fn || stack_bytes > UINT32_MAX * sizeof(StackType_t)) return NULL;

    pb_task_t *task = calloc(1, sizeof(*task));
    pb_task_start_t *start = malloc(sizeof(*start));
    if (!task || !start) {
        free(task);
        free(start);
        return NULL;
    }
    start->fn = fn;
    start->arg = arg;
    start->task = task;

    uint32_t stack_words = (uint32_t)((stack_bytes + sizeof(StackType_t) - 1)
                                      / sizeof(StackType_t));
    if (xTaskCreate(pb_task_start, name ? name : "phoneblock", stack_words,
                    start, 5, &task->handle) != pdPASS) {
        free(task);
        free(start);
        return NULL;
    }
    return task;
}

pb_mutex_t *pb_mutex_create(void)
{
    pb_mutex_t *mutex = calloc(1, sizeof(*mutex));
    if (!mutex) return NULL;
    mutex->handle = xSemaphoreCreateMutex();
    if (!mutex->handle) {
        free(mutex);
        return NULL;
    }
    return mutex;
}

void pb_mutex_lock(pb_mutex_t *mutex)
{
    if (mutex) xSemaphoreTake(mutex->handle, portMAX_DELAY);
}

void pb_mutex_unlock(pb_mutex_t *mutex)
{
    if (mutex) xSemaphoreGive(mutex->handle);
}

int pb_get_mac(uint8_t mac[6])
{
    if (!mac) return -1;
    return esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK ? 0 : -1;
}