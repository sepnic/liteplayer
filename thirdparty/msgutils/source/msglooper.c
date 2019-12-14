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

#include "sw_watchdog.h"
#include "msgutils/common_list.h"
#include "msgutils/os_thread.h"
#include "msgutils/os_time.h"
#include "msgutils/os_memory.h"
#include "msgutils/os_logger.h"
#include "msgutils/msglooper.h"

#define LOG_TAG "msglooper"

#define DEFAULT_LOOPER_PRIORITY  OS_THREAD_PRIO_NORMAL
#define DEFAULT_LOOPER_STACKSIZE 1024

struct msglooper {
    struct listnode msg_list;
    size_t msg_count;
    message_handle_cb msg_handle;
    message_free_cb msg_free;
    os_mutex_t msg_mutex;
    os_cond_t msg_cond;

    os_thread_t thread_id;
    const char *thread_name;
    struct os_threadattr thread_attr;
    bool thread_exit;
    os_mutex_t thread_mutex;

    bool watchdog_enable;
    swwatch_t watchdog_node;
};

struct message_node {
    // This must be the first member of message_node, as users of this structure
    // will cast a message to message_node pointer in contexts where it's known
    // the message references a message_node
    struct message msg;

    unsigned long long when;
    unsigned long long timeout;
    struct listnode listnode;
};

static void mlooper_free_msgnode(mlooper_t looper, struct message_node *node)
{
    struct message *msg = &node->msg;

    if (msg->free_cb != NULL)
        msg->free_cb(msg);
    else if (looper->msg_free != NULL)
        looper->msg_free(msg);
    else if (msg->data != NULL)
        OS_LOGW(LOG_TAG, "[%s]: Forget to free message data: what=[%d], memory leak?",
                looper->thread_name, msg->what);

    if (msg->notify_cb != NULL)
        msg->notify_cb(msg, MESSAGE_DESTROY);

    OS_FREE(msg);
}

static void mlooper_clear_msglist(mlooper_t looper)
{
    struct message_node *node = NULL;
    struct listnode *item, *tmp;

    OS_THREAD_MUTEX_LOCK(looper->msg_mutex);

    list_for_each_safe(item, tmp, &looper->msg_list) {
        node = node_to_item(item, struct message_node, listnode);
        list_remove(item);
        mlooper_free_msgnode(looper, node);
    }

    looper->msg_count = 0;

    OS_THREAD_MUTEX_UNLOCK(looper->msg_mutex);
}

static void *mlooper_thread_entry(void *arg)
{
    struct msglooper *looper = (struct msglooper *)arg;
    struct message_node *node = NULL;
    struct message *msg = NULL;
    struct listnode *front = NULL;
    unsigned long long now;

    OS_LOGD(LOG_TAG, "[%s]: Entry looper thread: thread_id=[%p]", looper->thread_name, looper->thread_id);

    OS_THREAD_SET_NAME(looper->thread_id, looper->thread_name);

    while (!looper->thread_exit) {
        {
            OS_THREAD_MUTEX_LOCK(looper->msg_mutex);

            while (list_empty(&looper->msg_list) && !looper->thread_exit)
                OS_THREAD_COND_WAIT(looper->msg_cond, looper->msg_mutex);

            if (looper->thread_exit) {
                OS_THREAD_MUTEX_UNLOCK(looper->msg_mutex);
                break;
            }

            front = list_head(&looper->msg_list);
            node = node_to_item(front, struct message_node, listnode);
            msg = &node->msg;

            now = OS_MONOTONIC_USEC();
            if (node->when > now) {
                unsigned long wait = node->when - now;
                OS_LOGV(LOG_TAG, "[%s]: Waiting message: what=[%d], wait=[%lums]",
                        looper->thread_name, msg->what, wait/1000);
                OS_THREAD_COND_TIMEDWAIT(looper->msg_cond, looper->msg_mutex, wait);
                msg = NULL;
            }
            else {
                list_remove(front);
                looper->msg_count--;
            }

            OS_THREAD_MUTEX_UNLOCK(looper->msg_mutex);
        }

        if (msg != NULL) {
            if (node->timeout > 0 && node->timeout < now) {
                OS_LOGE(LOG_TAG, "[%s]: Timeout, discard message: what=[%d]", looper->thread_name, msg->what);
                if (msg->notify_cb != NULL)
                    msg->notify_cb(msg, MESSAGE_TIMEOUT);
            }
            else {
                if (looper->watchdog_enable)
                    swwatchdog_start(looper->watchdog_node);

                if (msg->notify_cb != NULL)
                    msg->notify_cb(msg, MESSAGE_RUNNING);

                if (msg->handle_cb != NULL)
                    msg->handle_cb(msg);
                else if (looper->msg_handle != NULL)
                    looper->msg_handle(msg);
                else
                    OS_LOGW(LOG_TAG, "[%s]: No message handler: what=[%d]", looper->thread_name, msg->what);

                if (msg->notify_cb != NULL)
                    msg->notify_cb(msg, MESSAGE_COMPLETED);

                if (looper->watchdog_enable)
                    swwatchdog_stop(looper->watchdog_node);
            }

            mlooper_free_msgnode(looper, node);
        }
    }

    mlooper_clear_msglist(looper);

    OS_LOGD(LOG_TAG, "[%s]: Leave looper thread: thread_id=[%p]", looper->thread_name, looper->thread_id);
    return NULL;
}

mlooper_t mlooper_create(struct os_threadattr *attr, message_handle_cb handle_cb, message_free_cb free_cb)
{
    struct msglooper *looper = OS_CALLOC(1, sizeof(struct msglooper));
    if (looper == NULL) {
        OS_LOGE(LOG_TAG, "Failed to allocate looper");
        return NULL;
    }

    looper->msg_mutex = OS_THREAD_MUTEX_CREATE();
    if (looper->msg_mutex == NULL) {
        OS_LOGE(LOG_TAG, "Failed to create msg_mutex");
        goto error;
    }

    looper->msg_cond = OS_THREAD_COND_CREATE();
    if (looper->msg_cond == NULL) {
        OS_LOGE(LOG_TAG, "Failed to create msg_cond");
        goto error;
    }

    looper->thread_mutex = OS_THREAD_MUTEX_CREATE();
    if (looper->thread_mutex == NULL) {
        OS_LOGE(LOG_TAG, "Failed to create thread_mutex");
        goto error;
    }

    list_init(&looper->msg_list);
    looper->msg_count = 0;
    looper->msg_handle = handle_cb;
    looper->msg_free = free_cb;
    looper->thread_name = (attr && attr->name) ? OS_STRDUP(attr->name) : OS_STRDUP("looper");
    looper->thread_exit = true;
    looper->thread_attr.name = looper->thread_name;
    if (attr != NULL) {
        looper->thread_attr.priority = attr->priority;
        looper->thread_attr.stacksize = attr->stacksize > 0 ? attr->stacksize : DEFAULT_LOOPER_STACKSIZE;
        looper->thread_attr.joinable = true; // force joinalbe, wait exit when mlooper_stop
    }
    else {
        looper->thread_attr.priority = DEFAULT_LOOPER_PRIORITY;
        looper->thread_attr.stacksize = DEFAULT_LOOPER_STACKSIZE;
        looper->thread_attr.joinable = true;
    }

    return looper;

error:
    if (looper->thread_mutex != NULL)
        OS_THREAD_MUTEX_DESTROY(looper->thread_mutex);
    if (looper->msg_cond != NULL)
        OS_THREAD_COND_DESTROY(looper->msg_cond);
    if (looper->msg_mutex != NULL)
        OS_THREAD_MUTEX_DESTROY(looper->msg_mutex);
    OS_FREE(looper);
    return NULL;
}

int mlooper_start(mlooper_t looper)
{
    int ret = 0;
    OS_THREAD_MUTEX_LOCK(looper->thread_mutex);

    if (looper->thread_exit) {
        looper->thread_exit = false;
        looper->thread_id = OS_THREAD_CREATE(&(looper->thread_attr), mlooper_thread_entry, looper);
        if (looper->thread_id == NULL) {
            OS_LOGE(LOG_TAG, "[%s]: Failed to run thread looper", looper->thread_name);
            looper->thread_exit = true;
            ret = -1;
        }
    }

    OS_THREAD_MUTEX_UNLOCK(looper->thread_mutex);
    return ret;
}

int mlooper_post_message(mlooper_t looper, struct message *msg)
{
    return mlooper_post_message_delay(looper, msg, 0);
}

int mlooper_post_message_front(mlooper_t looper, struct message *msg)
{
    unsigned long long now = OS_MONOTONIC_USEC();
    struct message_node *node = (struct message_node *)msg;
    struct message_node *temp;

    node->when = now;
    if (msg->timeout_ms > 0)
        node->timeout = now + msg->timeout_ms * 1000;

    {
        OS_THREAD_MUTEX_LOCK(looper->msg_mutex);

        if (!list_empty(&looper->msg_list)) {
            temp = node_to_item(list_head(&looper->msg_list), struct message_node, listnode);
            node->when = now < temp->when ? now : temp->when;
        }

        list_add_head(&looper->msg_list, &node->listnode);
        looper->msg_count++;

        OS_THREAD_COND_SIGNAL(looper->msg_cond);

        OS_THREAD_MUTEX_UNLOCK(looper->msg_mutex);
    }

    if (msg->notify_cb != NULL)
        msg->notify_cb(msg, MESSAGE_PENDING);
    return 0;
}

int mlooper_post_message_delay(mlooper_t looper, struct message *msg, unsigned long msec)
{
    unsigned long long now = OS_MONOTONIC_USEC();
    struct message_node *node = (struct message_node *)msg;
    struct message_node *temp;
    struct listnode *item;

    node->when = now + msec * 1000;
    if (msg->timeout_ms > 0) {
        if (msg->timeout_ms < msec) {
            OS_LOGW(LOG_TAG, "[%s]: Invalid timeout: timeout_ms < delay_ms", looper->thread_name);
            node->timeout = 0;
        }
        else {
            node->timeout = now + msg->timeout_ms * 1000;
        }
    }

    {
        OS_THREAD_MUTEX_LOCK(looper->msg_mutex);

        list_for_each_reverse(item, &looper->msg_list) {
            temp = node_to_item(item, struct message_node, listnode);
            if (node->when >= temp->when) {
                list_add_after(&temp->listnode, &node->listnode);
                looper->msg_count++;
                break;
            }
        }

        if (item == &looper->msg_list) {
            list_add_head(&looper->msg_list, &node->listnode);
            looper->msg_count++;
        }

        OS_THREAD_COND_SIGNAL(looper->msg_cond);

        OS_THREAD_MUTEX_UNLOCK(looper->msg_mutex);
    }

    if (msg->notify_cb != NULL)
        msg->notify_cb(msg, MESSAGE_PENDING);
    return 0;
}

int mlooper_remove_message(mlooper_t looper, int what)
{
    struct message_node *node = NULL;
    struct listnode *item, *tmp;

    OS_THREAD_MUTEX_LOCK(looper->msg_mutex);

    list_for_each_safe(item, tmp, &looper->msg_list) {
        node = node_to_item(item, struct message_node, listnode);
        if (node->msg.what == what) {
            list_remove(item);
            mlooper_free_msgnode(looper, node);
            looper->msg_count--;
        }
    }

    OS_THREAD_MUTEX_UNLOCK(looper->msg_mutex);
    return 0;
}

int mlooper_remove_message_if(mlooper_t looper, message_match_cb match_cb)
{
    struct message_node *node = NULL;
    struct listnode *item, *tmp;

    OS_THREAD_MUTEX_LOCK(looper->msg_mutex);

    list_for_each_safe(item, tmp, &looper->msg_list) {
        node = node_to_item(item, struct message_node, listnode);
        if (match_cb(&node->msg)) {
            list_remove(item);
            mlooper_free_msgnode(looper, node);
            looper->msg_count--;
        }
    }

    OS_THREAD_MUTEX_UNLOCK(looper->msg_mutex);
    return 0;
}

size_t mlooper_message_count(mlooper_t looper)
{
    return looper->msg_count;
}

void mlooper_dump(mlooper_t looper)
{
    struct message_node *node = NULL;
    struct listnode *item;
    int i = 0;

    OS_THREAD_MUTEX_LOCK(looper->msg_mutex);

    OS_LOGI(LOG_TAG, "Dump looper thread:");
    OS_LOGI(LOG_TAG, " > thread_name=[%s]", looper->thread_name);
    OS_LOGI(LOG_TAG, " > thread_exit=[%s]", looper->thread_exit ? "true" : "false");
    OS_LOGI(LOG_TAG, " > message_count=[%d]", looper->msg_count);

    if (looper->msg_count != 0) {
        OS_LOGI(LOG_TAG, " > message list info:");

        list_for_each(item, &looper->msg_list) {
            node = node_to_item(item, struct message_node, listnode);
            i++;
            OS_LOGI(LOG_TAG, "   > [%d]: what=[%d], arg1=[%d], arg2=[%d], when=[%llu]",
                    i, node->msg.what, node->msg.arg1, node->msg.arg2, node->when);
        }
    }

    OS_THREAD_MUTEX_UNLOCK(looper->msg_mutex);
}

void mlooper_stop(mlooper_t looper)
{
    OS_THREAD_MUTEX_LOCK(looper->thread_mutex);

    if (!looper->thread_exit) {
        looper->thread_exit = true;
        OS_THREAD_COND_SIGNAL(looper->msg_cond);

        OS_THREAD_JOIN(looper->thread_id, NULL);
    }

    OS_THREAD_MUTEX_UNLOCK(looper->thread_mutex);
}

void mlooper_destroy(mlooper_t looper)
{
    mlooper_stop(looper);

    if (looper->watchdog_node != NULL)
        swwatchdog_destroy(looper->watchdog_node);

    OS_THREAD_MUTEX_DESTROY(looper->thread_mutex);
    OS_THREAD_COND_DESTROY(looper->msg_cond);
    OS_THREAD_MUTEX_DESTROY(looper->msg_mutex);

    OS_FREE(looper->thread_name);
    OS_FREE(looper);
}

int mlooper_enable_watchdog(mlooper_t looper, unsigned long long timeout_ms, void (*timeout_cb)(void *arg), void *arg)
{
    OS_THREAD_MUTEX_LOCK(looper->msg_mutex);

    if (looper->watchdog_node != NULL)
        swwatchdog_destroy(looper->watchdog_node);

    looper->watchdog_node = swwatchdog_create(looper->thread_name, timeout_ms, timeout_cb, arg);
    if (looper->watchdog_node == NULL) {
        looper->watchdog_enable = false;
        OS_THREAD_MUTEX_UNLOCK(looper->msg_mutex);
        return -1;
    }

    looper->watchdog_enable = true;

    OS_THREAD_MUTEX_UNLOCK(looper->msg_mutex);
    return 0;
}

void mlooper_disable_watchdog(mlooper_t looper)
{
    OS_THREAD_MUTEX_LOCK(looper->msg_mutex);

    if (looper->watchdog_enable) {
        looper->watchdog_enable = false;
        swwatchdog_destroy(looper->watchdog_node);
        looper->watchdog_node = NULL;
    }

    OS_THREAD_MUTEX_UNLOCK(looper->msg_mutex);
}

struct message *message_obtain(int what, int arg1, int arg2, void *data)
{
    struct message *msg = OS_CALLOC(1, sizeof(struct message_node));
    if (msg == NULL) {
        OS_LOGE(LOG_TAG, "Failed to allocate message");
        return NULL;
    }

    msg->what = what;
    msg->arg1 = arg1;
    msg->arg2 = arg2;
    msg->data = data;
    return msg;
}

struct message *message_obtain2(int what, int arg1, int arg2, void *data, unsigned long timeout_ms,
                                message_handle_cb handle_cb, message_free_cb free_cb, message_notify_cb notify_cb)
{
    struct message *msg = OS_CALLOC(1, sizeof(struct message_node));
    if (msg == NULL) {
        OS_LOGE(LOG_TAG, "Failed to allocate message");
        return NULL;
    }

    msg->what = what;
    msg->arg1 = arg1;
    msg->arg2 = arg2;
    msg->data = data;
    msg->timeout_ms = timeout_ms;
    msg->handle_cb = handle_cb;
    msg->free_cb = free_cb;
    msg->notify_cb = notify_cb;
    return msg;
}
