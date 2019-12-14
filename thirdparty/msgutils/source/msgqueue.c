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
#include <stdbool.h>

#include "msgutils/os_memory.h"
#include "msgutils/os_thread.h"
#include "msgutils/os_logger.h"
#include "msgutils/common_list.h"
#include "msgutils/msgqueue.h"

#define LOG_TAG "msgqueue"

struct msgqueue {
    char *head;  /**< Head pointer */
    char *read;  /**< Read pointer */
    char *write; /**< Write pointer */
    char *tail;  /**< Tail pointer */

    unsigned int element_size; /**< Size of msg element */
    unsigned int element_count;/**< Number of total slots */
    unsigned int filled_count; /**< Number of filled slots */

    os_cond_t can_read;
    os_cond_t can_write;
    os_mutex_t lock;

    bool is_set;            /**< Whether is queue-set */
    struct listnode list;   /**< List node for queue, list head for queue-set */
    mqueueset_t parent_set; /**< Parent queue-set pointer */
};

mqueue_t mqueue_create(unsigned int msg_size, unsigned int msg_count)
{
    struct msgqueue *queue = OS_CALLOC(1, sizeof(struct msgqueue));
    if (queue == NULL) {
        OS_LOGE(LOG_TAG, "Failed to allocate queue");
        return NULL;
    }

    queue->lock = OS_THREAD_MUTEX_CREATE();
    if (queue->lock == NULL) {
        OS_LOGE(LOG_TAG, "Failed to create queue mutex");
        goto error;
    }

    queue->can_read = OS_THREAD_COND_CREATE();
    if (queue->can_read == NULL) {
        OS_LOGE(LOG_TAG, "Failed to create queue cond");
        goto error;
    }

    queue->can_write = OS_THREAD_COND_CREATE();
    if (queue->can_write == NULL) {
        OS_LOGE(LOG_TAG, "Failed to create queue cond");
        goto error;
    }

    queue->head = OS_CALLOC(msg_count, msg_size);
    if (queue->head == NULL) {
        OS_LOGE(LOG_TAG, "Failed to allocate queue buffer");
        goto error;
    }

    queue->read = queue->head;
    queue->write = queue->head;
    queue->tail = queue->head + msg_size * msg_count;
    queue->element_size = msg_size;
    queue->element_count = msg_count;
    queue->filled_count = 0;
    queue->parent_set = NULL;
    queue->is_set = false;
    list_init(&queue->list);

    return queue;

error:
    if (queue->head != NULL)
        OS_FREE(queue->head);
    if (queue->can_write != NULL)
        OS_THREAD_COND_DESTROY(queue->can_write);
    if (queue->can_read != NULL)
        OS_THREAD_COND_DESTROY(queue->can_read);
    if (queue->lock != NULL)
        OS_THREAD_MUTEX_DESTROY(queue->lock);
    OS_FREE(queue);
    return NULL;
}

int mqueue_destroy(mqueue_t queue)
{
    if (mqueue_count_filled(queue) > 0) {
        OS_LOGE(LOG_TAG, "Can't destroy queue that isn't empty");
        return -1;
    }

    if (queue->parent_set != NULL) {
        OS_THREAD_MUTEX_LOCK(queue->parent_set->lock);
        list_remove(&queue->list);
        OS_THREAD_MUTEX_UNLOCK(queue->parent_set->lock);
    }

    OS_FREE(queue->head);
    OS_THREAD_COND_DESTROY(queue->can_write);
    OS_THREAD_COND_DESTROY(queue->can_read);
    OS_THREAD_MUTEX_DESTROY(queue->lock);
    OS_FREE(queue);
    return 0;
}

int mqueue_reset(mqueue_t queue)
{
    OS_THREAD_MUTEX_LOCK(queue->lock);

    if (queue->parent_set != NULL && mqueue_count_filled(queue) > 0) {
        OS_LOGE(LOG_TAG, "Can't reset queue that parent set isn't empty");
        OS_THREAD_MUTEX_UNLOCK(queue->lock);
        return -1;
    }

    queue->read = queue->head;
    queue->write = queue->head;
    queue->filled_count = 0;
    OS_THREAD_COND_SIGNAL(queue->can_write);

    OS_THREAD_MUTEX_UNLOCK(queue->lock);
    return 0;
}

static void mqueue_copy_msg(mqueue_t queue, char *msg)
{
    memcpy(queue->write, msg, queue->element_size);
    queue->filled_count++;
    queue->write += queue->element_size;
    if (queue->write >= queue->tail)
        queue->write = queue->head;
}

int mqueue_send(mqueue_t queue, char *msg, unsigned int timeout_ms)
{
    int ret = -1;

    OS_THREAD_MUTEX_LOCK(queue->lock);

    if (mqueue_count_available(queue) == 0) {
        if (timeout_ms == 0)
            goto write_done;
        else
            OS_THREAD_COND_TIMEDWAIT(queue->can_write, queue->lock, timeout_ms*1000);
    }

    if (mqueue_count_available(queue) > 0) {
        ret = 0;

        if (queue->parent_set) {
            mqueueset_t set = queue->parent_set;

            OS_THREAD_MUTEX_LOCK(set->lock);

            if (mqueue_count_available(set) > 0) {
                mqueue_copy_msg(queue, msg);
                mqueue_copy_msg(set, (char *)&queue);
                OS_THREAD_COND_SIGNAL(set->can_read);
            }
            else {
                OS_LOGE(LOG_TAG, "Failed to send msg to queue that parent set is full");
                ret = -1;
            }

            OS_THREAD_MUTEX_UNLOCK(set->lock);
        }
        else {
            mqueue_copy_msg(queue, msg);
        }
    }

write_done:
    if (ret == 0)
        OS_THREAD_COND_SIGNAL(queue->can_read);
    else
        OS_LOGE(LOG_TAG, "Failed to send msg to full queue");

    OS_THREAD_MUTEX_UNLOCK(queue->lock);
    return ret;
}

int mqueue_receive(mqueue_t queue, char *msg, unsigned int timeout_ms)
{
    int ret = -1;

    OS_THREAD_MUTEX_LOCK(queue->lock);

    if (mqueue_count_filled(queue) == 0) {
        if (timeout_ms == 0)
            goto read_done;
        else
            OS_THREAD_COND_TIMEDWAIT(queue->can_read, queue->lock, timeout_ms*1000);
    }

    if (mqueue_count_filled(queue) > 0) {
        memcpy(msg, queue->read, queue->element_size);
        queue->filled_count--;
        queue->read += queue->element_size;
        if (queue->read >= queue->tail)
            queue->read = queue->head;
        ret = 0;
    }

read_done:
    if (ret == 0)
        OS_THREAD_COND_SIGNAL(queue->can_write);

    OS_THREAD_MUTEX_UNLOCK(queue->lock);
    return ret;
}

unsigned int mqueue_count_available(mqueue_t queue)
{
    return queue->element_count - queue->filled_count;
}

unsigned int mqueue_count_filled(mqueue_t queue)
{
    return queue->filled_count;
}

mqueueset_t mqueueset_create(unsigned int msg_count)
{
    struct msgqueue *queue = mqueue_create(sizeof(mqueue_t), msg_count);
    if (queue == NULL) {
        OS_LOGE(LOG_TAG, "Failed to allocate queue set");
        return NULL;
    }

    queue->is_set = true;
    return queue;
}

int mqueueset_destroy(mqueueset_t set)
{
    struct msgqueue *queue = NULL;
    struct listnode *item, *tmp;

    list_for_each_safe(item, tmp, &set->list) {
        queue = node_to_item(item, struct msgqueue, list);
        list_remove(&queue->list);

        OS_THREAD_MUTEX_LOCK(queue->lock);
        queue->parent_set = NULL;
        OS_THREAD_MUTEX_UNLOCK(queue->lock);
    }

    return mqueue_destroy(set);
}

int mqueueset_add_queue(mqueueset_t set, mqueue_t queue)
{
    if (queue->parent_set != NULL) {
        OS_LOGE(LOG_TAG, "Can't add queue to more than one set");
        return -1;
    }

    if (queue->is_set) {
        OS_LOGE(LOG_TAG, "Can't add a set to another set");
        return -1;
    }

    OS_THREAD_MUTEX_LOCK(queue->lock);

    if (mqueue_count_filled(queue) > 0) {
        OS_LOGE(LOG_TAG, "Can't add queue that isn't empty");
        OS_THREAD_MUTEX_UNLOCK(queue->lock);
        return -1;
    }

    queue->parent_set = set;

    OS_THREAD_MUTEX_LOCK(set->lock);
    list_add_tail(&set->list, &queue->list);
    OS_THREAD_MUTEX_UNLOCK(set->lock);

    OS_THREAD_MUTEX_UNLOCK(queue->lock);

    return 0;
}

int mqueueset_remove_queue(mqueueset_t set, mqueue_t queue)
{
    if (queue->parent_set != set) {
        OS_LOGE(LOG_TAG, "Can't remove queue that isn't a member of the set");
        return -1;
    }

    OS_THREAD_MUTEX_LOCK(queue->lock);

    if (mqueue_count_filled(queue) > 0) {
        OS_LOGE(LOG_TAG, "Can't remove queue that isn't empty");
        OS_THREAD_MUTEX_UNLOCK(queue->lock);
        return -1;
    }

    queue->parent_set = NULL;

    OS_THREAD_MUTEX_LOCK(set->lock);
    list_remove(&queue->list);
    OS_THREAD_MUTEX_UNLOCK(set->lock);

    OS_THREAD_MUTEX_UNLOCK(queue->lock);

    return 0;
}

mqueue_t mqueueset_select_queue(mqueueset_t set, unsigned int timeout_ms)
{
    mqueue_t queue = NULL;
    (void) mqueue_receive(set, (char *)&queue, timeout_ms);
    return queue;
}
