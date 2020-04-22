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
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "msgutils/os_memory.h"
#include "msgutils/os_logger.h"
#include "esp_adf/audio_common.h"
#include "liteplayer_main.h"
#include "liteplayer_manager.h"
#include "httpclient_wrapper.h"
#include "fatfs_wrapper.h"
#if defined(ENABLE_TINYALSA)
#include "tinyalsa_wrapper.h"
#else
#include "wave_wrapper.h"
#endif

#define TAG "liteplayermngr_demo"

#define PLAYLIST_FILE "liteplayermngr_demo.playlist"

#define LITEPLYAER_TEST_TASK_PRIO  (OS_THREAD_PRIO_NORMAL)
#define LITEPLYAER_TEST_TASK_STACK (8192)

static liteplayer_state_t g_state = LITEPLAYER_IDLE;
static liteplayer_mngr_handle_t g_player = NULL;
static bool g_exit = false;

static int generate_playlist(const char *path)
{
    DIR *dir = NULL;
    if ((dir = opendir(path)) == NULL) {
        OS_LOGE(TAG, "Failed to open dir[%s]", path);
        return -1;
    }
    else {
        struct dirent *entry;
        char buffer[256];
        FILE *file = fopen(PLAYLIST_FILE, "wb+");
        if (file == NULL) {
            OS_LOGE(TAG, "Failed to open playlist file");
            closedir(dir);
            return -1;
        }

        while ((entry = readdir(dir)) != NULL) {
            OS_LOGD(TAG, "-->d_name=[%s], d_type=[%d]", entry->d_name, entry->d_type);
            if (entry->d_type == DT_REG &&
                (strstr(entry->d_name, ".mp3") != NULL ||
                 strstr(entry->d_name, ".m4a") != NULL ||
                 strstr(entry->d_name, ".wav") != NULL)) {
                snprintf(buffer, sizeof(buffer), "%s/%s\n", path, entry->d_name);
                fwrite(buffer, 1, strlen(buffer), file);
            }
        }
        buffer[0] = '\n';
        fwrite(buffer, 1, 1, file);

        fflush(file);
        fclose(file);
        closedir(dir);
        return 0;
    }

    return -1;
}

static int liteplayer_test_state_callback(liteplayer_state_t state, int errcode, void *priv)
{
    switch (state) {
    case LITEPLAYER_IDLE:
        OS_LOGI(TAG, "-->LITEPLAYER_IDLE");
        g_state = state;
        break;
    case LITEPLAYER_INITED:
        OS_LOGI(TAG, "-->LITEPLAYER_INITED");
        g_state = state;
        break;
    case LITEPLAYER_PREPARED:
        OS_LOGI(TAG, "-->LITEPLAYER_PREPARED");
        g_state = state;
        break;
    case LITEPLAYER_STARTED:
        OS_LOGI(TAG, "-->LITEPLAYER_STARTED");
        g_state = state;
        break;
    case LITEPLAYER_PAUSED:
        OS_LOGI(TAG, "-->LITEPLAYER_PAUSED");
        g_state = state;
        break;
    case LITEPLAYER_SEEKCOMPLETED:
        OS_LOGI(TAG, "-->LITEPLAYER_SEEKCOMPLETED");
        g_state = state;
        break;
    case LITEPLAYER_NEARLYCOMPLETED:
        OS_LOGI(TAG, "-->LITEPLAYER_NEARLYCOMPLETED");
        break;
    case LITEPLAYER_COMPLETED:
        OS_LOGI(TAG, "-->LITEPLAYER_COMPLETED");
        g_state = state;
        break;
    case LITEPLAYER_STOPPED:
        OS_LOGI(TAG, "-->LITEPLAYER_STOPPED");
        g_state = state;
        break;
    case LITEPLAYER_ERROR:
        OS_LOGI(TAG, "-->LITEPLAYER_ERROR: %d", errcode);
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

    g_player = liteplayer_mngr_create();
    if (g_player == NULL)
        return NULL;

    liteplayer_mngr_register_state_listener(g_player, liteplayer_test_state_callback, (void *)g_player);

#if defined(ENABLE_TINYALSA)
    sink_wrapper_t sink_wrapper = {
        .sink_priv = NULL,
        .open = tinyalsa_wrapper_open,
        .write = tinyalsa_wrapper_write,
        .close = tinyalsa_wrapper_close,
    };
#else
    sink_wrapper_t sink_wrapper = {
        .sink_priv = NULL,
        .open = wave_wrapper_open,
        .write = wave_wrapper_write,
        .close = wave_wrapper_close,
    };
#endif
    liteplayer_mngr_register_sink_wrapper(g_player, &sink_wrapper);

    file_wrapper_t file_wrapper = {
        .file_priv = NULL,
        .open = fatfs_wrapper_open,
        .read = fatfs_wrapper_read,
        .write = fatfs_wrapper_write,
        .filesize = fatfs_wrapper_filesize,
        .seek = fatfs_wrapper_seek,
        .close = fatfs_wrapper_close,
    };
    liteplayer_mngr_register_file_wrapper(g_player, &file_wrapper);

    http_wrapper_t http_wrapper = {
        .http_priv = NULL,
        .open = httpclient_wrapper_open,
        .read = httpclient_wrapper_read,
        .filesize = httpclient_wrapper_filesize,
        .seek = httpclient_wrapper_seek,
        .close = httpclient_wrapper_close,
    };
    liteplayer_mngr_register_http_wrapper(g_player, &http_wrapper);

    if (liteplayer_mngr_set_data_source(g_player, url) != ESP_OK) {
        OS_LOGE(TAG, "Failed to set data source");
        goto test_done;
    }

    if (liteplayer_mngr_prepare_async(g_player) != ESP_OK) {
        OS_LOGE(TAG, "Failed to prepare player");
        goto test_done;
    }

    while (g_state != LITEPLAYER_PREPARED && g_state != LITEPLAYER_ERROR) {
        OS_THREAD_SLEEP_MSEC(100);
    }

    if (g_state == LITEPLAYER_ERROR) {
        OS_LOGE(TAG, "Failed to prepare player");
        goto test_done;
    }

    if (liteplayer_mngr_start(g_player) != ESP_OK) {
        OS_LOGE(TAG, "Failed to start player");
        goto test_done;
    }

    OS_MEMORY_DUMP();

    while (g_state != LITEPLAYER_COMPLETED && g_state != LITEPLAYER_ERROR &&
           g_state != LITEPLAYER_STOPPED && g_state != LITEPLAYER_IDLE) {
        if (g_state == LITEPLAYER_SEEKCOMPLETED) {
            if (liteplayer_mngr_start(g_player) != ESP_OK) {
                OS_LOGE(TAG, "Failed to start player");
                goto test_done;
            }
        }
        OS_THREAD_SLEEP_MSEC(100);
    }

    if (g_state == LITEPLAYER_STOPPED || g_state == LITEPLAYER_IDLE) {
        OS_LOGD(TAG, "Player has been stopped");
        goto test_done;
    }

    if (liteplayer_mngr_stop(g_player) != ESP_OK) {
        OS_LOGE(TAG, "Failed to stop player");
        goto test_done;
    }

    while (g_state != LITEPLAYER_STOPPED) {
        OS_THREAD_SLEEP_MSEC(50);
    }

test_done:
    liteplayer_mngr_reset(g_player);
    liteplayer_mngr_destroy(g_player);
    g_player = NULL;

    OS_THREAD_SLEEP_MSEC(100);
    OS_MEMORY_DUMP();

    OS_LOGI(TAG, "liteplayer test thread leave");
    g_exit = true;
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        OS_LOGW(TAG, "Usage: %s [url]", argv[0]);
        return 0;
    }

    struct os_threadattr attr = {
        .name = "liteplayer_mngr_demo",
        .priority = LITEPLYAER_TEST_TASK_PRIO,
        .stacksize = LITEPLYAER_TEST_TASK_STACK,
        .joinable = true,
    };

    const char *filename = audio_strdup(argv[1]);
    const char *url = filename;
    if (strstr(filename, "http") == NULL) {
        struct stat statbuf;
        if (stat(filename, &statbuf) < 0) {
            OS_LOGE(TAG, "Failed to stat path[%s]", filename);
            goto done;
        }

        if (S_ISDIR(statbuf.st_mode) && generate_playlist(filename) == 0) {
            url = PLAYLIST_FILE;
        }
    }

    os_thread_t tid = OS_THREAD_CREATE(&attr, liteplayer_test_thread, (void *)url);
    if (tid == NULL)
        goto done;

    char input = 0;
    while (!g_exit) {
        if (input != '\n') {
            OS_LOGW(TAG, "Waiting enter command:");
            OS_LOGW(TAG, "  Q|q: quit");
            OS_LOGW(TAG, "  P|p: pause");
            OS_LOGW(TAG, "  R|r: resume");
            OS_LOGW(TAG, "  N|n: switch next");
            OS_LOGW(TAG, "  V|v: switch prev");
            OS_LOGW(TAG, "  O:   enable single looping");
            OS_LOGW(TAG, "  o:   disable single looping");
        }
        input = getc(stdin);

        if (input == 'Q' || input == 'q') {
           OS_LOGI(TAG, "Quit");
            if (g_player)
                liteplayer_mngr_reset(g_player);
            break;
        }
        else if (input == 'P' || input == 'p') {
           OS_LOGI(TAG, "Pause");
            if (g_player)
                liteplayer_mngr_pause(g_player);
        }
        else if (input == 'R' || input == 'r') {
           OS_LOGI(TAG, "Resume");
            if (g_player)
                liteplayer_mngr_resume(g_player);
        }
        else if (input == 'S' || input == 's') {
           OS_LOGI(TAG, "Seek 10S");
            if (g_player) {
                int position = 0;
                liteplayer_mngr_get_position(g_player, &position);
                liteplayer_mngr_seek(g_player, position+10000);
            }
        }
        else if (input == 'N' || input == 'n') {
           OS_LOGI(TAG, "Next");
            if (g_player)
                liteplayer_mngr_next(g_player);
        }
        else if (input == 'V' || input == 'v') {
           OS_LOGI(TAG, "Prev");
            if (g_player)
                liteplayer_mngr_prev(g_player);
        }
        else if (input == 'O') {
           OS_LOGI(TAG, "Enable looping");
            if (g_player)
                liteplayer_mngr_set_single_looping(g_player, true);
        }
        else if (input == 'o') {
           OS_LOGI(TAG, "Disable looping");
            if (g_player)
                liteplayer_mngr_set_single_looping(g_player, false);
        }
        else {
            if (input != '\n')
                OS_LOGW(TAG, "Unknown command: %c", input);
        }
    }

    OS_THREAD_JOIN(tid, NULL);

done:
    audio_free(filename);
    OS_LOGI(TAG, "main thread leave");
    return 0;
}
