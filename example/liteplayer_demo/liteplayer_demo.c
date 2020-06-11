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
#include "msgutils/os_memory.h"
#include "msgutils/os_logger.h"
#include "liteplayer_main.h"
#include "httpclient_wrapper.h"
#include "fatfs_wrapper.h"
#if defined(ENABLE_TINYALSA)
#include "tinyalsa_wrapper.h"
#else
#include "wave_wrapper.h"
#endif

#define TAG "[liteplayer]DEMO"

#define LITEPLYAER_DEMO_THRESHOLD_MS (5000)

static int liteplayer_demo_state_listener(enum liteplayer_state state, int errcode, void *priv)
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
    case LITEPLAYER_CACHECOMPLETED:
        OS_LOGD(TAG, "-->LITEPLAYER_CACHECOMPLETED");
        state_sync = false;
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

static int liteplayer_demo(const char *url)
{
    int ret = -1;
    liteplayer_handle_t player = liteplayer_create();
    if (player == NULL)
        return ret;

    enum liteplayer_state player_state = LITEPLAYER_IDLE;
    liteplayer_register_state_listener(player, liteplayer_demo_state_listener, (void *)&player_state);

#if defined(ENABLE_TINYALSA)
    struct sink_wrapper sink_ops = {
        .sink_priv = NULL,
        .open = tinyalsa_wrapper_open,
        .write = tinyalsa_wrapper_write,
        .close = tinyalsa_wrapper_close,
    };
#else
    struct sink_wrapper sink_ops = {
        .sink_priv = NULL,
        .open = wave_wrapper_open,
        .write = wave_wrapper_write,
        .close = wave_wrapper_close,
    };
#endif
    liteplayer_register_sink_wrapper(player, &sink_ops);

    struct file_wrapper file_ops = {
        .file_priv = NULL,
        .open = fatfs_wrapper_open,
        .read = fatfs_wrapper_read,
        .filesize = fatfs_wrapper_filesize,
        .seek = fatfs_wrapper_seek,
        .close = fatfs_wrapper_close,
    };
    liteplayer_register_file_wrapper(player, &file_ops);

    struct http_wrapper http_ops = {
        .http_priv = NULL,
        .open = httpclient_wrapper_open,
        .read = httpclient_wrapper_read,
        .filesize = httpclient_wrapper_filesize,
        .seek = httpclient_wrapper_seek,
        .close = httpclient_wrapper_close,
    };
    liteplayer_register_http_wrapper(player, &http_ops);

    if (liteplayer_set_data_source(player, url, LITEPLYAER_DEMO_THRESHOLD_MS) != 0) {
        OS_LOGE(TAG, "Failed to set data source");
        goto test_done;
    }

    if (liteplayer_prepare_async(player) != 0) {
        OS_LOGE(TAG, "Failed to prepare player");
        goto test_done;
    }
    while (player_state != LITEPLAYER_PREPARED && player_state != LITEPLAYER_ERROR) {
        OS_THREAD_SLEEP_MSEC(100);
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
        OS_THREAD_SLEEP_MSEC(100);
    }

    if (liteplayer_stop(player) != 0) {
        OS_LOGE(TAG, "Failed to stop player");
        goto test_done;
    }
    while (player_state != LITEPLAYER_STOPPED) {
        OS_THREAD_SLEEP_MSEC(100);
    }

    ret = 0;

test_done:
    liteplayer_reset(player);
    while (player_state != LITEPLAYER_IDLE) {
        OS_THREAD_SLEEP_MSEC(100);
    }

    liteplayer_destroy(player);

    OS_THREAD_SLEEP_MSEC(100);
    OS_MEMORY_DUMP();
    return ret;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        OS_LOGW(TAG, "Usage: %s [url]", argv[0]);
        return -1;
    }

    liteplayer_demo(argv[1]);
    return 0;
}
