/* The MIT License (MIT)
 *
 * Copyright (c) 2018 luoyun <sysu.zqlong@gmail.com>
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

#ifndef __OS_THREAD_H__
#define __OS_THREAD_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *os_thread_t;
typedef void *os_mutex_t;
typedef void *os_cond_t;

#if 0
// If PTHREAD_MUTEX_INITIALIZER not supported on some platforms,
// just declare mutex null, ofcurse it's unsafe.
#define OS_MUTEX_DECLARE(name) static os_mutex_t name = NULL;
#else
#include <pthread.h>
#define OS_MUTEX_DECLARE(name) \
    static pthread_mutex_t temp##name = PTHREAD_MUTEX_INITIALIZER; \
    static os_mutex_t name = (os_mutex_t)(&temp##name);
#endif

// FIXME: Config task priority on your platform, current config for MT7686
#if defined(OS_FREERTOS)
enum os_threadprio {
    OS_THREAD_PRIO_INVALID = -1,
    OS_THREAD_PRIO_IDLE = 0, // lowest, special for idle task
    OS_THREAD_PRIO_LOW = 2,
    OS_THREAD_PRIO_NORMAL = 3,
    OS_THREAD_PRIO_HIGH = 6,
    OS_THREAD_PRIO_SOFT_REALTIME = 7,
    OS_THREAD_PRIO_HARD_REALTIME = 8,
};

#else // NOT USED FOR Linux/MacosX/Android, DON'T MODIFY
enum os_threadprio {
    OS_THREAD_PRIO_INVALID = -1,
    OS_THREAD_PRIO_HARD_REALTIME,
    OS_THREAD_PRIO_SOFT_REALTIME,
    OS_THREAD_PRIO_HIGH,
    OS_THREAD_PRIO_NORMAL,
    OS_THREAD_PRIO_LOW,
    OS_THREAD_PRIO_IDLE,
};
#endif

struct os_threadattr {
    const char *name;
    enum os_threadprio priority;
    unsigned int stacksize;
    bool joinable;
};

void OS_THREAD_SLEEP_USEC(unsigned long usec);
void OS_THREAD_SLEEP_MSEC(unsigned long msec);

os_thread_t OS_THREAD_CREATE(struct os_threadattr *attr, void *(*cb)(void *arg), void *arg);
void OS_THREAD_CANCEL(os_thread_t tid);
int OS_THREAD_JOIN(os_thread_t tid, void **retval);
os_thread_t OS_THREAD_SELF();
int OS_THREAD_SET_NAME(os_thread_t tid, const char *name);

os_mutex_t OS_THREAD_MUTEX_CREATE();
int OS_THREAD_MUTEX_LOCK(os_mutex_t mutex);
int OS_THREAD_MUTEX_TRYLOCK(os_mutex_t mutex);
int OS_THREAD_MUTEX_UNLOCK(os_mutex_t mutex);
void OS_THREAD_MUTEX_DESTROY(os_mutex_t mutex);

os_cond_t OS_THREAD_COND_CREATE();
int OS_THREAD_COND_WAIT(os_cond_t cond, os_mutex_t mutex);
int OS_THREAD_COND_TIMEDWAIT(os_cond_t cond, os_mutex_t mutex, unsigned long usec);
int OS_THREAD_COND_SIGNAL(os_cond_t cond);
void OS_THREAD_COND_DESTROY(os_cond_t cond);

#ifdef __cplusplus
}
#endif

#endif /* __OS_THREAD_H__ */
