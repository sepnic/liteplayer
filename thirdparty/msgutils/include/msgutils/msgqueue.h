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

#ifndef __MSGQUEUE_H__
#define __MSGQUEUE_H__

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct msgqueue *mqueue_t;
typedef struct msgqueue *mqueueset_t;

mqueue_t mqueue_create(unsigned int msg_size, unsigned int msg_count);

int mqueue_destroy(mqueue_t queue);

int mqueue_reset(mqueue_t queue);

int mqueue_send(mqueue_t queue, char *msg, unsigned int timeout_ms);

int mqueue_receive(mqueue_t queue, char *msg, unsigned int timeout_ms);

unsigned int mqueue_count_available(mqueue_t queue);

unsigned int mqueue_count_filled(mqueue_t queue);

/*
 * Queue sets provide a mechanism to allow a task to block (pend) on a read
 * operation from multiple queues or simultaneously.
 *
 * See https://www.freertos.org/xQueueCreateSet.html for an example using this
 * function.
 *
 * A queue set must be explicitly created using a call to mqueueset_create()
 * before it can be used.  Once created, standard FreeRTOS queues can be added
 * to the set using calls to mqueueset_add_queue().
 * mqueueset_select_queue() is used to determine which, if any of the queues
 * contained in the set is in a state where a queue read operation would be
 * successful.
 *
 * Note 1:  See the documentation on https://wwwFreeRTOS.org/RTOS-queue-sets.html
 * for reasons why queue sets are very rarely needed in practice as there are
 * simpler methods of blocking on multiple objects.
 *
 * Note 2:  A receive (in the case of a queue) operation must not be performed
 * on a member of a queue set unless a call to mqueueset_select_queue() has first
 * returned a handle to that set member.
 *
 * Note 3:  @msg_count specifies the maximum number of events that can be queued
 * at once. To be absolutely certain that events are not lost @msg_count should
 * be set to the total sum of the length of the queues added to the set.
 *
 * Note 4:  If the queue could not be successfully added to the queue set because
 * it is already a member of a different queue set.
 *
 * Note 5:  If the queue could not be successfully remove from the queue set
 * because it is not in the queue set, or the queue is not empty.
 *
 * Usage example:
 *
 * // Create the queues that will be contained in the set.
 * queue1 = mqueue_create(ITEM_SIZE_QUEUE_1, QUEUE_LENGTH_1);
 * queue2 = mqueue_create(ITEM_SIZE_QUEUE_2, QUEUE_LENGTH_2);
 *
 * // Create the queue set large enough to hold an event for every space
 * // in every queue that is to be added to the set.
 * queueset = mqueueset_create(QUEUE_LENGTH_1 + QUEUE_LENGTH_2);
 *
 * // Add the queues to the set. Reading from these queues can only be
 * // performed after a call to mqueueset_select_queue() has returned the queue
 * // handle.
 * mqueueset_add_queue(queueset, queue1);
 * mqueueset_add_queue(queueset, queue2);
 *
 * for ( ;; ) {
 *     // Block to wait for something to be available from the queues that
 *     // have been added to the set. Don't block longer than 200ms.
 *     activemember = mqueueset_select_queue(queueset, 200);
 *
 *     if (activemember == queue1) {
 *         mqueue_receive(activemember, &queue1_msg, 0);
 *         // todo: process queue1_msg
 *     }
 *     else if(activemember == queue2) {
 *         mqueue_receive(activemember, &queue2_msg, 0);
 *         // todo: process queue2_msg
 *     }
 *     else {
 *         // The 200ms block time expired without an queue being ready.
 *     }
 * }
 *
 */
mqueueset_t mqueueset_create(unsigned int msg_count);

int mqueueset_destroy(mqueueset_t set);

int mqueueset_add_queue(mqueueset_t set, mqueue_t queue);

int mqueueset_remove_queue(mqueueset_t set, mqueue_t queue);

mqueue_t mqueueset_select_queue(mqueueset_t set, unsigned int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* __MSGQUEUE_H__ */
