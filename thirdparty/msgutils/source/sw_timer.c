/* The MIT License (MIT)
 *
 * Copyright (c) 2019 luoyun <sysu.zqlong@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

#include "msgutils/common_list.h"
#include "msgutils/os_memory.h"
#include "msgutils/os_thread.h"
#include "msgutils/os_time.h"
#include "msgutils/os_logger.h"
#include "msgutils/sw_timer.h"

#define LOG_TAG "timer"

struct swtimer {
    os_thread_t thread_id;
    os_mutex_t thread_mutex;
    os_cond_t thread_cond;
    void (*thread_cb)();
    const char *thread_name;
    bool thread_exit;

    unsigned long period_ms;
    bool reload;
    bool started;
};

static void *swtimer_thread_entry(void *arg)
{
    struct swtimer *timer = (struct swtimer *)arg;
    unsigned long long now = 0;
    unsigned long long escape = 0;
    unsigned long long wait = 0;
    unsigned long long period_us = timer->period_ms * 1000;

    OS_LOGD(LOG_TAG, "[%s]: Entry timer thread: thread_id=[%p]", timer->thread_name, timer->thread_id);

    OS_THREAD_SET_NAME(timer->thread_id, timer->thread_name);

    while (!timer->thread_exit) {
        OS_THREAD_MUTEX_LOCK(timer->thread_mutex);

        while (!timer->started && !timer->thread_exit)
            OS_THREAD_COND_WAIT(timer->thread_cond, timer->thread_mutex);

        if (timer->thread_exit) {
            OS_THREAD_MUTEX_UNLOCK(timer->thread_mutex);
            break;
        }

        wait = period_us - escape;
        OS_THREAD_COND_TIMEDWAIT(timer->thread_cond, timer->thread_mutex, wait);

        if (timer->started) {
            now = OS_MONOTONIC_USEC();

            timer->thread_cb();

            escape = OS_MONOTONIC_USEC() - now;

            if (timer->reload) {
                if (escape >= period_us) {
                    OS_LOGE(LOG_TAG, "[%s]: Handler timecost more than timer period, stop timer", timer->thread_name);
                    timer->started = false;
                    escape = 0;
                }
            }
            else {
                timer->started = false;
                escape = 0;
            }
        }
        else {
            escape = 0;
        }

        OS_THREAD_MUTEX_UNLOCK(timer->thread_mutex);
    }

    OS_LOGD(LOG_TAG, "[%s]: Leave timer thread: thread_id=[%p]", timer->thread_name, timer->thread_id);
    return NULL;
}

swtimer_t swtimer_create(struct swtimer_attr *attr, void (*swtimer_callback)())
{
    struct swtimer *timer = NULL;
    struct os_threadattr thread_attr = {
        .name = attr && attr->name ? attr->name : "timer",
        .priority = OS_THREAD_PRIO_SOFT_REALTIME,
        .stacksize = 1024,
        .joinable = true,
    };

    timer = OS_CALLOC(1, sizeof(struct swtimer));
    if (timer == NULL) {
        OS_LOGE(LOG_TAG, "Failed to allocate timer");
        return NULL;
    }

    timer->thread_mutex = OS_THREAD_MUTEX_CREATE();
    if (timer->thread_mutex == NULL) {
        OS_LOGE(LOG_TAG, "Failed to create thread_mutex");
        goto error;
    }

    timer->thread_cond = OS_THREAD_COND_CREATE();
    if (timer->thread_cond == NULL) {
        OS_LOGE(LOG_TAG, "Failed to create thread_cond");
        goto error;
    }

    if (attr == NULL || swtimer_callback == NULL) {
        OS_LOGE(LOG_TAG, "Invalid timer attr or callback");
        goto error;
    }

    timer->thread_name = OS_STRDUP(thread_attr.name);
    timer->thread_cb = swtimer_callback;
    timer->thread_exit = false;

    timer->period_ms = attr->period_ms;
    timer->reload = attr->reload;
    timer->started = false;

    timer->thread_id = OS_THREAD_CREATE(&thread_attr, swtimer_thread_entry, timer);
    if (timer->thread_id == NULL) {
        OS_LOGE(LOG_TAG, "[%s]: Failed to run thread timer", timer->thread_name);
        goto error;
    }

    return timer;

error:
    if (timer->thread_cond != NULL)
        OS_THREAD_COND_DESTROY(timer->thread_cond);
    if (timer->thread_mutex != NULL)
        OS_THREAD_MUTEX_DESTROY(timer->thread_mutex);
    OS_FREE(timer);
    return NULL;
}

int swtimer_start(swtimer_t timer)
{
    OS_THREAD_MUTEX_LOCK(timer->thread_mutex);

    timer->started = true;
    OS_THREAD_COND_SIGNAL(timer->thread_cond);

    OS_THREAD_MUTEX_UNLOCK(timer->thread_mutex);
    return 0;
}

int swtimer_stop(swtimer_t timer)
{
    OS_THREAD_MUTEX_LOCK(timer->thread_mutex);

    timer->started = false;
    OS_THREAD_COND_SIGNAL(timer->thread_cond);

    OS_THREAD_MUTEX_UNLOCK(timer->thread_mutex);
    return 0;
}

bool swtimer_is_active(swtimer_t timer)
{
    return timer->started;
}

void swtimer_destroy(swtimer_t timer)
{
    {
        OS_THREAD_MUTEX_LOCK(timer->thread_mutex);

        timer->started = false;
        timer->thread_exit = true;
        OS_THREAD_COND_SIGNAL(timer->thread_cond);

        OS_THREAD_MUTEX_UNLOCK(timer->thread_mutex);
    }

    OS_THREAD_JOIN(timer->thread_id, NULL);

    OS_THREAD_MUTEX_DESTROY(timer->thread_mutex);
    OS_THREAD_COND_DESTROY(timer->thread_cond);

    OS_FREE(timer->thread_name);
    OS_FREE(timer);
}
