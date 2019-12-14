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

#include <stdbool.h>

#include "msgutils/common_list.h"
#include "msgutils/os_thread.h"
#include "msgutils/os_time.h"
#include "msgutils/os_logger.h"
#include "msgutils/os_memory.h"

#if defined(ENABLE_MEMORY_LEAK_DETECT) || defined(ENABLE_MEMORY_OVERFLOW_DETECT)

#define LOG_TAG "memory_detect"

#define MEMORY_BOUNDARY_SIZE    8
#define MEMORY_BOUNDARY_FLAG    '*'

struct mem_node {
    void *ptr;
    size_t size;
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

struct mem_info {
    struct listnode list;

    long malloc_count;
    long free_count;
    size_t cur_used;
    size_t max_used;

    os_mutex_t mutex;
};

OS_MUTEX_DECLARE(g_meminfo_mutex);
static struct mem_info *g_meminfo = NULL;

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

static void memory_node_debug(struct mem_node *node, const char *info)
{
#if defined(OS_FREERTOS)
    OS_LOGW(LOG_TAG, "> %s: ptr=[%p], size=[%d], created by [%s:%d], at [%lu]",
            info, node->ptr, node->size, node->func, node->line, node->when);

#else
    OS_LOGW(LOG_TAG, "> %s: ptr=[%p], size=[%d], "
           "created by [%s:%s:%d], at [%04d%02d%02d-%02d%02d%02d:%03d]",
           info, node->ptr, node->size,
           file_name(node->file), node->func, node->line,
           node->when.year, node->when.mon, node->when.day,
           node->when.hour, node->when.min, node->when.sec, node->when.msec);
#endif
}

#if defined(ENABLE_MEMORY_OVERFLOW_DETECT)
static void *memory_boundary_malloc(size_t size)
{
    void *ptr = NULL;
    size_t real_size = size;

    real_size += 2 * MEMORY_BOUNDARY_SIZE;
    ptr = malloc(real_size);

    if (ptr != NULL) {
        // fill upper boundary with specific flag
        memset(ptr,
               MEMORY_BOUNDARY_FLAG,
               MEMORY_BOUNDARY_SIZE);
        // fill lower boundary with specific flag
        memset((char *)ptr + real_size - MEMORY_BOUNDARY_SIZE,
               MEMORY_BOUNDARY_FLAG,
               MEMORY_BOUNDARY_SIZE);

        return (char *)ptr + MEMORY_BOUNDARY_SIZE;
    }
    return NULL;
}

static void memory_boundary_free(void *ptr)
{
    if (ptr != NULL) {
        void *temp = (void *) ((char *)ptr - MEMORY_BOUNDARY_SIZE);
        free(temp);
    }
}

static void memory_boundary_verify(struct mem_node *node)
{
    int i = 0;
    char *ptr;

    // check upper boundary
    ptr = (char *)node->ptr - MEMORY_BOUNDARY_SIZE;
    for (i = 0; i < MEMORY_BOUNDARY_SIZE; i++) {
        if (((char *)ptr)[i] != MEMORY_BOUNDARY_FLAG) {
            memory_node_debug(node, "Overflow at upper boundary");
            break;
        }
    }

    // check lower boundary
    ptr = (char *)node->ptr + node->size;
    for (i = 0; i < MEMORY_BOUNDARY_SIZE; i++) {
        if (((char *)ptr)[i] != MEMORY_BOUNDARY_FLAG) {
            memory_node_debug(node, "Overflow at lower boundary");
            break;
        }
    }
}
#endif

static struct mem_info *memory_detect_init()
{
    if (g_meminfo == NULL) {
        if (g_meminfo_mutex != NULL)
            OS_THREAD_MUTEX_LOCK(g_meminfo_mutex);

        if (g_meminfo == NULL) {
            g_meminfo = calloc(1, sizeof(struct mem_info));
            if (g_meminfo == NULL) {
                OS_LOGE(LOG_TAG, "Failed to alloc mem_info, abort memory detect");
                if (g_meminfo_mutex != NULL)
                    OS_THREAD_MUTEX_UNLOCK(g_meminfo_mutex);
                return NULL;
            }

            g_meminfo->mutex = OS_THREAD_MUTEX_CREATE();
            if (g_meminfo->mutex == NULL) {
                OS_LOGE(LOG_TAG, "Failed to alloc mem_mutex, abort memory detect");
                goto error;
            }

            g_meminfo->malloc_count = 0;
            g_meminfo->free_count = 0;
            g_meminfo->max_used = 0;
            list_init(&g_meminfo->list);
        }

        if (g_meminfo_mutex != NULL)
            OS_THREAD_MUTEX_UNLOCK(g_meminfo_mutex);
    }

    return g_meminfo;

error:
    if (g_meminfo->mutex != NULL)
        OS_THREAD_MUTEX_DESTROY(g_meminfo->mutex);

    free(g_meminfo);
    g_meminfo = NULL;

    if (g_meminfo_mutex != NULL)
        OS_THREAD_MUTEX_UNLOCK(g_meminfo_mutex);
    return NULL;
}

void *memory_detect_malloc(size_t size, const char *file, const char *func, int line)
{
    void *ptr;
    struct mem_node *node;
    struct mem_info *info = memory_detect_init();

#if defined(ENABLE_MEMORY_OVERFLOW_DETECT)
    ptr = memory_boundary_malloc(size);
#else
    ptr = malloc(size);
#endif
    if (ptr == NULL) {
        OS_LOGF(LOG_TAG, "%s:%s:%d: failed to alloc memory", file_name(file), func, line);
        return NULL;
    }

    if (info != NULL) {
        node = malloc(sizeof(struct mem_node));
        if (node != NULL) {
            node->ptr = ptr;
            node->size = size;
            node->file = file;
            node->func = func;
            node->line = line;
#if defined(OS_FREERTOS)
            node->when = (unsigned long)(OS_MONOTONIC_USEC()/1000);
#else
            OS_TIMESTAMP_TO_LOCAL(&node->when);
#endif

            //memory_node_debug(node, "Malloc");

            OS_THREAD_MUTEX_LOCK(info->mutex);

            list_add_tail(&info->list, &node->listnode);
            info->malloc_count++;
            info->cur_used += size;
            if (info->cur_used > info->max_used)
                info->max_used = info->cur_used;

            OS_THREAD_MUTEX_UNLOCK(info->mutex);
        }
    }

    return ptr;
}

void *memory_detect_calloc(size_t n, size_t size, const char *file, const char *func, int line)
{
    void *ptr = memory_detect_malloc(size * n, file, func, line);
    if (ptr != NULL)
        memset(ptr, 0x0, size * n);
    return ptr;
}

void *memory_detect_realloc(void *ptr, size_t size, const char *file, const char *func, int line)
{
    struct mem_info *info;
    struct listnode *item;

    if (ptr == NULL) {
        if (size > 0)
            return memory_detect_malloc(size, file, func, line);
        else
            return NULL;
    }

    if (size == 0) {
        memory_detect_free(ptr, file, func, line);
        return NULL;
    }

    info = memory_detect_init();

    if (info != NULL) {
        void *prev_ptr = NULL;
        size_t prev_size = 0;

        OS_THREAD_MUTEX_LOCK(info->mutex);

        list_for_each_reverse(item, &info->list) {
            struct mem_node *node = node_to_item(item, struct mem_node, listnode);
            if (node->ptr == ptr) {
                prev_ptr = node->ptr;
                prev_size = node->size;
                break;
            }
        }

        if (prev_ptr == NULL) {
            OS_LOGF(LOG_TAG, "%s:%s:%d: failed to find ptr[%p] in list, abort realloc",
                   file_name(file), func, line, ptr);
            return NULL;
        }

        OS_THREAD_MUTEX_UNLOCK(info->mutex);

        if (size > prev_size) {
            void *new_ptr = memory_detect_malloc(size, file, func, line);
            if (new_ptr != NULL)
                memcpy(new_ptr, prev_ptr, prev_size);

            memory_detect_free(prev_ptr, file, func, line);
            return new_ptr;
        }
        else
            return prev_ptr;
    }
    else {
#if defined(ENABLE_MEMORY_OVERFLOW_DETECT)
        OS_LOGF(LOG_TAG, "%s:%s:%d: failed to create mem_info instance, abort realloc",
               file_name(file), func, line);
        return NULL;
#else
        return realloc(ptr, size);
#endif
    }

    return NULL;
}

void memory_detect_free(void *ptr, const char *file, const char *func, int line)
{
    struct mem_info *info = memory_detect_init();
    struct listnode *item;

    if (info != NULL) {
        struct mem_node *node;
        bool found = false;

        OS_THREAD_MUTEX_LOCK(info->mutex);

        list_for_each_reverse(item, &info->list) {
            node = node_to_item(item, struct mem_node, listnode);
            if (node->ptr == ptr) {
                found = true;
                break;
            }
        }

        if (found) {
#if defined(ENABLE_MEMORY_OVERFLOW_DETECT)
            memory_boundary_verify(node);
#endif
            //memory_node_debug(node, "Free");

            info->free_count++;
            info->cur_used -= node->size;

            list_remove(item);
            free(node);
        }
        else {
            OS_LOGF(LOG_TAG, "%s:%s:%d: failed to find ptr[%p] in list, double free?",
                   file_name(file), func, line, ptr);
        }

        OS_THREAD_MUTEX_UNLOCK(info->mutex);
    }

    // free memory
#if defined(ENABLE_MEMORY_OVERFLOW_DETECT)
    memory_boundary_free(ptr);
#else
    free(ptr);
#endif
}

void *memory_detect_strdup(void *str, const char *file, const char *func, int line)
{
    char *ptr;
    size_t len;

    if (str == NULL)
        return NULL;

    len = strlen(str);
    ptr = memory_detect_malloc(len + 1, file, func, line);
    if (ptr != NULL) {
        memcpy(ptr, str, len);
        ptr[len] = '\0';
    }

    return ptr;
}

void memory_detect_dump()
{
    struct mem_info *info  = memory_detect_init();
    struct mem_node *node;
    struct listnode *item;

    if (info != NULL) {
        OS_LOGW(LOG_TAG, ">>");
        OS_LOGW(LOG_TAG, "++++++++++++++++++++ MEMORY DETECT ++++++++++++++++++++");

        OS_THREAD_MUTEX_LOCK(info->mutex);

        list_for_each(item, &info->list) {
            node = node_to_item(item, struct mem_node, listnode);
            memory_node_debug(node, "Dump");
#if defined(ENABLE_MEMORY_OVERFLOW_DETECT)
            memory_boundary_verify(node);
#endif
        }

        OS_LOGW(LOG_TAG, "Summary: malloc [%d] blocks, free [%d] blocks, current use [%d] Bytes, max use [%d] Bytes",
                info->malloc_count, info->free_count, info->cur_used, info->max_used);

        OS_THREAD_MUTEX_UNLOCK(info->mutex);

        OS_LOGW(LOG_TAG, "-------------------- MEMORY DETECT --------------------");
        OS_LOGW(LOG_TAG, "<<");
    }
}

#endif
