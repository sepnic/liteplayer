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
#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "liteplayer_main.h"
#include "source_httpclient_wrapper.h"
#include "source_file_wrapper.h"
#if defined(ENABLE_LINUX_ALSA)
#include "sink_alsa_wrapper.h"
#else
#include "sink_wave_wrapper.h"
#endif

#define TAG "basic_demo"

static int basic_demo_state_listener(enum liteplayer_state state, int errcode, void *priv)
{
    enum liteplayer_state *player_state = (enum liteplayer_state *)priv;
    bool state_sync = true;

    switch (state) {
    case LITEPLAYER_IDLE:
        OS_LOGD(TAG, "-->LITEPLAYER_IDLE");
        break;
    case LITEPLAYER_INITED:
        OS_LOGD(TAG, "-->LITEPLAYER_INITED");
        break;
    case LITEPLAYER_PREPARED:
        OS_LOGD(TAG, "-->LITEPLAYER_PREPARED");
        break;
    case LITEPLAYER_STARTED:
        OS_LOGD(TAG, "-->LITEPLAYER_STARTED");
        break;
    case LITEPLAYER_PAUSED:
        OS_LOGD(TAG, "-->LITEPLAYER_PAUSED");
        break;
    case LITEPLAYER_NEARLYCOMPLETED:
        OS_LOGD(TAG, "-->LITEPLAYER_NEARLYCOMPLETED");
        state_sync = false;
        break;
    case LITEPLAYER_COMPLETED:
        OS_LOGD(TAG, "-->LITEPLAYER_COMPLETED");
        break;
    case LITEPLAYER_STOPPED:
        OS_LOGD(TAG, "-->LITEPLAYER_STOPPED");
        break;
    case LITEPLAYER_ERROR:
        OS_LOGE(TAG, "-->LITEPLAYER_ERROR: %d", errcode);
        break;
    default:
        OS_LOGE(TAG, "-->LITEPLAYER_UNKNOWN: %d", state);
        state_sync = false;
        break;
    }

    if (state_sync)
        *player_state = state;
    return 0;
}

static int basic_demo(const char *url)
{
    int ret = -1;
    liteplayer_handle_t player = liteplayer_create();
    if (player == NULL)
        return ret;

    enum liteplayer_state player_state = LITEPLAYER_IDLE;
    liteplayer_register_state_listener(player, basic_demo_state_listener, (void *)&player_state);

#if defined(ENABLE_LINUX_ALSA)
    struct sink_wrapper sink_ops = {
        .priv_data = NULL,
        .name = alsa_wrapper_name,
        .open = alsa_wrapper_open,
        .write = alsa_wrapper_write,
        .close = alsa_wrapper_close,
    };
#else
    struct sink_wrapper sink_ops = {
        .priv_data = NULL,
        .name = wave_wrapper_name,
        .open = wave_wrapper_open,
        .write = wave_wrapper_write,
        .close = wave_wrapper_close,
    };
#endif
    liteplayer_register_sink_wrapper(player, &sink_ops);

    struct source_wrapper file_ops = {
        .async_mode = false,
        .buffer_size = 2*1024,
        .priv_data = NULL,
        .url_protocol = file_wrapper_url_protocol,
        .open = file_wrapper_open,
        .read = file_wrapper_read,
        .content_len = file_wrapper_content_len,
        .seek = file_wrapper_seek,
        .close = file_wrapper_close,
    };
    liteplayer_register_source_wrapper(player, &file_ops);

    struct source_wrapper http_ops = {
        .async_mode = true,
        .buffer_size = 256*1024,
        .priv_data = NULL,
        .url_protocol = httpclient_wrapper_url_protocol,
        .open = httpclient_wrapper_open,
        .read = httpclient_wrapper_read,
        .content_len = httpclient_wrapper_content_len,
        .seek = httpclient_wrapper_seek,
        .close = httpclient_wrapper_close,
    };
    liteplayer_register_source_wrapper(player, &http_ops);

    if (liteplayer_set_data_source(player, url) != 0) {
        OS_LOGE(TAG, "Failed to set data source");
        goto test_done;
    }

    if (liteplayer_prepare_async(player) != 0) {
        OS_LOGE(TAG, "Failed to prepare player");
        goto test_done;
    }
    while (player_state != LITEPLAYER_PREPARED && player_state != LITEPLAYER_ERROR) {
        os_thread_sleep_msec(100);
    }
    if (player_state == LITEPLAYER_ERROR) {
        OS_LOGE(TAG, "Failed to prepare player");
        goto test_done;
    }

    if (liteplayer_start(player) != 0) {
        OS_LOGE(TAG, "Failed to start player");
        goto test_done;
    }
    OS_MEMORY_DUMP();
    while (player_state != LITEPLAYER_COMPLETED && player_state != LITEPLAYER_ERROR) {
        os_thread_sleep_msec(100);
    }

    if (liteplayer_stop(player) != 0) {
        OS_LOGE(TAG, "Failed to stop player");
        goto test_done;
    }
    while (player_state != LITEPLAYER_STOPPED) {
        os_thread_sleep_msec(100);
    }

    ret = 0;

test_done:
    liteplayer_reset(player);
    while (player_state != LITEPLAYER_IDLE) {
        os_thread_sleep_msec(100);
    }

    liteplayer_destroy(player);

    os_thread_sleep_msec(100);
    OS_MEMORY_DUMP();
    return ret;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        OS_LOGW(TAG, "Usage: %s [url]", argv[0]);
        return -1;
    }

    basic_demo(argv[1]);
    return 0;
}
