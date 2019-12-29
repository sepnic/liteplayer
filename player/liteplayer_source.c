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
#include "msgutils/ringbuf.h"
#include "esp_adf/esp_log.h"
#include "esp_adf/esp_err.h"
#include "esp_adf/audio_common.h"

#include "liteplayer_config.h"
#include "liteplayer_source.h"

#define TAG "liteplayersource"

typedef struct media_source_priv {
    media_source_info_t info;
    ringbuf_handle_t rb;
    long long bytes_written;

    media_source_state_cb listener;
    void *listener_priv;

    bool stop;
    os_mutex_t lock; // lock for rb/listener
    os_cond_t cond;  // wait stop to exit mediasource thread
} media_source_priv_t;

static void media_source_cleanup(media_source_priv_t *priv)
{
    if (priv->lock != NULL)
        OS_THREAD_MUTEX_DESTROY(priv->lock);
    if (priv->cond != NULL)
        OS_THREAD_COND_DESTROY(priv->cond);
    if (priv->info.url != NULL)
        audio_free(priv->info.url);
    audio_free(priv);
}

static void *media_source_thread(void *arg)
{
    media_source_priv_t *priv = (media_source_priv_t *)arg;
    media_source_state_t state = MEDIA_SOURCE_READ_FAILED;
    http_handle_t client = NULL;
    fatfs_handle_t file = NULL;

    if (priv->info.source_type == MEDIA_SOURCE_HTTP) {
        client = priv->info.http_wrapper.open(priv->info.url,
                                              priv->info.content_pos,
                                              priv->info.http_wrapper.http_priv);
        if (client == NULL) {
            state = MEDIA_SOURCE_READ_FAILED;
            goto thread_exit;
        }
    }
    else if (priv->info.source_type == MEDIA_SOURCE_FILE) {
        file = priv->info.fatfs_wrapper.open(priv->info.url,
                                             FATFS_READ,
                                             priv->info.content_pos,
                                             priv->info.fatfs_wrapper.fatfs_priv);
        if (file == NULL) {
            state = MEDIA_SOURCE_READ_FAILED;
            goto thread_exit;
        }
    }
    else {
        state = MEDIA_SOURCE_READ_FAILED;
        goto thread_exit;
    }

    char buffer[DEFAULT_MEDIA_SOURCE_BUFFER_SIZE];
    int bytes_read = 0, bytes_written = 0;

    while (!priv->stop) {
        if (client != NULL)
            bytes_read = priv->info.http_wrapper.read(client, buffer, sizeof(buffer));
        else if (file != NULL)
            bytes_read = priv->info.fatfs_wrapper.read(file, buffer, sizeof(buffer));

        if (bytes_read < 0) {
            ESP_LOGE(TAG, "Media source read failed");
            state = MEDIA_SOURCE_READ_FAILED;
            goto thread_exit;
        }
        else if (bytes_read == 0) {
            ESP_LOGD(TAG, "Media source read done");
            state = MEDIA_SOURCE_READ_DONE;
            goto thread_exit;
        }

        bytes_written = 0;

        do {
            OS_THREAD_MUTEX_LOCK(priv->lock);
            if (!priv->stop)
                bytes_written = rb_write(priv->rb, &buffer[bytes_written], bytes_read, AUDIO_MAX_DELAY);
            OS_THREAD_MUTEX_UNLOCK(priv->lock);

            if (bytes_written > 0) {
                bytes_read -= bytes_written;
                priv->bytes_written += bytes_written;
            }
            else {
                if (bytes_written == RB_DONE || bytes_written == RB_ABORT || bytes_written == RB_OK) {
                    ESP_LOGD(TAG, "Media source write done");
                    state = MEDIA_SOURCE_WRITE_DONE;
                }
                else {
                    ESP_LOGD(TAG, "Media source write failed");
                    state = MEDIA_SOURCE_WRITE_FAILED;
                }
                goto thread_exit;
            }
        } while (!priv->stop && bytes_read > 0);
    }

thread_exit:
    OS_THREAD_MUTEX_LOCK(priv->lock);
    if (!priv->stop) {
        if (state == MEDIA_SOURCE_READ_DONE || state == MEDIA_SOURCE_WRITE_DONE)
            rb_done_write(priv->rb);
        else
            rb_abort(priv->rb);
        if (priv->listener)
            priv->listener(state, priv->listener_priv);
    }
    OS_THREAD_MUTEX_UNLOCK(priv->lock);

    if (client != NULL)
        priv->info.http_wrapper.close(client);
    else if (file != NULL)
        priv->info.fatfs_wrapper.close(file);

    while (!priv->stop)
        OS_THREAD_COND_WAIT(priv->cond, priv->lock);
    media_source_cleanup(priv);

    ESP_LOGD(TAG, "Media source task leave");
    return NULL;
}

media_source_handle_t media_source_start(media_source_info_t *info,
                                         ringbuf_handle_t rb,
                                         media_source_state_cb listener,
                                         void *listener_priv)
{
    if (info == NULL || info->url == NULL || rb == NULL)
        return NULL;

    media_source_priv_t *priv = audio_calloc(1, sizeof(media_source_priv_t));
    if (priv == NULL)
        return NULL;

    memcpy(&priv->info, info, sizeof(media_source_info_t));
    priv->rb = rb;
    priv->listener = listener;
    priv->listener_priv = listener_priv;
    priv->lock = OS_THREAD_MUTEX_CREATE();
    priv->cond = OS_THREAD_COND_CREATE();
    priv->info.url = audio_strdup(info->url);
    if (priv->lock == NULL || priv->cond == NULL || priv->info.url == NULL)
        goto start_failed;

    struct os_threadattr attr = {
        .name = "ael_source",
        .priority = DEFAULT_MEDIA_SOURCE_TASK_PRIO,
        .stacksize = DEFAULT_MEDIA_SOURCE_TASK_STACKSIZE,
        .joinable = false,
    };
    os_thread_t id = OS_THREAD_CREATE(&attr, media_source_thread, priv);
    if (id == NULL)
        goto start_failed;

    return priv;

start_failed:
    media_source_cleanup(priv);
    return NULL;
}

long long media_source_bytes_written(media_source_handle_t handle)
{
    media_source_priv_t *priv = (media_source_priv_t *)handle;
    if (priv == NULL)
        return -1;
    return priv->bytes_written;
}

void media_source_stop(media_source_handle_t handle)
{
    media_source_priv_t *priv = (media_source_priv_t *)handle;
    if (priv == NULL)
        return;

    rb_done_read(priv->rb);
    rb_done_write(priv->rb);

    OS_THREAD_MUTEX_LOCK(priv->lock);
    priv->stop = true;
    OS_THREAD_COND_SIGNAL(priv->cond);
    OS_THREAD_MUTEX_UNLOCK(priv->lock);
}
