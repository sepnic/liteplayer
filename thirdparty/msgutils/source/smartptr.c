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
#include "msgutils/smartptr.h"

#define LOG_TAG "smartptr"

struct smartptr_ctr_block {
    int refs_cnt;
    void (*free_cb)(void *ptr);
    os_mutex_t mutex;
};

#define SMARTPTR_GET_CTRBLOCK(ptr) \
    ((struct smartptr_ctr_block *)((char *)(ptr) - sizeof(struct smartptr_ctr_block)))

void *smartptr_new(size_t size, void (*free_cb)(void *ptr))
{
    void *ptr = OS_MALLOC(size + sizeof(struct smartptr_ctr_block));
    if (ptr != NULL) {
        struct smartptr_ctr_block *block = (struct smartptr_ctr_block *)ptr;

        block->refs_cnt = 1;
        block->free_cb = free_cb;
        block->mutex = OS_THREAD_MUTEX_CREATE();
        if (block->mutex == NULL) {
            OS_FREE(ptr);
            return NULL;
        }

        return (void *)((char *)ptr + sizeof(struct smartptr_ctr_block));
    }
    return NULL;
}

void smartptr_get(void *ptr)
{
    struct smartptr_ctr_block *block = SMARTPTR_GET_CTRBLOCK(ptr);

    OS_THREAD_MUTEX_LOCK(block->mutex);

    block->refs_cnt++;

    OS_THREAD_MUTEX_UNLOCK(block->mutex);
}

void smartptr_put(void *ptr)
{
    struct smartptr_ctr_block *block = SMARTPTR_GET_CTRBLOCK(ptr);

    OS_THREAD_MUTEX_LOCK(block->mutex);

    block->refs_cnt--;

    if (block->refs_cnt <= 0) {
        block->free_cb(ptr);
        OS_THREAD_MUTEX_UNLOCK(block->mutex);
        OS_THREAD_MUTEX_DESTROY(block->mutex);
        OS_FREE(block);
        return;
    }

    OS_THREAD_MUTEX_UNLOCK(block->mutex);
}

#if defined(ENABLE_SMARTPTR_DETECT)
#include "msgutils/common_list.h"
#include "msgutils/os_time.h"
#include "msgutils/os_logger.h"

struct smartptr_node {
    void *ptr;
    size_t user_size;
    size_t real_size;
    int refs_cnt;
    const char *file;
    const char *func;
    int line;
#if defined(OS_FREERTOS)
    unsigned long when;
#else
    struct os_realtime when;
#endif

    struct listnode listnode;
};

struct smartptr_info {
    struct listnode list;

    long new_cnt;
    long delete_cnt;
    size_t cur_used;
    size_t max_used;

    os_mutex_t mutex;
};

OS_MUTEX_DECLARE(g_ptrinfo_mutex);
static struct smartptr_info *g_ptrinfo = NULL;

static char *file_name(const char *filepath)
{
    char *filename = (char *)filepath;

    if (filename != NULL) {
        unsigned int len = strlen(filepath);
        if (len > 0) {
            filename += len - 1;
            while (filename > filepath) {
                if ((*filename == '\\') || *filename == '/') {
                    filename++;
                    break;
                }
                filename--;
            }
        }
    }
    return filename;
}

static void smartptr_node_debug(struct smartptr_node *node, const char *info)
{
#if defined(OS_FREERTOS)
    OS_LOGW(LOG_TAG, "> %s: ptr=[%p], user_size=[%d], real_size=[%d], refs_cnt=[%d], "
           "created by [%s:%d], at [%lu]",
           info, node->ptr, node->user_size, node->real_size, node->refs_cnt,
           node->func, node->line, node->when);

#else
    OS_LOGW(LOG_TAG, "> %s: ptr=[%p], user_size=[%d], real_size=[%d], refs_cnt=[%d], "
           "created by [%s:%s:%d], at [%04d%02d%02d-%02d%02d%02d:%03d]",
           info, node->ptr, node->user_size, node->real_size, node->refs_cnt,
           file_name(node->file), node->func, node->line,
           node->when.year, node->when.mon, node->when.day,
           node->when.hour, node->when.min, node->when.sec, node->when.msec);
#endif
}

static struct smartptr_info *smartptr_detect_init()
{
    if (g_ptrinfo == NULL) {
        if (g_ptrinfo_mutex != NULL)
            OS_THREAD_MUTEX_LOCK(g_ptrinfo_mutex);

        if (g_ptrinfo == NULL) {
            g_ptrinfo = OS_CALLOC(1, sizeof(struct smartptr_info));
            if (g_ptrinfo == NULL) {
                OS_LOGE(LOG_TAG, "Failed to alloc smartptr_info, abort smartptr detect");
                if (g_ptrinfo_mutex != NULL)
                    OS_THREAD_MUTEX_UNLOCK(g_ptrinfo_mutex);
                return NULL;
            }

            g_ptrinfo->mutex = OS_THREAD_MUTEX_CREATE();
            if (g_ptrinfo->mutex == NULL) {
                OS_LOGE(LOG_TAG, "Failed to alloc smartptr_mutex, abort smartptr detect");
                goto error;
            }

            g_ptrinfo->new_cnt = 0;
            g_ptrinfo->delete_cnt = 0;
            list_init(&g_ptrinfo->list);
        }

        if (g_ptrinfo_mutex != NULL)
            OS_THREAD_MUTEX_UNLOCK(g_ptrinfo_mutex);
    }

    return g_ptrinfo;

error:
    if (g_ptrinfo->mutex != NULL)
        OS_THREAD_MUTEX_DESTROY(g_ptrinfo->mutex);

    OS_FREE(g_ptrinfo);
    g_ptrinfo = NULL;

    if (g_ptrinfo_mutex != NULL)
        OS_THREAD_MUTEX_UNLOCK(g_ptrinfo_mutex);
    return NULL;
}

void *smartptr_new_debug(size_t size, void (*free_cb)(void *ptr),
                           const char *file, const char *func, int line)
{
    void *ptr = smartptr_new(size, free_cb);
    struct smartptr_node *node;
    struct smartptr_info *info = smartptr_detect_init();

    if (ptr == NULL) {
        OS_LOGF(LOG_TAG, "%s:%s:%d: failed to alloc smartptr", file_name(file), func, line);
        return NULL;
    }

    if (info != NULL) {
        node = OS_MALLOC(sizeof(struct smartptr_node));
        if (node != NULL) {
            node->ptr = ptr;
            node->user_size = size;
            node->real_size = size + sizeof(struct smartptr_ctr_block);
            node->refs_cnt = SMARTPTR_GET_CTRBLOCK(ptr)->refs_cnt;
            node->file = file;
            node->func = func;
            node->line = line;
#if defined(OS_FREERTOS)
            node->when = (unsigned long)(OS_MONOTONIC_USEC()/1000);
#else
            OS_TIMESTAMP_TO_LOCAL(&node->when);
#endif

            //smartptr_node_debug(node, "New");

            OS_THREAD_MUTEX_LOCK(info->mutex);

            list_add_tail(&info->list, &node->listnode);
            info->new_cnt++;
            info->cur_used += node->real_size;

            OS_THREAD_MUTEX_UNLOCK(info->mutex);
        }
    }

    return ptr;
}

void smartptr_get_debug(void *ptr, const char *file, const char *func, int line)
{
    struct smartptr_info *info = smartptr_detect_init();
    struct listnode *item;

    if (info != NULL) {
        struct smartptr_node *node;
        bool found = false;

        OS_THREAD_MUTEX_LOCK(info->mutex);

        list_for_each_reverse(item, &info->list) {
            node = node_to_item(item, struct smartptr_node, listnode);
            if (node->ptr == ptr) {
                found = true;
                break;
            }
        }

        if (found) {
            node->refs_cnt = SMARTPTR_GET_CTRBLOCK(ptr)->refs_cnt + 1;
            //smartptr_node_debug(node, "Get");
        }
        else {
            OS_LOGF(LOG_TAG, "%s:%s:%d: failed to find ptr[%p] in list, will overflow",
                   file_name(file), func, line, ptr);
        }

        OS_THREAD_MUTEX_UNLOCK(info->mutex);
    }

    smartptr_get(ptr);
}

void smartptr_put_debug(void *ptr, const char *file, const char *func, int line)
{
    struct smartptr_info *info = smartptr_detect_init();
    struct listnode *item;

    if (info != NULL) {
        struct smartptr_node *node;
        bool found = false;

        OS_THREAD_MUTEX_LOCK(info->mutex);

        list_for_each_reverse(item, &info->list) {
            node = node_to_item(item, struct smartptr_node, listnode);
            if (node->ptr == ptr) {
                found = true;
                break;
            }
        }

        if (found) {
            node->refs_cnt = SMARTPTR_GET_CTRBLOCK(ptr)->refs_cnt - 1;
            //smartptr_node_debug(node, "Put");
            if (node->refs_cnt == 0) {
                info->delete_cnt++;
                info->cur_used -= node->real_size;
                list_remove(item);
                OS_FREE(node);
            }
        }
        else {
            OS_LOGF(LOG_TAG, "%s:%s:%d: failed to find ptr[%p] in list, will overflow",
                   file_name(file), func, line, ptr);
        }

        OS_THREAD_MUTEX_UNLOCK(info->mutex);
    }

    smartptr_put(ptr);
}

void smartptr_dump_debug()
{
    struct smartptr_info *info  = smartptr_detect_init();
    struct smartptr_node *node;
    struct listnode *item;

    if (info != NULL) {
        OS_LOGW(LOG_TAG, ">>");
        OS_LOGW(LOG_TAG, "++++++++++++++++++++ SMARTPTR DETECT ++++++++++++++++++++");

        OS_THREAD_MUTEX_LOCK(info->mutex);

        list_for_each(item, &info->list) {
            node = node_to_item(item, struct smartptr_node, listnode);
            smartptr_node_debug(node, "Dump");
        }

        OS_LOGW(LOG_TAG, "Summary: new [%d] blocks, delete [%d] blocks, current use [%d] Bytes",
               info->new_cnt, info->delete_cnt, info->cur_used);

        OS_THREAD_MUTEX_UNLOCK(info->mutex);

        OS_LOGW(LOG_TAG, "-------------------- SMARTPTR DETECT --------------------");
        OS_LOGW(LOG_TAG, "<<");
    }
}
#endif
