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
#include <stdbool.h>
#include <string.h>

#include "msgutils/os_thread.h"
#include "msgutils/os_logger.h"
#include "msgutils/msglooper.h"
#include "esp_adf/audio_common.h"
#include "liteplayer_adapter.h"
#include "liteplayer_config.h"
#include "liteplayer_main.h"
#include "liteplayer_manager.h"

#define TAG "[liteplayer]MANAGER"

struct liteplayer_mngr {
    liteplayer_handle_t  player;
    mlooper_t            looper;
    os_mutex_t           lock;
    int                  threshold_ms;
    enum liteplayer_state state;
    liteplayer_state_cb  listener;
    void                *listener_priv;

    struct file_wrapper  file_ops;
    struct http_wrapper  http_ops;
    struct sink_wrapper  sink_ops;

    char                *url_list[DEFAULT_PLAYLIST_URL_MAX];
    int                  url_index;
    int                  url_count;

    bool                 is_list;
    bool                 is_paused;
    bool                 is_completed;
    bool                 is_looping;
};

enum {
    PLAYER_DO_SET_SOURCE = 0,
    PLAYER_DO_PREPARE,
    PLAYER_DO_START,
    PLAYER_DO_PAUSE,
    PLAYER_DO_RESUME,
    PLAYER_DO_SEEK,
    PLAYER_DO_NEXT,
    PLAYER_DO_PREV,
    PLAYER_DO_STOP,
    PLAYER_DO_RESET,
};

static void playlist_clear(liteplayer_mngr_handle_t mngr)
{
    OS_THREAD_MUTEX_LOCK(mngr->lock);

    for (int i = 0; i < mngr->url_count; i++) {
        audio_free(mngr->url_list[i]);
    }
    mngr->url_index = 0;
    mngr->url_count = 0;
    mngr->is_list = false;

    OS_THREAD_MUTEX_UNLOCK(mngr->lock);
}

static int playlist_insert(liteplayer_mngr_handle_t mngr, const char *url)
{
    OS_THREAD_MUTEX_LOCK(mngr->lock);

    if (mngr->url_count >= DEFAULT_PLAYLIST_URL_MAX) {
        OS_LOGE(TAG, "Reach max url count: %d", mngr->url_count);
        OS_THREAD_MUTEX_UNLOCK(mngr->lock);
        return -1;
    }

    char *insert = audio_strdup(url);
    if (insert == NULL) {
        OS_THREAD_MUTEX_UNLOCK(mngr->lock);
        return -1;
    }
    mngr->url_list[mngr->url_count] = insert;
    mngr->url_count++;

    OS_THREAD_MUTEX_UNLOCK(mngr->lock);
    return 0;
}

static char *playlist_get_line(char *buffer, int *index, int *remain)
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

static int playlist_resolve(liteplayer_mngr_handle_t mngr, const char *filename)
{
    int ret = -1;
    file_handle_t file = NULL;
    char *content = audio_malloc(DEFAULT_PLAYLIST_BUFFER_SIZE);
    if (content == NULL) {
        OS_LOGE(TAG, "Failed to allocate playlist parser buffer");
        goto resolve_done;
    }

    file = mngr->file_ops.open(filename, FILE_READ, 0, mngr->file_ops.file_priv);
    if (file == NULL) {
        OS_LOGE(TAG, "Failed to open playlist");
        goto resolve_done;
    }

    int bytes_read = mngr->file_ops.read(file, content, DEFAULT_PLAYLIST_BUFFER_SIZE);
    if (bytes_read <= 0) {
        OS_LOGE(TAG, "Failed to read playlist");
        goto resolve_done;
    }
    OS_LOGV(TAG, "Succeed to read playlist:\n%s", content);

    int index = 0, remain = bytes_read;
    char *line = NULL;
    while ((line = playlist_get_line(content, &index, &remain)) != NULL) {
        playlist_insert(mngr, line);
    }

    OS_THREAD_MUTEX_LOCK(mngr->lock);
    if (mngr->url_count > 0) {
        mngr->is_list = true;
        ret = 0;
    }
#if 1
    for (int i = 0; i < mngr->url_count; i++) {
        OS_LOGV(TAG, "-->url[%d]=[%s]", i, mngr->url_list[i]);
    }
#endif
    OS_THREAD_MUTEX_UNLOCK(mngr->lock);

resolve_done:
    if (file != NULL)
        mngr->file_ops.close(file);
    if (content != NULL)
        audio_free(content);
    return ret;
}

static int manager_state_callback(enum liteplayer_state state, int errcode, void *priv)
{
    liteplayer_mngr_handle_t mngr = (liteplayer_mngr_handle_t)priv;
    bool state_sync = true;

    OS_THREAD_MUTEX_LOCK(mngr->lock);

    switch (state) {
    case LITEPLAYER_INITED:
        if (mngr->is_completed) {
            struct message *msg = message_obtain(PLAYER_DO_PREPARE, 0, 0, mngr);
            if (msg != NULL) {
                state_sync = false;
                mlooper_post_message(mngr->looper, msg);
            }
        }
        break;

    case LITEPLAYER_PREPARED:
        if (mngr->is_completed) {
            struct message *msg = message_obtain(PLAYER_DO_START, 0, 0, mngr);
            if (msg != NULL) {
                state_sync = false;
                mlooper_post_message(mngr->looper, msg);
            }
        }
        break;

    case LITEPLAYER_STARTED:
        if (mngr->is_completed) {
            state_sync = mngr->is_paused;
        }
        mngr->is_completed = false;
        mngr->is_paused = false;
        break;

    case LITEPLAYER_PAUSED:
        mngr->is_paused = true;
        break;

    case LITEPLAYER_SEEKCOMPLETED:
        state_sync = true;
        break;

    case LITEPLAYER_CACHECOMPLETED:
    case LITEPLAYER_NEARLYCOMPLETED:
        if (mngr->is_list || mngr->is_looping) {
            state_sync = false;
        }
        break;

    case LITEPLAYER_COMPLETED:
        mngr->is_completed = true;
        if (mngr->is_list || mngr->is_looping) {
            struct message *msg = message_obtain(PLAYER_DO_STOP, 0, 0, mngr);
            if (msg != NULL) {
                state_sync = false;
                mlooper_post_message(mngr->looper, msg);
            }
        }
        break;

    case LITEPLAYER_ERROR:
        if (mngr->is_list && !mngr->is_looping && mngr->url_count > 1) {
            mngr->is_completed = true; // fake completed, try to play next
            struct message *msg = message_obtain(PLAYER_DO_STOP, 0, 0, mngr);
            if (msg != NULL) {
                state_sync = false;
                mlooper_post_message(mngr->looper, msg);
            }
        }
        break;

    case LITEPLAYER_STOPPED:
        if ((mngr->is_list || mngr->is_looping) && mngr->is_completed) {
            struct message *msg = message_obtain(PLAYER_DO_RESET, 0, 0, mngr);
            if (msg != NULL) {
                state_sync = false;
                mlooper_post_message(mngr->looper, msg);
            }
        }
        break;

    case LITEPLAYER_IDLE:
        if ((mngr->is_list || mngr->is_looping) && mngr->is_completed) {
            if (!mngr->is_looping) {
                mngr->url_index++;
                if (mngr->url_index >= mngr->url_count)
                    mngr->url_index = 0;
            }
            struct message *msg = message_obtain(PLAYER_DO_SET_SOURCE, 0, 0, mngr);
            if (msg != NULL) {
                state_sync = false;
                mlooper_post_message(mngr->looper, msg);
            }
        }
        break;

    default:
        state_sync = false;
        break;
    }

    mngr->state = state;

    OS_THREAD_MUTEX_UNLOCK(mngr->lock);

    if (state_sync && mngr->listener)
        mngr->listener(state, errcode, mngr->listener_priv);
    return 0;
}

static void manager_looper_handle(struct message *msg)
{
    liteplayer_mngr_handle_t mngr = (liteplayer_mngr_handle_t)msg->data;

    switch (msg->what) {
    case PLAYER_DO_SET_SOURCE: {
        const char *url = NULL;
        OS_THREAD_MUTEX_LOCK(mngr->lock);
        url = audio_strdup(mngr->url_list[mngr->url_index]);
        OS_THREAD_MUTEX_UNLOCK(mngr->lock);
        if (url != NULL) {
            liteplayer_set_data_source(mngr->player, url, mngr->threshold_ms);
            audio_free(url);
        }
        break;
    }

    case PLAYER_DO_PREPARE:
        liteplayer_prepare_async(mngr->player);
        break;

    case PLAYER_DO_START:
        liteplayer_start(mngr->player);
        break;

    case PLAYER_DO_PAUSE:
        liteplayer_pause(mngr->player);
        break;

    case PLAYER_DO_RESUME:
        liteplayer_resume(mngr->player);
        break;

    case PLAYER_DO_SEEK:
        liteplayer_seek(mngr->player, msg->arg1);
        break;

    case PLAYER_DO_NEXT: {
        OS_THREAD_MUTEX_LOCK(mngr->lock);
        if (mngr->is_list) {
            if (mngr->is_looping) {
                mngr->url_index++;
                if (mngr->url_index >= mngr->url_count)
                    mngr->url_index = 0;
            }
            mngr->is_completed = true;
        }
        OS_THREAD_MUTEX_UNLOCK(mngr->lock);
        if (mngr->is_list)
            liteplayer_stop(mngr->player);
        break;
    }

    case PLAYER_DO_PREV: {
        OS_THREAD_MUTEX_LOCK(mngr->lock);
        if (mngr->is_list) {
            mngr->url_index--;
            if (mngr->url_index < 0)
                mngr->url_index = mngr->url_count -1;
            if (!mngr->is_looping) {
                mngr->url_index--;
                if (mngr->url_index < 0)
                    mngr->url_index = mngr->url_count -1;
            }
            mngr->is_completed = true;
        }
        OS_THREAD_MUTEX_UNLOCK(mngr->lock);
        if (mngr->is_list)
            liteplayer_stop(mngr->player);
        break;
    }

    case PLAYER_DO_STOP:
        liteplayer_stop(mngr->player);
        break;

    case PLAYER_DO_RESET:
        liteplayer_reset(mngr->player);
        break;

    default:
        break;
    }
}

static void manager_looper_free(struct message *msg)
{
    // nothing to free
}

liteplayer_mngr_handle_t liteplayer_mngr_create()
{
    liteplayer_mngr_handle_t mngr = (liteplayer_mngr_handle_t)audio_calloc(1, sizeof(struct liteplayer_mngr));
    if (mngr != NULL) {
        mngr->lock = OS_THREAD_MUTEX_CREATE();
        if (mngr->lock == NULL)
            goto failed;

        mngr->player = liteplayer_create();
        if (mngr->player == NULL)
            goto failed;

        struct os_threadattr attr = {
            .name = "ael-manager",
            .priority = DEFAULT_MANAGER_TASK_PRIO,
            .stacksize = DEFAULT_MANAGER_TASK_STACKSIZE,
            .joinable = true,
        };
        mngr->looper = mlooper_create(&attr, manager_looper_handle, manager_looper_free);
        if (mngr->looper == NULL)
            goto failed;

        if (mlooper_start(mngr->looper) != 0)
            goto failed;
    }
    return mngr;

failed:
    liteplayer_mngr_destroy(mngr);
    return NULL;
}

int liteplayer_mngr_register_file_wrapper(liteplayer_mngr_handle_t mngr, struct file_wrapper *file_ops)
{
    if (mngr == NULL || file_ops == NULL)
        return -1;
    memcpy(&mngr->file_ops, file_ops, sizeof(struct file_wrapper));
    return liteplayer_register_file_wrapper(mngr->player, file_ops);
}

int liteplayer_mngr_register_http_wrapper(liteplayer_mngr_handle_t mngr, struct http_wrapper *http_ops)
{
    if (mngr == NULL || http_ops == NULL)
        return -1;
    memcpy(&mngr->http_ops, http_ops, sizeof(struct http_wrapper));
    return liteplayer_register_http_wrapper(mngr->player, http_ops);
}

int liteplayer_mngr_register_sink_wrapper(liteplayer_mngr_handle_t mngr, struct sink_wrapper *sink_ops)
{
    if (mngr == NULL || sink_ops == NULL)
        return -1;
    memcpy(&mngr->sink_ops, sink_ops, sizeof(struct sink_wrapper));
    return liteplayer_register_sink_wrapper(mngr->player, sink_ops);
}

int liteplayer_mngr_register_state_listener(liteplayer_mngr_handle_t mngr, liteplayer_state_cb listener, void *listener_priv)
{
    if (mngr == NULL)
        return -1;
    mngr->listener = listener;
    mngr->listener_priv = listener_priv;
    return 0;
}

int liteplayer_mngr_set_data_source(liteplayer_mngr_handle_t mngr, const char *url, int threshold_ms)
{
    if (mngr == NULL || url == NULL)
        return -1;

    OS_THREAD_MUTEX_LOCK(mngr->lock);
    if (mngr->url_count > 0) {
        OS_LOGE(TAG, "Failed to set source, playlist isn't empty");
        OS_THREAD_MUTEX_UNLOCK(mngr->lock);
        return -1;
    }
    mngr->is_list = false;
    mngr->is_completed = false;
    mngr->is_paused = false;
    OS_THREAD_MUTEX_UNLOCK(mngr->lock);

    if (strstr(url, DEFAULT_PLAYLIST_FILE_SUFFIX) != NULL) {
        if (playlist_resolve(mngr, url) != 0) {
            OS_LOGE(TAG, "Failed to resolve playlist");
            return -1;
        }
    }
    else {
        if (playlist_insert(mngr, url) != 0) {
            OS_LOGE(TAG, "Failed to insert playlist");
            return -1;
        }
    }

    mngr->threshold_ms = threshold_ms;
    liteplayer_register_state_listener(mngr->player, manager_state_callback, (void *)mngr);
    struct message *msg = message_obtain(PLAYER_DO_SET_SOURCE, 0, 0, mngr);
    if (msg != NULL) {
        mlooper_post_message(mngr->looper, msg);
        return 0;
    }
    return -1;
}

int liteplayer_mngr_prepare_async(liteplayer_mngr_handle_t mngr)
{
    if (mngr == NULL)
        return -1;
    struct message *msg = message_obtain(PLAYER_DO_PREPARE, 0, 0, mngr);
    if (msg != NULL) {
        mlooper_post_message(mngr->looper, msg);
        return 0;
    }
    return -1;
}

int liteplayer_mngr_write(liteplayer_mngr_handle_t mngr, char *data, int size, bool final)
{
    if (mngr == NULL)
        return -1;
    return liteplayer_write(mngr->player, data, size, final);
}

int liteplayer_mngr_start(liteplayer_mngr_handle_t mngr)
{
    if (mngr == NULL)
        return -1;
    struct message *msg = message_obtain(PLAYER_DO_START, 0, 0, mngr);
    if (msg != NULL) {
        mlooper_post_message(mngr->looper, msg);
        return 0;
    }
    return -1;
}

int liteplayer_mngr_pause(liteplayer_mngr_handle_t mngr)
{
    if (mngr == NULL)
        return -1;
    struct message *msg = message_obtain(PLAYER_DO_PAUSE, 0, 0, mngr);
    if (msg != NULL) {
        mlooper_post_message(mngr->looper, msg);
        return 0;
    }
    return -1;
}

int liteplayer_mngr_resume(liteplayer_mngr_handle_t mngr)
{
    if (mngr == NULL)
        return -1;
    struct message *msg = message_obtain(PLAYER_DO_RESUME, 0, 0, mngr);
    if (msg != NULL) {
        mlooper_post_message(mngr->looper, msg);
        return 0;
    }
    return -1;
}

int liteplayer_mngr_seek(liteplayer_mngr_handle_t mngr, int msec)
{
    if (mngr == NULL || msec < 0)
        return -1;
    struct message *msg = message_obtain(PLAYER_DO_SEEK, msec, 0, mngr);
    if (msg != NULL) {
        mlooper_post_message(mngr->looper, msg);
        return 0;
    }
    return -1;
}

int liteplayer_mngr_next(liteplayer_mngr_handle_t mngr)
{
    if (mngr == NULL)
        return -1;

    OS_THREAD_MUTEX_LOCK(mngr->lock);
    if (!mngr->is_list) {
        OS_LOGE(TAG, "Failed to switch next without playlist");
        OS_THREAD_MUTEX_UNLOCK(mngr->lock);
        return -1;
    }
    OS_THREAD_MUTEX_UNLOCK(mngr->lock);

    struct message *msg = message_obtain(PLAYER_DO_NEXT, 0, 0, mngr);
    if (msg != NULL) {
        mlooper_post_message(mngr->looper, msg);
        return 0;
    }
    return -1;
}

int liteplayer_mngr_prev(liteplayer_mngr_handle_t mngr)
{
    if (mngr == NULL)
        return -1;

    OS_THREAD_MUTEX_LOCK(mngr->lock);
    if (!mngr->is_list) {
        OS_LOGE(TAG, "Failed to switch prev without playlist");
        OS_THREAD_MUTEX_UNLOCK(mngr->lock);
        return -1;
    }
    OS_THREAD_MUTEX_UNLOCK(mngr->lock);

    struct message *msg = message_obtain(PLAYER_DO_PREV, 0, 0, mngr);
    if (msg != NULL) {
        mlooper_post_message(mngr->looper, msg);
        return 0;
    }
    return -1;
}

int liteplayer_mngr_set_single_looping(liteplayer_mngr_handle_t mngr, bool enable)
{
    if (mngr == NULL)
        return -1;

    OS_THREAD_MUTEX_LOCK(mngr->lock);

    if (mngr->is_looping != enable) {
        if (mngr->state == LITEPLAYER_INITED || mngr->state == LITEPLAYER_PREPARED ||
            mngr->state == LITEPLAYER_COMPLETED || mngr->state == LITEPLAYER_STOPPED ||
            mngr->state == LITEPLAYER_ERROR) {
            OS_LOGE(TAG, "Failed to set looping in critical state");
            OS_THREAD_MUTEX_UNLOCK(mngr->lock);
            return -1;
        }
        mngr->is_looping = enable;
    }

    OS_THREAD_MUTEX_UNLOCK(mngr->lock);
    return 0;
}

int liteplayer_mngr_stop(liteplayer_mngr_handle_t mngr)
{
    if (mngr == NULL)
        return -1;

    playlist_clear(mngr);

    struct message *msg = message_obtain(PLAYER_DO_STOP, 0, 0, mngr);
    if (msg != NULL) {
        mlooper_post_message(mngr->looper, msg);
        return 0;
    }
    return -1;
}

int liteplayer_mngr_reset(liteplayer_mngr_handle_t mngr)
{
    if (mngr == NULL)
        return -1;

    playlist_clear(mngr);

    struct message *msg = message_obtain(PLAYER_DO_RESET, 0, 0, mngr);
    if (msg != NULL) {
        mlooper_post_message(mngr->looper, msg);
        return 0;
    }
    return -1;
}

int liteplayer_mngr_get_available_size(liteplayer_mngr_handle_t mngr)
{
    if (mngr == NULL)
        return -1;
    return liteplayer_get_available_size(mngr->player);
}

int liteplayer_mngr_get_position(liteplayer_mngr_handle_t mngr, int *msec)
{
    if (mngr == NULL || msec == NULL)
        return -1;
    return liteplayer_get_position(mngr->player, msec);
}

int liteplayer_mngr_get_duration(liteplayer_mngr_handle_t mngr, int *msec)
{
    if (mngr == NULL || msec == NULL)
        return -1;
    return liteplayer_get_duration(mngr->player, msec);
}

void liteplayer_mngr_destroy(liteplayer_mngr_handle_t mngr)
{
    if (mngr == NULL)
        return;
    if (mngr->looper != NULL)
        mlooper_destroy(mngr->looper);
    if (mngr->player != NULL)
        liteplayer_destroy(mngr->player);
    if (mngr->lock != NULL)
        OS_THREAD_MUTEX_DESTROY(mngr->lock);
    audio_free(mngr);
}
