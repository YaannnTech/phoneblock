#define _GNU_SOURCE

#include "platform.h"

#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <time.h>
#include <unistd.h>

struct pb_task {
    pthread_t thread;
};

struct pb_mutex {
    pthread_mutex_t mutex;
};

static void pb_log(const char *level, const char *tag, const char *fmt,
                   va_list args)
{
    fprintf(stderr, "%s (%s): ", level, tag ? tag : "phoneblock");
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
}

void pb_log_info(const char *tag, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    pb_log("I", tag, fmt, args);
    va_end(args);
}

void pb_log_warn(const char *tag, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    pb_log("W", tag, fmt, args);
    va_end(args);
}

void pb_log_err(const char *tag, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    pb_log("E", tag, fmt, args);
    va_end(args);
}

uint64_t pb_monotonic_us(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0;
    return (uint64_t)now.tv_sec * 1000000u + (uint64_t)now.tv_nsec / 1000u;
}

uint32_t pb_random_u32(void)
{
    uint32_t value;
    ssize_t count = getrandom(&value, sizeof(value), 0);
    if (count == (ssize_t)sizeof(value)) return value;

    int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd >= 0) {
        count = read(fd, &value, sizeof(value));
        close(fd);
        if (count == (ssize_t)sizeof(value)) return value;
    }

    value = (uint32_t)time(NULL) ^ (uint32_t)getpid();
    return value ^ (uint32_t)(uintptr_t)&value;
}

void pb_task_sleep_ms(uint32_t milliseconds)
{
    struct timespec delay = {
        .tv_sec = milliseconds / 1000u,
        .tv_nsec = (long)(milliseconds % 1000u) * 1000000L
    };
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {
    }
}

void pb_task_yield(void)
{
    sched_yield();
}

typedef struct {
    void (*fn)(void *);
    void *arg;
    pb_task_t *task;
} pb_task_start_t;

static void *pb_task_start(void *opaque)
{
    pb_task_start_t start = *(pb_task_start_t *)opaque;
    free(opaque);
    start.fn(start.arg);
    free(start.task);
    return NULL;
}

pb_task_t *pb_task_create(void (*fn)(void *), void *arg,
                          const char *name, size_t stack_bytes)
{
    (void)name;
    if (!fn) return NULL;

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

    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes) != 0) {
        free(task);
        free(start);
        return NULL;
    }
    if (stack_bytes > 0) {
        int rc = pthread_attr_setstacksize(&attributes, stack_bytes);
        if (rc != 0) {
            pthread_attr_destroy(&attributes);
            free(task);
            free(start);
            return NULL;
        }
    }
    pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
    int rc = pthread_create(&task->thread, &attributes, pb_task_start, start);
    pthread_attr_destroy(&attributes);
    if (rc != 0) {
        free(task);
        free(start);
        return NULL;
    }
    return task;
}

pb_mutex_t *pb_mutex_create(void)
{
    pb_mutex_t *mutex = calloc(1, sizeof(*mutex));
    if (!mutex || pthread_mutex_init(&mutex->mutex, NULL) != 0) {
        free(mutex);
        return NULL;
    }
    return mutex;
}

void pb_mutex_lock(pb_mutex_t *mutex)
{
    if (mutex) pthread_mutex_lock(&mutex->mutex);
}

void pb_mutex_unlock(pb_mutex_t *mutex)
{
    if (mutex) pthread_mutex_unlock(&mutex->mutex);
}

int pb_get_mac(uint8_t mac[6])
{
    if (!mac) return -1;
    memset(mac, 0, 6);

    struct ifaddrs *interfaces;
    if (getifaddrs(&interfaces) != 0) return -1;
    int result = -1;
    for (struct ifaddrs *entry = interfaces; entry; entry = entry->ifa_next) {
        if (!entry->ifa_addr || (entry->ifa_flags & IFF_LOOPBACK)) continue;
        if (entry->ifa_addr->sa_family != AF_PACKET) continue;
        struct sockaddr_ll *address = (struct sockaddr_ll *)entry->ifa_addr;
        if (address->sll_halen != 6) continue;
        memcpy(mac, address->sll_addr, 6);
        result = 0;
        break;
    }
    freeifaddrs(interfaces);
    return result;
}