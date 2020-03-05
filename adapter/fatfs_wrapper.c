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
#include <string.h>

#include "esp_adf/esp_log.h"
#include "esp_adf/audio_common.h"
#include "audio_stream/file_stream.h"
#include "fatfs_wrapper.h"

#define TAG "fatfswrapper"

typedef struct fatfs_wrapper_priv {
    const char *url;
    file_mode_t mode;
    FILE *file;
    long content_pos;
    long content_len;
} fatfs_wrapper_priv_t;

file_handle_t fatfs_wrapper_open(const char *url, file_mode_t mode, long long content_pos, void *priv)
{
    fatfs_wrapper_priv_t *handle = audio_calloc(1, sizeof(fatfs_wrapper_priv_t));
    FILE *file = NULL;

    if (handle == NULL)
        return NULL;

    if (mode == FILE_READ)
        file = fopen(url, "rb");
    else
        file = fopen(url, "wb+");

    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open file:%s", url);
        audio_free(file);
        return NULL;
    }

    handle->url = url;
    handle->mode = mode;
    handle->file = file;
    handle->content_pos = (long)content_pos;

    fseek(handle->file, 0, SEEK_END);
    handle->content_len = ftell(handle->file);
    fseek(handle->file, handle->content_pos, SEEK_SET);

    return handle;
}

int fatfs_wrapper_read(file_handle_t handle, char *buffer, int size)
{
    fatfs_wrapper_priv_t *priv = (fatfs_wrapper_priv_t *)handle;
    if (priv->file) {
        if (priv->content_len > 0 && priv->content_pos >= priv->content_len) {
            ESP_LOGD(TAG, "fatfs file read done: %d/%d", (int)priv->content_pos, (int)priv->content_len);
            return 0;
        }
        size_t bytes_read = fread(buffer, 1, size, priv->file);
        if (bytes_read > 0)
            priv->content_pos += bytes_read;
        return bytes_read;
    }
    return -1;
}

int fatfs_wrapper_write(file_handle_t handle, char *buffer, int size)
{
    fatfs_wrapper_priv_t *priv = (fatfs_wrapper_priv_t *)handle;
    if (priv->file) {
        size_t bytes_written = fwrite(buffer, 1, size, priv->file);
        if (bytes_written > 0)
            priv->content_pos += bytes_written;
        return bytes_written;
    }
    return -1;
}

long long fatfs_wrapper_filesize(file_handle_t handle)
{
    fatfs_wrapper_priv_t *priv = (fatfs_wrapper_priv_t *)handle;
    if (priv->file)
        return priv->content_len;
    return 0;
}

int fatfs_wrapper_seek(file_handle_t handle, long offset)
{
    fatfs_wrapper_priv_t *priv = (fatfs_wrapper_priv_t *)handle;
    if (priv->file) {
        int ret = fseek(priv->file, offset, SEEK_SET);
        if (ret == 0)
            priv->content_pos = offset;
        return ret;
    }
    return -1;
}

void fatfs_wrapper_close(file_handle_t handle)
{
    fatfs_wrapper_priv_t *priv = (fatfs_wrapper_priv_t *)handle;
    if (priv->file) {
        if (priv->mode == FILE_WRITE)
            fflush(priv->file);
        fclose(priv->file);
        priv->file = NULL;
    }
    audio_free(priv);
}
