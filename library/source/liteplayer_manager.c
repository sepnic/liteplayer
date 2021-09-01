/*
 * Copyright (c) 2019-2021 Qinglong <sysu.zqlong@gmail.com>
 *
 * This file is part of Liteplayer
 * (see https://github.com/sepnic/liteplayer_priv).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 2.1 of the License, or
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

#include "osal/os_thread.h"
#include "cutils/log_helper.h"
#include "cutils/mlooper.h"
#include "esp_adf/audio_common.h"
#include "liteplayer_adapter_internal.h"
#include "liteplayer_adapter.h"
#include "liteplayer_config.h"
#include "liteplayer_main.h"
#include "liteplayer_manager.h"

#define TAG "[liteplayer]MANAGER"

#define DEFAULT_PLAYLIST_BUFFER_SIZE  (1024*32)

struct liteplayer_manager {
    liteplayer_handle_t  player;
    mlooper_handle       looper;
    os_mutex             lock;
    int                  threshold_ms;
    enum liteplayer_state state;
    liteplayer_state_cb  listener;
    void                *listener_priv;

    liteplayer_adapter_handle_t adapter_handle;
    struct source_wrapper *file_ops;

    char                *url_list[DEFAULT_PLAYLIST_URL_MAX];
    int                  url_index;
    int                  url_count;

    bool                 is_list;
    bool                 is_paused;
    bool                 is_completed;
    bool                 is_looping;
    bool                 has_started;
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

static void playlist_clear(liteplayermanager_handle_t mngr)
{
    os_mutex_lock(mngr->lock);

    for (int i = 0; i < mngr->url_count; i++) {
        audio_free(mngr->url_list[i]);
    }
    mngr->url_index = 0;
    mngr->url_count = 0;
    mngr->is_list = false;

    os_mutex_unlock(mngr->lock);
}

static int playlist_insert(liteplayermanager_handle_t mngr, const char *url)
{
    os_mutex_lock(mngr->lock);

    if (mngr->url_count >= DEFAULT_PLAYLIST_URL_MAX) {
        OS_LOGE(TAG, "Reach max url count: %d", mngr->url_count);
        os_mutex_unlock(mngr->lock);
        return -1;
    }

    char *insert = audio_strdup(url);
    if (insert == NULL) {
        os_mutex_unlock(mngr->lock);
        return -1;
    }
    mngr->url_list[mngr->url_count] = insert;
    mngr->url_count++;

    os_mutex_unlock(mngr->lock);
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
            } else if (line_end) {
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

static int playlist_resolve(liteplayermanager_handle_t mngr, const char *filename)
{
    int ret = -1;
    source_handle_t file = NULL;
    char *content = audio_malloc(DEFAULT_PLAYLIST_BUFFER_SIZE);
    if (content == NULL) {
        OS_LOGE(TAG, "Failed to allocate playlist parser buffer");
        goto resolve_done;
    }

    file = mngr->file_ops->open(filename, 0, mngr->file_ops->priv_data);
    if (file == NULL) {
        OS_LOGE(TAG, "Failed to open playlist");
        goto resolve_done;
    }

    int bytes_read = mngr->file_ops->read(file, content, DEFAULT_PLAYLIST_BUFFER_SIZE);
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

    os_mutex_lock(mngr->lock);
    if (mngr->url_count > 0) {
        mngr->is_list = true;
        ret = 0;
    }
#if 1
    for (int i = 0; i < mngr->url_count; i++) {
        OS_LOGV(TAG, "-->url[%d]=[%s]", i, mngr->url_list[i]);
    }
#endif
    os_mutex_unlock(mngr->lock);

resolve_done:
    if (file != NULL)
        mngr->file_ops->close(file);
    if (content != NULL)
        audio_free(content);
    return ret;
}

static int manager_state_callback(enum liteplayer_state state, int errcode, void *priv)
{
    liteplayermanager_handle_t mngr = (liteplayermanager_handle_t)priv;
    bool state_sync = true;

    os_mutex_lock(mngr->lock);

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
        if (mngr->has_started) {
            state_sync = mngr->is_paused;
        }
        mngr->is_completed = false;
        mngr->is_paused = false;
        mngr->has_started = true;
        break;

    case LITEPLAYER_PAUSED:
        mngr->is_paused = true;
        break;

    case LITEPLAYER_SEEKCOMPLETED:
        state_sync = false;
        if (!mngr->is_paused) {
            struct message *msg = message_obtain(PLAYER_DO_START, 0, 0, mngr);
            if (msg != NULL)
                mlooper_post_message(mngr->looper, msg);
        }
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

    os_mutex_unlock(mngr->lock);

    if (state_sync && mngr->listener)
        mngr->listener(state, errcode, mngr->listener_priv);
    return 0;
}

static void manager_looper_handle(struct message *msg)
{
    liteplayermanager_handle_t mngr = (liteplayermanager_handle_t)msg->data;

    switch (msg->what) {
    case PLAYER_DO_SET_SOURCE: {
        const char *url = NULL;
        os_mutex_lock(mngr->lock);
        url = audio_strdup(mngr->url_list[mngr->url_index]);
        os_mutex_unlock(mngr->lock);
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
        os_mutex_lock(mngr->lock);
        if (mngr->is_list) {
            if (mngr->is_looping) {
                mngr->url_index++;
                if (mngr->url_index >= mngr->url_count)
                    mngr->url_index = 0;
            }
            mngr->is_completed = true;
        }
        os_mutex_unlock(mngr->lock);
        if (mngr->is_list)
            liteplayer_stop(mngr->player);
        break;
    }

    case PLAYER_DO_PREV: {
        os_mutex_lock(mngr->lock);
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
        os_mutex_unlock(mngr->lock);
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

liteplayermanager_handle_t liteplayermanager_create()
{
    liteplayermanager_handle_t mngr = audio_calloc(1, sizeof(struct liteplayer_manager));
    if (mngr != NULL) {
        mngr->lock = os_mutex_create();
        if (mngr->lock == NULL)
            goto failed;

        mngr->adapter_handle = liteplayer_adapter_init();
        if (mngr->adapter_handle == NULL)
            goto failed;

        mngr->player = liteplayer_create();
        if (mngr->player == NULL)
            goto failed;

        struct os_thread_attr attr = {
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
    liteplayermanager_destroy(mngr);
    return NULL;
}

int liteplayermanager_register_source_wrapper(liteplayermanager_handle_t mngr, struct source_wrapper *wrapper)
{
    if (mngr == NULL || wrapper == NULL)
        return -1;
    mngr->adapter_handle->add_source_wrapper(mngr->adapter_handle, wrapper);
    return liteplayer_register_source_wrapper(mngr->player, wrapper);
}

int liteplayermanager_set_prefered_source_wrapper(liteplayermanager_handle_t mngr, struct source_wrapper *wrapper)
{
    if (mngr == NULL || wrapper == NULL)
        return -1;
    return liteplayer_set_prefered_source_wrapper(mngr->player, wrapper);
}

int liteplayermanager_register_sink_wrapper(liteplayermanager_handle_t mngr, struct sink_wrapper *wrapper)
{
    if (mngr == NULL || wrapper == NULL)
        return -1;
    mngr->adapter_handle->add_sink_wrapper(mngr->adapter_handle, wrapper);
    return liteplayer_register_sink_wrapper(mngr->player, wrapper);
}

int liteplayermanager_set_prefered_sink_wrapper(liteplayermanager_handle_t mngr, struct sink_wrapper *wrapper)
{
    if (mngr == NULL || wrapper == NULL)
        return -1;
    return liteplayer_set_prefered_sink_wrapper(mngr->player, wrapper);
}

int liteplayermanager_register_state_listener(liteplayermanager_handle_t mngr, liteplayer_state_cb listener, void *listener_priv)
{
    if (mngr == NULL)
        return -1;
    mngr->listener = listener;
    mngr->listener_priv = listener_priv;
    return 0;
}

int liteplayermanager_set_data_source(liteplayermanager_handle_t mngr, const char *url, int threshold_ms)
{
    if (mngr == NULL || url == NULL)
        return -1;

    os_mutex_lock(mngr->lock);
    if (mngr->url_count > 0) {
        OS_LOGE(TAG, "Failed to set source, playlist isn't empty");
        os_mutex_unlock(mngr->lock);
        return -1;
    }
    mngr->is_list = false;
    mngr->is_completed = false;
    mngr->is_paused = false;
    mngr->has_started = false;

    if (mngr->file_ops == NULL) {
        mngr->file_ops = mngr->adapter_handle->find_source_wrapper(mngr->adapter_handle, url);
        if (mngr->file_ops == NULL) {
            OS_LOGE(TAG, "Can't find source wrapper for this url");
            os_mutex_unlock(mngr->lock);
            return -1;
        }
    }
    os_mutex_unlock(mngr->lock);

    if (strstr(url, DEFAULT_PLAYLIST_FILE_SUFFIX) != NULL) {
        if (playlist_resolve(mngr, url) != 0) {
            OS_LOGE(TAG, "Failed to resolve playlist");
            return -1;
        }
    } else {
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

int liteplayermanager_prepare_async(liteplayermanager_handle_t mngr)
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

int liteplayermanager_start(liteplayermanager_handle_t mngr)
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

int liteplayermanager_pause(liteplayermanager_handle_t mngr)
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

int liteplayermanager_resume(liteplayermanager_handle_t mngr)
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

int liteplayermanager_seek(liteplayermanager_handle_t mngr, int msec)
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

int liteplayermanager_next(liteplayermanager_handle_t mngr)
{
    if (mngr == NULL)
        return -1;

    os_mutex_lock(mngr->lock);
    if (!mngr->is_list) {
        OS_LOGE(TAG, "Failed to switch next without playlist");
        os_mutex_unlock(mngr->lock);
        return -1;
    }
    os_mutex_unlock(mngr->lock);

    struct message *msg = message_obtain(PLAYER_DO_NEXT, 0, 0, mngr);
    if (msg != NULL) {
        mlooper_post_message(mngr->looper, msg);
        return 0;
    }
    return -1;
}

int liteplayermanager_prev(liteplayermanager_handle_t mngr)
{
    if (mngr == NULL)
        return -1;

    os_mutex_lock(mngr->lock);
    if (!mngr->is_list) {
        OS_LOGE(TAG, "Failed to switch prev without playlist");
        os_mutex_unlock(mngr->lock);
        return -1;
    }
    os_mutex_unlock(mngr->lock);

    struct message *msg = message_obtain(PLAYER_DO_PREV, 0, 0, mngr);
    if (msg != NULL) {
        mlooper_post_message(mngr->looper, msg);
        return 0;
    }
    return -1;
}

int liteplayermanager_set_single_looping(liteplayermanager_handle_t mngr, bool enable)
{
    if (mngr == NULL)
        return -1;

    os_mutex_lock(mngr->lock);

    if (mngr->is_looping != enable) {
        if (mngr->state == LITEPLAYER_INITED || mngr->state == LITEPLAYER_PREPARED ||
            mngr->state == LITEPLAYER_COMPLETED || mngr->state == LITEPLAYER_STOPPED ||
            mngr->state == LITEPLAYER_ERROR) {
            OS_LOGE(TAG, "Failed to set looping in critical state");
            os_mutex_unlock(mngr->lock);
            return -1;
        }
        mngr->is_looping = enable;
    }

    os_mutex_unlock(mngr->lock);
    return 0;
}

int liteplayermanager_stop(liteplayermanager_handle_t mngr)
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

int liteplayermanager_reset(liteplayermanager_handle_t mngr)
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

int liteplayermanager_get_position(liteplayermanager_handle_t mngr, int *msec)
{
    if (mngr == NULL || msec == NULL)
        return -1;
    return liteplayer_get_position(mngr->player, msec);
}

int liteplayermanager_get_duration(liteplayermanager_handle_t mngr, int *msec)
{
    if (mngr == NULL || msec == NULL)
        return -1;
    return liteplayer_get_duration(mngr->player, msec);
}

void liteplayermanager_destroy(liteplayermanager_handle_t mngr)
{
    if (mngr == NULL)
        return;
    if (mngr->looper != NULL)
        mlooper_destroy(mngr->looper);
    if (mngr->player != NULL)
        liteplayer_destroy(mngr->player);
    if (mngr->lock != NULL)
        os_mutex_destroy(mngr->lock);
    if (mngr->adapter_handle != NULL)
        mngr->adapter_handle->destory(mngr->adapter_handle);
    audio_free(mngr);
}
