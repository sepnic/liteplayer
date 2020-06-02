/*
 * Copyright 2019-2020 LUOYUN <sysu.zqlong@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>

#include "msgutils/os_thread.h"
#include "msgutils/os_logger.h"
#include "msgutils/ringbuf.h"
#include "msgutils/common_list.h"
#include "esp_adf/audio_common.h"

#include "liteplayer_config.h"
#include "liteplayer_source.h"

#define TAG "[liteplayer]SOURCE"

#define DEFAULT_M3U_BUFFER_SIZE    ( 16*1024 )
#define DEFAULT_M3U_FILL_THRESHOLD ( 32*1024 )

struct media_source_priv {
    struct media_source_info info;
    ringbuf_handle_t rb;
    struct listnode m3u_list;

    media_source_state_cb listener;
    void *listener_priv;

    bool stop;
    bool reach_threshold;
    os_mutex_t lock; // lock for rb/listener
    os_cond_t cond;  // wait stop to exit mediasource thread
};

struct m3u_node {
    const char *url;
    struct listnode listnode;
};

static void media_source_cleanup(struct media_source_priv *priv);

static void m3u_list_clear(struct listnode *list)
{
    struct listnode *item, *tmp;
    list_for_each_safe(item, tmp, list) {
        struct m3u_node *node = node_to_item(item, struct m3u_node, listnode);
        list_remove(item);
        audio_free(node->url);
        audio_free(node);
    }
}

static int m3u_list_insert(struct listnode *list, const char *url)
{
    struct m3u_node *node = audio_malloc(sizeof(struct m3u_node));
    if (node == NULL)
        return -1;
    node->url = audio_strdup(url);
    if (node->url == NULL) {
        audio_free(node);
        return -1;
    }
    list_add_tail(list, &node->listnode);
    return 0;
}

static char *m3u_parser_get_line(char *buffer, int *index, int *remain)
{
    char c, *out = NULL;
    if (*remain > 0) {
        bool line_end = false;
        out = buffer + *index;
        int idx = *index;
        while ((c = buffer[idx]) != 0) {
            if (c == '\r' || c == '\n') {
                buffer[idx] = 0;
                line_end = true;
            }
            else if (line_end) {
                *remain -= idx - *index;
                *index = idx;
                return out;
            }
            idx++;
            if (idx == (*index + *remain)) {
                *remain = 0;
                return out;
            }
        }
    }
    return NULL;
}

static int m3u_parser_process_line(struct media_source_priv *priv, char *line)
{
    char temp[256];
    int ret = -1;
    if (strstr(line, "http") == line) { // full uri
        ret = m3u_list_insert(&priv->m3u_list, line);
    }
    else if (strstr(line, "//") == line) { //schemeless uri
        if (strstr(priv->info.url, "https") == priv->info.url)
            snprintf(temp, sizeof(temp), "https:%s", line);
        else
            snprintf(temp, sizeof(temp), "http:%s", line);
        ret = m3u_list_insert(&priv->m3u_list, temp);
    }
    else if (strstr(line, "/") == line) { // Root uri
        char *dup_url = audio_strdup(priv->info.url);
        if (dup_url == NULL) {
            return -1;
        }
        char *host = strstr(dup_url, "//");
        if (host == NULL) {
            audio_free(dup_url);
            return -1;
        }
        host += 2;
        char *path = strstr(host, "/");
        if (path == NULL) {
            audio_free(dup_url);
            return -1;
        }
        path[0] = 0;
        snprintf(temp, sizeof(temp), "%s%s", dup_url, line);
        audio_free(dup_url);
        ret = m3u_list_insert(&priv->m3u_list, temp);
    }
    else { // Relative URI
        char *dup_url = audio_strdup(priv->info.url);
        if (dup_url == NULL) {
            return -1;
        }
        char *pos = strrchr(dup_url, '/'); // Search for last "/"
        if (pos == NULL) {
            audio_free(dup_url);
            return -1;
        }
        pos[1] = '\0';
        snprintf(temp, sizeof(temp), "%s%s", dup_url, line);
        audio_free(dup_url);
        ret = m3u_list_insert(&priv->m3u_list, temp);
    }
    return ret;
}

static int m3u_parser_resolve(struct media_source_priv *priv)
{
    int ret = -1;
    http_handle_t http = NULL;
    char *content = audio_malloc(DEFAULT_M3U_BUFFER_SIZE);
    if (content == NULL)
        goto resolve_done;

    http = priv->info.http_ops.open(priv->info.url, 0, priv->info.http_ops.http_priv);
    if (http == NULL) {
        OS_LOGE(TAG, "Failed to connect m3u url");
        goto resolve_done;
    }

    int bytes_read = priv->info.http_ops.read(http, content, DEFAULT_M3U_BUFFER_SIZE);
    if (bytes_read <= 0) {
        OS_LOGE(TAG, "Failed to read m3u content");
        goto resolve_done;
    }
    OS_LOGV(TAG, "Succeed to read m3u content:\n%s", content);

    int index = 0, remain = bytes_read;
    char *line = NULL;
    bool is_valid_m3u = false;
    bool is_valid_url = false;
    while ((line = m3u_parser_get_line(content, &index, &remain)) != NULL) {
        if (!is_valid_m3u && strcmp(line, "#EXTM3U") == 0) {
            is_valid_m3u = true;
            continue;
        }
        if (strstr(line, "http") == line) {
            m3u_parser_process_line(priv, line);
            is_valid_m3u = true;
            continue;
        }
        if (!is_valid_m3u) {
            break;
        }
        if (!is_valid_url && strstr(line, "#EXTINF") == line) {
            is_valid_url = true;
            continue;
        }
        else if (!is_valid_url && strstr(line, "#EXT-X-STREAM-INF") == line) {
            /**
             * As these are stream URIs we need to fetch thse periodically to keep live streaming.
             * For now we handle it same as normal uri and exit.
             */
            is_valid_url = true;
            continue;
        }
        else if (strncmp(line, "#", 1) == 0) {
            /**
             * Some other playlist field we don't support.
             * Simply treat this as a comment and continue to find next line.
             */
            continue;
        }
        if (!is_valid_url) {
            continue;
        }
        is_valid_url = false;
        m3u_parser_process_line(priv, line);
    }

    if (!list_empty(&priv->m3u_list))
        ret = 0;
#if 0
    struct listnode *item;
    int i = 0;
    list_for_each(item, &priv->m3u_list) {
        struct m3u_node *node = node_to_item(item, struct m3u_node, listnode);
        OS_LOGD(TAG, "-->url[%d]=[%s]", i++, node->url);
    }
#endif

resolve_done:
    if (http != NULL)
        priv->info.http_ops.close(http);
    if (content != NULL)
        audio_free(content);
    return ret;
}

static void *m3u_source_thread(void *arg)
{
    struct media_source_priv *priv = (struct media_source_priv *)arg;
    enum media_source_state state = MEDIA_SOURCE_READ_FAILED;
    http_handle_t http = NULL;
    char *buffer = NULL;
    long long pos = priv->info.content_pos;
    int ret = 0;

    buffer = audio_malloc(DEFAULT_MEDIA_SOURCE_BUFFER_SIZE);
    if (buffer == NULL) {
        OS_LOGE(TAG, "Failed to allocate response buffer");
        goto thread_exit;
    }

resolve_m3u:
    if (priv->stop)
        goto thread_exit;
    ret = m3u_parser_resolve(priv);
    if (ret != 0) {
        OS_LOGE(TAG, "Failed to parse m3u url");
        goto thread_exit;
    }

dequeue_url:
    if (list_empty(&priv->m3u_list)) {
        int fill_size = 0;
        while (!priv->stop) {
            OS_THREAD_MUTEX_LOCK(priv->lock);
            if (!priv->stop)
                fill_size = rb_bytes_filled(priv->rb);
            else
                fill_size = 0;
            OS_THREAD_MUTEX_UNLOCK(priv->lock);

            // waiting decoder to consume the old data in the ringbuf
            if (fill_size > DEFAULT_M3U_FILL_THRESHOLD)
                OS_THREAD_SLEEP_MSEC(100);
            else
                break;
        }
        OS_LOGV(TAG, "Current m3u list playdone, resolve more");
        goto resolve_m3u;
    }

    if (http != NULL) {
        pos = 0;
        priv->info.http_ops.close(http);
    }

    struct listnode *front = list_head(&priv->m3u_list);
    struct m3u_node *node = node_to_item(front, struct m3u_node, listnode);
    http = priv->info.http_ops.open(node->url, pos, priv->info.http_ops.http_priv);

    list_remove(front);
    audio_free(node->url);
    audio_free(node);

    if (http == NULL) {
        OS_LOGE(TAG, "Connect failed, request next url");
        state = MEDIA_SOURCE_READ_FAILED;
        goto dequeue_url;
    }

    int bytes_read = 0, bytes_written = 0;
    while (!priv->stop) {
        if (http != NULL)
            bytes_read = priv->info.http_ops.read(http, buffer, DEFAULT_MEDIA_SOURCE_BUFFER_SIZE);
        if (bytes_read < 0) {
            OS_LOGE(TAG, "Read failed, request next url");
            state = MEDIA_SOURCE_READ_FAILED;
            goto dequeue_url;
        }
        else if (bytes_read == 0) {
            OS_LOGD(TAG, "Read done, request next url");
            state = MEDIA_SOURCE_READ_DONE;
            goto dequeue_url;
        }

        bytes_written = 0;

        do {
            OS_THREAD_MUTEX_LOCK(priv->lock);
            if (!priv->stop)
                ret = rb_write(priv->rb, &buffer[bytes_written], bytes_read, AUDIO_MAX_DELAY);
            OS_THREAD_MUTEX_UNLOCK(priv->lock);

            if (ret > 0) {
                bytes_read -= ret;
                bytes_written += ret;
            }
            else {
                if (ret == RB_DONE || ret == RB_ABORT || ret == RB_OK) {
                    OS_LOGD(TAG, "Write done, abort left urls");
                    state = MEDIA_SOURCE_WRITE_DONE;
                }
                else {
                    OS_LOGD(TAG, "Write failed, abort left urls");
                    state = MEDIA_SOURCE_WRITE_FAILED;
                }
                goto thread_exit;
            }
        } while (!priv->stop && bytes_read > 0);

        if (!priv->reach_threshold) {
            OS_THREAD_MUTEX_LOCK(priv->lock);
            priv->reach_threshold = rb_reach_threshold(priv->rb);
            if (priv->reach_threshold && priv->listener)
                priv->listener(MEDIA_SOURCE_REACH_THRESHOLD, priv->listener_priv);
            OS_THREAD_MUTEX_UNLOCK(priv->lock);
        }
    }

thread_exit:
    if (http != NULL)
        priv->info.http_ops.close(http);
    if (buffer != NULL)
        audio_free(buffer);

    {
        OS_THREAD_MUTEX_LOCK(priv->lock);

        if (!priv->stop) {
            if (state == MEDIA_SOURCE_READ_DONE || state == MEDIA_SOURCE_WRITE_DONE)
                rb_done_write(priv->rb);
            else
                rb_abort(priv->rb);
            if (priv->listener)
                priv->listener(state, priv->listener_priv);
        }

        OS_LOGV(TAG, "Waiting stop command");
        while (!priv->stop)
            OS_THREAD_COND_WAIT(priv->cond, priv->lock);

        OS_THREAD_MUTEX_UNLOCK(priv->lock);
    }

    media_source_cleanup(priv);
    OS_LOGD(TAG, "M3U source task leave");
    return NULL;
}

int m3u_get_first_url(struct media_source_info *info, char *buf, int buf_size)
{
    if (info == NULL || info->url == NULL || buf == NULL || buf_size <=0)
        return -1;

    int ret = -1;
    http_handle_t http = NULL;
    char *content = audio_malloc(DEFAULT_M3U_BUFFER_SIZE);
    if (content == NULL)
        goto resolve_done;

    http = info->http_ops.open(info->url, 0, info->http_ops.http_priv);
    if (http == NULL) {
        OS_LOGE(TAG, "Failed to connect m3u url");
        goto resolve_done;
    }

    int bytes_read = info->http_ops.read(http, content, DEFAULT_M3U_BUFFER_SIZE);
    if (bytes_read <= 0) {
        OS_LOGE(TAG, "Failed to read m3u content");
        goto resolve_done;
    }
    OS_LOGV(TAG, "Succeed to read m3u content:\n%s", content);

    int index = 0, remain = bytes_read;
    char *line = NULL;
    char *url_line = NULL;
    bool is_valid_m3u = false;
    bool is_valid_url = false;
    while ((line = m3u_parser_get_line(content, &index, &remain)) != NULL) {
        if (!is_valid_m3u && strcmp(line, "#EXTM3U") == 0) {
            is_valid_m3u = true;
            continue;
        }
        if (strstr(line, "http") == line) {
            url_line = line;
            break;
        }
        if (!is_valid_m3u) {
            break;
        }
        if (!is_valid_url && strstr(line, "#EXTINF") == line) {
            is_valid_url = true;
            continue;
        }
        else if (!is_valid_url && strstr(line, "#EXT-X-STREAM-INF") == line) {
            /**
             * As these are stream URIs we need to fetch thse periodically to keep live streaming.
             * For now we handle it same as normal uri and exit.
             */
            is_valid_url = true;
            continue;
        }
        else if (strncmp(line, "#", 1) == 0) {
            /**
             * Some other playlist field we don't support.
             * Simply treat this as a comment and continue to find next line.
             */
            continue;
        }
        if (!is_valid_url) {
            continue;
        }
        url_line = line;
        break;
    }

    if (url_line != NULL) {
        if (strstr(url_line, "http") == url_line) { // full uri
            snprintf(buf, buf_size, "%s", url_line);
            ret = 0;
        }
        else if (strstr(url_line, "//") == url_line) { //schemeless uri
            if (strstr(info->url, "https") == info->url)
                snprintf(buf, buf_size, "https:%s", url_line);
            else
                snprintf(buf, buf_size, "http:%s", url_line);
            ret = 0;
        }
        else if (strstr(url_line, "/") == url_line) { // Root uri
            char *dup_url = audio_strdup(info->url);
            if (dup_url == NULL) {
                goto resolve_done;
            }
            char *host = strstr(dup_url, "//");
            if (host == NULL) {
                audio_free(dup_url);
                goto resolve_done;
            }
            host += 2;
            char *path = strstr(host, "/");
            if (path == NULL) {
                audio_free(dup_url);
                goto resolve_done;
            }
            path[0] = 0;
            snprintf(buf, buf_size, "%s%s", dup_url, url_line);
            audio_free(dup_url);
            ret = 0;
        }
        else { // Relative URI
            char *dup_url = audio_strdup(info->url);
            if (dup_url == NULL) {
                goto resolve_done;
            }
            char *pos = strrchr(dup_url, '/'); // Search for last "/"
            if (pos == NULL) {
                audio_free(dup_url);
                goto resolve_done;
            }
            pos[1] = '\0';
            snprintf(buf, buf_size, "%s%s", dup_url, url_line);
            audio_free(dup_url);
            ret = 0;
        }
    }

resolve_done:
    if (http != NULL)
        info->http_ops.close(http);
    if (content != NULL)
        audio_free(content);
    return ret;
}

static void media_source_cleanup(struct media_source_priv *priv)
{
    if (priv->lock != NULL)
        OS_THREAD_MUTEX_DESTROY(priv->lock);
    if (priv->cond != NULL)
        OS_THREAD_COND_DESTROY(priv->cond);
    if (priv->info.url != NULL)
        audio_free(priv->info.url);
    m3u_list_clear(&priv->m3u_list);
    audio_free(priv);
}

static void *media_source_thread(void *arg)
{
    struct media_source_priv *priv = (struct media_source_priv *)arg;
    enum media_source_state state = MEDIA_SOURCE_READ_FAILED;
    http_handle_t http = NULL;
    file_handle_t file = NULL;
    char *buffer = NULL;

    buffer = audio_malloc(DEFAULT_MEDIA_SOURCE_BUFFER_SIZE);
    if (buffer == NULL) {
        OS_LOGE(TAG, "Failed to allocate response buffer");
        goto thread_exit;
    }

    if (priv->info.source_type == MEDIA_SOURCE_HTTP) {
        http = priv->info.http_ops.open(priv->info.url,
                                        priv->info.content_pos,
                                        priv->info.http_ops.http_priv);
        if (http == NULL) {
            state = MEDIA_SOURCE_READ_FAILED;
            goto thread_exit;
        }
    }
    else if (priv->info.source_type == MEDIA_SOURCE_FILE) {
        file = priv->info.file_ops.open(priv->info.url,
                                        FILE_READ,
                                        priv->info.content_pos,
                                        priv->info.file_ops.file_priv);
        if (file == NULL) {
            state = MEDIA_SOURCE_READ_FAILED;
            goto thread_exit;
        }
    }
    else {
        state = MEDIA_SOURCE_READ_FAILED;
        goto thread_exit;
    }

    int bytes_read = 0, bytes_written = 0;
    int ret = 0;
    while (!priv->stop) {
        if (http != NULL)
            bytes_read = priv->info.http_ops.read(http, buffer, DEFAULT_MEDIA_SOURCE_BUFFER_SIZE);
        else if (file != NULL)
            bytes_read = priv->info.file_ops.read(file, buffer, DEFAULT_MEDIA_SOURCE_BUFFER_SIZE);

        if (bytes_read < 0) {
            OS_LOGE(TAG, "Media source read failed");
            state = MEDIA_SOURCE_READ_FAILED;
            goto thread_exit;
        }
        else if (bytes_read == 0) {
            OS_LOGD(TAG, "Media source read done");
            state = MEDIA_SOURCE_READ_DONE;
            goto thread_exit;
        }

        bytes_written = 0;

        do {
            OS_THREAD_MUTEX_LOCK(priv->lock);
            if (!priv->stop)
                ret = rb_write(priv->rb, &buffer[bytes_written], bytes_read, AUDIO_MAX_DELAY);
            OS_THREAD_MUTEX_UNLOCK(priv->lock);

            if (ret > 0) {
                bytes_read -= ret;
                bytes_written += ret;
            }
            else {
                if (ret == RB_DONE || ret == RB_ABORT || ret == RB_OK) {
                    OS_LOGD(TAG, "Media source write done");
                    state = MEDIA_SOURCE_WRITE_DONE;
                }
                else {
                    OS_LOGD(TAG, "Media source write failed");
                    state = MEDIA_SOURCE_WRITE_FAILED;
                }
                goto thread_exit;
            }
        } while (!priv->stop && bytes_read > 0);

        if (!priv->reach_threshold) {
            OS_THREAD_MUTEX_LOCK(priv->lock);
            priv->reach_threshold = rb_reach_threshold(priv->rb);
            if (priv->reach_threshold && priv->listener)
                priv->listener(MEDIA_SOURCE_REACH_THRESHOLD, priv->listener_priv);
            OS_THREAD_MUTEX_UNLOCK(priv->lock);
        }
    }

thread_exit:
    if (http != NULL)
        priv->info.http_ops.close(http);
    else if (file != NULL)
        priv->info.file_ops.close(file);
    if (buffer != NULL)
        audio_free(buffer);

    {
        OS_THREAD_MUTEX_LOCK(priv->lock);

        if (!priv->stop) {
            if (state == MEDIA_SOURCE_READ_DONE || state == MEDIA_SOURCE_WRITE_DONE)
                rb_done_write(priv->rb);
            else
                rb_abort(priv->rb);
            if (priv->listener)
                priv->listener(state, priv->listener_priv);
        }

        OS_LOGV(TAG, "Waiting stop command");
        while (!priv->stop)
            OS_THREAD_COND_WAIT(priv->cond, priv->lock);

        OS_THREAD_MUTEX_UNLOCK(priv->lock);
    }

    media_source_cleanup(priv);
    OS_LOGD(TAG, "Media source task leave");
    return NULL;
}

media_source_handle_t media_source_start(struct media_source_info *info,
                                         ringbuf_handle_t rb,
                                         media_source_state_cb listener,
                                         void *listener_priv)
{
    if (info == NULL || info->url == NULL || rb == NULL)
        return NULL;

    struct media_source_priv *priv = audio_calloc(1, sizeof(struct media_source_priv));
    if (priv == NULL)
        return NULL;

    memcpy(&priv->info, info, sizeof(struct media_source_info));
    priv->rb = rb;
    priv->listener = listener;
    priv->listener_priv = listener_priv;
    priv->lock = OS_THREAD_MUTEX_CREATE();
    priv->cond = OS_THREAD_COND_CREATE();
    priv->info.url = audio_strdup(info->url);
    list_init(&priv->m3u_list);
    if (priv->lock == NULL || priv->cond == NULL || priv->info.url == NULL)
        goto start_failed;

    struct os_threadattr attr = {
        .name = "ael-source",
        .priority = DEFAULT_MEDIA_SOURCE_TASK_PRIO,
        .stacksize = DEFAULT_MEDIA_SOURCE_TASK_STACKSIZE,
        .joinable = false,
    };
    os_thread_t id = NULL;

    if (strstr(info->url, ".m3u") != NULL)
        id = OS_THREAD_CREATE(&attr, m3u_source_thread, priv);
    else
        id = OS_THREAD_CREATE(&attr, media_source_thread, priv);
    if (id == NULL)
        goto start_failed;

    return priv;

start_failed:
    media_source_cleanup(priv);
    return NULL;
}

void media_source_stop(media_source_handle_t handle)
{
    struct media_source_priv *priv = (struct media_source_priv *)handle;
    if (priv == NULL)
        return;

    rb_done_read(priv->rb);
    rb_done_write(priv->rb);

    {
        OS_THREAD_MUTEX_LOCK(priv->lock);
        priv->stop = true;
        OS_THREAD_COND_SIGNAL(priv->cond);
        OS_THREAD_MUTEX_UNLOCK(priv->lock);
    }
}
