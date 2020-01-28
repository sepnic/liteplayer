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

#include "msgutils/os_memory.h"
#include "esp_adf/esp_log.h"
#include "esp_adf/audio_common.h"
#include "liteplayer_main.h"
#include "httpclient_wrapper.h"
#include "fatfs_wrapper.h"
#include "pcmout_wrapper.h"

#define TAG "liteplayerdemo"

#define LITEPLYAER_TEST_TASK_PRIO  (OS_THREAD_PRIO_NORMAL)
#define LITEPLYAER_TEST_TASK_STACK (8192)

static liteplayer_state_t g_state = LITEPLAYER_ERROR;

static int liteplayer_test_state_listener(liteplayer_state_t state, int errcode, void *priv)
{
    switch (state) {
    case LITEPLAYER_IDLE:
        ESP_LOGD(TAG, "-->LITEPLAYER_IDLE");
        break;
    case LITEPLAYER_PREPARED:
        ESP_LOGD(TAG, "-->LITEPLAYER_PREPARED");
        break;
    case LITEPLAYER_STARTED:
        ESP_LOGD(TAG, "-->LITEPLAYER_STARTED");
        break;
    case LITEPLAYER_PAUSED:
        ESP_LOGD(TAG, "-->LITEPLAYER_PAUSED");
        break;
    case LITEPLAYER_NEARLYCOMPLETED:
        ESP_LOGD(TAG, "-->LITEPLAYER_NEARLYCOMPLETED");
        break;
    case LITEPLAYER_COMPLETED:
        ESP_LOGD(TAG, "-->LITEPLAYER_COMPLETED");
        break;
    case LITEPLAYER_STOPPED:
        ESP_LOGD(TAG, "-->LITEPLAYER_STOPPED");
        break;
    case LITEPLAYER_ERROR:
        ESP_LOGD(TAG, "-->LITEPLAYER_ERROR: %d", errcode);
        break;
    default:
        break;
    }

    g_state = state;
    return 0;
}

static int liteplayer_test(const char *url)
{
    int ret = -1;
    liteplayer_handle_t player = liteplayer_create();
    if (player == NULL)
        return ret;

    liteplayer_register_state_listener(player, liteplayer_test_state_listener, (void *)player);

    alsa_wrapper_t alsa_wrapper = {
        .alsa_priv = NULL,
        .open = pcmout_wrapper_open,
        .write = pcmout_wrapper_write,
        .close = pcmout_wrapper_close,
    };
    liteplayer_register_alsa_wrapper(player, &alsa_wrapper);

    fatfs_wrapper_t fatfs_wrapper = {
        .fatfs_priv = NULL,
        .open = fatfs_wrapper_open,
        .read = fatfs_wrapper_read,
        .write = fatfs_wrapper_write,
        .filesize = fatfs_wrapper_filesize,
        .seek = fatfs_wrapper_seek,
        .close = fatfs_wrapper_close,
    };
    liteplayer_register_fatfs_wrapper(player, &fatfs_wrapper);

    http_wrapper_t http_wrapper = {
        .http_priv = NULL,
        .open = httpclient_wrapper_open,
        .read = httpclient_wrapper_read,
        .filesize = httpclient_wrapper_filesize,
        .seek = httpclient_wrapper_seek,
        .close = httpclient_wrapper_close,
    };
    liteplayer_register_http_wrapper(player, &http_wrapper);

    if (liteplayer_set_data_source(player, url) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set data source");
        goto test_done;
    }

    if (liteplayer_prepare_async(player) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to prepare player");
        goto test_done;
    }

    while (g_state != LITEPLAYER_PREPARED && g_state != LITEPLAYER_ERROR) {
        OS_THREAD_SLEEP_MSEC(100);
    }

    if (g_state == LITEPLAYER_ERROR) {
        ESP_LOGE(TAG, "Failed to prepare player");
        goto test_done;
    }

    if (liteplayer_start(player) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start player");
        goto test_done;
    }

    OS_MEMORY_DUMP();

    while (g_state != LITEPLAYER_COMPLETED && g_state != LITEPLAYER_ERROR) {
        OS_THREAD_SLEEP_MSEC(100);
    }

    if (liteplayer_stop(player) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop player");
        goto test_done;
    }

    while (g_state != LITEPLAYER_STOPPED) {
        OS_THREAD_SLEEP_MSEC(50);
    }

    ret = 0;

test_done:
    liteplayer_reset(player);
    liteplayer_destroy(player);

    OS_THREAD_SLEEP_MSEC(100);
    OS_MEMORY_DUMP();
    return ret;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        ESP_LOGW(TAG, "Usage: %s [url]", argv[0]);
        return 0;
    }

    liteplayer_test(argv[1]);
    return 0;
}
