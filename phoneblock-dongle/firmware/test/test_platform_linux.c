#include <assert.h>
#include <pthread.h>
#include <stdio.h>

#include "platform.h"

static volatile int task_done;

static void mark_done(void *arg)
{
    int *value = arg;
    *value = 1;
    task_done = 1;
}

static void test_time(void)
{
    uint64_t before = pb_monotonic_us();
    pb_task_sleep_ms(2);
    uint64_t after = pb_monotonic_us();
    assert(after >= before);
}

static void test_random(void)
{
    uint32_t first = pb_random_u32();
    uint32_t second = pb_random_u32();
    assert(first != second);
}

static void test_task(void)
{
    int value = 0;
    task_done = 0;
    assert(pb_task_create(mark_done, &value, "test", 0) != NULL);
    for (int attempt = 0; attempt < 100 && !task_done; attempt++) {
        pb_task_sleep_ms(1);
    }
    assert(value == 1);
}

static void test_mutex(void)
{
    pb_mutex_t *mutex = pb_mutex_create();
    assert(mutex != NULL);
    pb_mutex_lock(mutex);
    pb_mutex_unlock(mutex);
}

static void test_mac(void)
{
    uint8_t mac[6];
    int result = pb_get_mac(mac);
    assert(result == 0 || result == -1);
}

int main(void)
{
    test_time();
    test_random();
    test_task();
    test_mutex();
    test_mac();
    puts("test_platform_linux: all tests passed");
    return 0;
}