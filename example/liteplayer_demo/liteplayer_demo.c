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
#include "esp_adf/esp_log.h"
#include "esp_adf/audio_common.h"

#include "liteplayer_main.h"
#include "fatfs_wrapper.h"
#include "httpstub_wrapper.h"
#include "alsastub_wrapper.h"

#define TAG "liteplayer_test"

#define LITEPLYAER_TEST_TASK_PRIO  (OS_THREAD_PRIO_NORMAL)
#define LITEPLYAER_TEST_TASK_STACK (8192)

static liteplayer_state_t g_state = LITEPLAYER_ERROR;

static int liteplayer_test_state_listener(liteplayer_state_t state, int errcode, void *priv)
{
    switch (state) {
    case LITEPLAYER_IDLE:
        ESP_LOGD(TAG, "-->LITEPLAYER_IDLE");
        g_state = state;
        break;
    case LITEPLAYER_PREPARED:
        ESP_LOGD(TAG, "-->LITEPLAYER_PREPARED");
        g_state = state;
        break;
    case LITEPLAYER_STARTED:
        ESP_LOGD(TAG, "-->LITEPLAYER_STARTED");
        g_state = state;
        break;
    case LITEPLAYER_PAUSED:
        ESP_LOGD(TAG, "-->LITEPLAYER_PAUSED");
        g_state = state;
        break;
    case LITEPLAYER_NEARLYCOMPLETED:
        ESP_LOGD(TAG, "-->LITEPLAYER_NEARLYCOMPLETED");
        break;
    case LITEPLAYER_COMPLETED:
        ESP_LOGD(TAG, "-->LITEPLAYER_COMPLETED");
        g_state = state;
        break;
    case LITEPLAYER_STOPPED:
        ESP_LOGD(TAG, "-->LITEPLAYER_STOPPED");
        g_state = state;
        break;
    case LITEPLAYER_ERROR:
        ESP_LOGD(TAG, "-->LITEPLAYER_ERROR: %d", errcode);
        g_state = state;
        break;
    default:
        break;
    }

    return 0;
}

static void *liteplayer_test_thread(void *arg)
{
    const char *url = (const char *)arg;
    liteplayer_handle_t player = liteplayer_create();
    long long positon = 0, duration = 0;

    if (player == NULL)
        return NULL;

    ESP_LOGW(TAG, "Memory dump after liteplayer_create");
    OS_MEMORY_DUMP();

    liteplayer_register_state_listener(player, liteplayer_test_state_listener, (void *)player);

    alsa_wrapper_t alsa_wrapper = {
        .alsa_priv = NULL,
        .open = alsastub_wrapper_open,
        .write = alsastub_wrapper_write,
        .close = alsastub_wrapper_close,
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
        .open = httpstub_wrapper_open,
        .read = httpstub_wrapper_read,
        .filesize = httpstub_wrapper_filesize,
        .seek = httpstub_wrapper_seek,
        .close = httpstub_wrapper_close,
    };
    liteplayer_register_http_wrapper(player, &http_wrapper);

    if (liteplayer_set_data_source(player, url) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set data source");
        goto test_done;
    }
    ESP_LOGW(TAG, "Memory dump after liteplayer_set_data_source");
    OS_MEMORY_DUMP();

    if (liteplayer_prepare_async(player) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to prepare player");
        goto test_done;
    }
    ESP_LOGW(TAG, "Memory dump after liteplayer_prepare");
    OS_MEMORY_DUMP();

    while (g_state != LITEPLAYER_PREPARED) {
        OS_THREAD_SLEEP_MSEC(100);
    }

    liteplayer_get_duration(player, &duration);
    ESP_LOGI(TAG, "Media duration: %d(ms)", (int)duration);

    if (liteplayer_start(player) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start player");
        goto test_done;
    }
    ESP_LOGW(TAG, "Memory dump after liteplayer_start");
    OS_MEMORY_DUMP();

    while (g_state != LITEPLAYER_COMPLETED && g_state != LITEPLAYER_ERROR) {
        OS_THREAD_SLEEP_MSEC(100);
    }

    if (liteplayer_stop(player) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop player");
        goto test_done;
    }
    ESP_LOGW(TAG, "Memory dump after liteplayer_stop");
    OS_MEMORY_DUMP();

    while (g_state != LITEPLAYER_STOPPED) {
        OS_THREAD_SLEEP_MSEC(50);
    }

    liteplayer_get_position(player, &positon);
    ESP_LOGI(TAG, "Media playdone postion: %d(ms)", (int)positon);

test_done:
    liteplayer_reset(player);
    liteplayer_destroy(player);
    ESP_LOGW(TAG, "Memory dump after liteplayer_destroy");
    OS_MEMORY_DUMP();
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        ESP_LOGW(TAG, "Usage: %s [url]", argv[0]);
        return 0;
    }

    const char *url = argv[1];
    struct os_threadattr attr = {
        .name = "liteplayer_test",
        .priority = LITEPLYAER_TEST_TASK_PRIO,
        .stacksize = LITEPLYAER_TEST_TASK_STACK,
        .joinable = true,
    };
    os_thread_t id = OS_THREAD_CREATE(&attr, liteplayer_test_thread, (void *)url);
    if (id != NULL)
        OS_THREAD_JOIN(id, NULL);
    return 0;
}
