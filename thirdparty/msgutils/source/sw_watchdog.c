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
#include "msgutils/sw_watchdog.h"

#define LOG_TAG "watchdog"

#define DEFAULT_TICK_MS 100
#define DEFAULT_MIN_TIMEOUT_MS (10 * DEFAULT_TICK_MS)

struct swwatchdog {
    struct listnode list;
    os_thread_t thread_id;
    const char *thread_name;
    os_mutex_t mutex;
    os_cond_t cond;

    unsigned long tick; // ms
    unsigned int active_num;
};

struct swwatchdog_node {
    const char *name;
    unsigned long long timeout_ms;
    void (*timeout_cb)(void *data);
    void *data;

    bool active;
    unsigned long long count;

    struct listnode listnode;
};

OS_MUTEX_DECLARE(g_watchdog_mutex);
static struct swwatchdog *g_watchdog = NULL;

static void default_timeout_cb(void *data)
{
    struct swwatchdog_node *node = (struct swwatchdog_node *)data;
    //int *ptr = (int *)0;

    OS_LOGF(LOG_TAG, "Thread[%s] bited by watchdog, exit...", node->name);
    //*ptr = 0; // make crash
}

static bool swwatchdog_check_node_l(swwatch_t node)
{
    struct swwatchdog *wd = g_watchdog;
    struct listnode *item;

    list_for_each(item, &wd->list) {
        struct swwatchdog_node *temp = node_to_item(item, struct swwatchdog_node, listnode);
        if (temp == node)
            return true;
    }

    return false;
}

static void *swwatchdog_thread_entry(void *arg)
{
    struct swwatchdog *wd = (struct swwatchdog *)arg;
    struct swwatchdog_node *node;
    struct listnode *item, *tmp;
    bool timeout;

    OS_LOGD(LOG_TAG, "Entry watchdog thread: thread_id=[%p]", wd->thread_id);

    OS_THREAD_SET_NAME(wd->thread_id, wd->thread_name);

    while (true) {
        OS_THREAD_SLEEP_MSEC(wd->tick);

        OS_THREAD_MUTEX_LOCK(wd->mutex);

        while (wd->active_num == 0)
            OS_THREAD_COND_WAIT(wd->cond, wd->mutex);

        list_for_each_safe(item, tmp, &wd->list) {
            node = node_to_item(item, struct swwatchdog_node, listnode);
            timeout = false;

            if (node->active) {
                if (node->count > node->timeout_ms) {
                    OS_LOGF(LOG_TAG, "Thread[%s] timeout, bited by watchdog", node->name);
                    timeout = true;
                }
                node->count += wd->tick;
            }

            if (timeout) {
                if (node->timeout_cb != NULL)
                    node->timeout_cb(node->data);
                else
                    default_timeout_cb(node);

                list_remove(item);
                OS_FREE(node);
                wd->active_num--;
            }
        }

        OS_THREAD_MUTEX_UNLOCK(wd->mutex);
    }

    return NULL;
}

static struct swwatchdog *swwatchdog_init()
{
    if (g_watchdog == NULL) {
        if (g_watchdog_mutex != NULL)
            OS_THREAD_MUTEX_LOCK(g_watchdog_mutex);

        if (g_watchdog == NULL) {
            struct os_threadattr attr  = {
                .name = "watchdog",
                .priority = OS_THREAD_PRIO_SOFT_REALTIME,
                .stacksize = 1024,
                .joinable = true,
            };

            g_watchdog = OS_CALLOC(1, sizeof(struct swwatchdog));
            if (g_watchdog == NULL) {
                OS_LOGE(LOG_TAG, "Failed to create watchdog instance");
                if (g_watchdog_mutex != NULL)
                    OS_THREAD_MUTEX_UNLOCK(g_watchdog_mutex);
                return NULL;
            }

            g_watchdog->mutex = OS_THREAD_MUTEX_CREATE();
            if (g_watchdog->mutex == NULL) {
                OS_LOGE(LOG_TAG, "Failed to create watchdog mutex");
                goto error;
            }

            g_watchdog->cond = OS_THREAD_COND_CREATE();
            if (g_watchdog->cond == NULL) {
                OS_LOGE(LOG_TAG, "Failed to create watchdog cond");
                goto error;
            }

            g_watchdog->tick = DEFAULT_TICK_MS;
            g_watchdog->active_num = 0;
            g_watchdog->thread_name = attr.name;

            g_watchdog->thread_id = OS_THREAD_CREATE(&attr, swwatchdog_thread_entry, g_watchdog);
            if (g_watchdog->thread_id == NULL) {
                OS_LOGE(LOG_TAG, "Failed to run watchdog thread");
                goto error;
            }

            list_init(&g_watchdog->list);
        }

        if (g_watchdog_mutex != NULL)
            OS_THREAD_MUTEX_UNLOCK(g_watchdog_mutex);
    }
    return g_watchdog;

error:
    if (g_watchdog->cond != NULL)
        OS_THREAD_COND_DESTROY(g_watchdog->cond);

    if (g_watchdog->mutex != NULL)
        OS_THREAD_MUTEX_DESTROY(g_watchdog->mutex);

    OS_FREE(g_watchdog);
    g_watchdog = NULL;

    if (g_watchdog_mutex != NULL)
        OS_THREAD_MUTEX_UNLOCK(g_watchdog_mutex);
    return NULL;
}

swwatch_t swwatchdog_create(const char *name, unsigned long long timeout_ms, void (*timeout_cb)(void *arg), void *arg)
{
    struct swwatchdog_node *node;
    struct swwatchdog *wd = swwatchdog_init();

    if (wd == NULL) {
        OS_LOGE(LOG_TAG, "Can't create watchdog handle");
        return NULL;
    }

    if (timeout_ms < DEFAULT_MIN_TIMEOUT_MS) {
        OS_LOGE(LOG_TAG, "Invalid timeout_ms[%llu], force timeout_ms %u", timeout_ms, DEFAULT_MIN_TIMEOUT_MS);
        timeout_ms = DEFAULT_MIN_TIMEOUT_MS;
    }

    node = OS_CALLOC(1, sizeof(struct swwatchdog_node));
    if (node == NULL) {
        OS_LOGE(LOG_TAG, "Failed to allocate watch node");
        return NULL;
    }

    node->name = name ? name : "unknown";
    node->timeout_ms = timeout_ms;
    node->timeout_cb = timeout_cb;
    node->data = arg;

    OS_THREAD_MUTEX_LOCK(wd->mutex);

    list_add_tail(&wd->list, &node->listnode);

    OS_THREAD_MUTEX_UNLOCK(wd->mutex);
    return node;
}

int swwatchdog_start(swwatch_t node)
{
    struct swwatchdog *wd = g_watchdog;
    if (wd == NULL)
        return -1;

    OS_THREAD_MUTEX_LOCK(wd->mutex);

    if (!swwatchdog_check_node_l(node)) {
        OS_LOGE(LOG_TAG, "Can't find node[%p] in watchdog list", node);
        OS_THREAD_MUTEX_UNLOCK(wd->mutex);
        return -1;
    }

    if (!node->active) {
        node->active = true;
        wd->active_num++;
        OS_THREAD_COND_SIGNAL(wd->cond);
    }

    OS_THREAD_MUTEX_UNLOCK(wd->mutex);
    return 0;
}

int swwatchdog_feed(swwatch_t node)
{

    struct swwatchdog *wd = g_watchdog;
    if (wd == NULL)
        return -1;

    OS_THREAD_MUTEX_LOCK(wd->mutex);

    if (!swwatchdog_check_node_l(node)) {
        OS_LOGE(LOG_TAG, "Can't find node[%p] in watchdog list", node);
        OS_THREAD_MUTEX_UNLOCK(wd->mutex);
        return -1;
    }

    if (node->active)
        node->count = 0;

    OS_THREAD_MUTEX_UNLOCK(wd->mutex);
    return 0;
}

int swwatchdog_stop(swwatch_t node)
{
    struct swwatchdog *wd = g_watchdog;
    if (wd == NULL)
        return -1;

    OS_THREAD_MUTEX_LOCK(wd->mutex);

    if (!swwatchdog_check_node_l(node)) {
        OS_LOGE(LOG_TAG, "Can't find node[%p] in watchdog list", node);
        OS_THREAD_MUTEX_UNLOCK(wd->mutex);
        return -1;
    }

    if (node->active) {
        node->active = false;
        wd->active_num--;
    }
    node->count = 0;

    OS_THREAD_MUTEX_UNLOCK(wd->mutex);
    return 0;
}

void swwatchdog_destroy(swwatch_t node)
{
    struct swwatchdog *wd = g_watchdog;
    struct listnode *item;

    if (wd == NULL)
        return;

    OS_THREAD_MUTEX_LOCK(wd->mutex);

    list_for_each(item, &wd->list) {
        struct swwatchdog_node *temp = node_to_item(item, struct swwatchdog_node, listnode);
        if (temp == node) {
            if (temp->active)
                wd->active_num--;

            list_remove(item);
            OS_FREE(temp);
            break;
        }
    }

    OS_THREAD_MUTEX_UNLOCK(wd->mutex);
}
