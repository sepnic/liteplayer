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
#include <string.h>

#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "source_file_wrapper.h"

#define TAG "[liteplayer]file"

struct file_priv {
    const char *url;
    FILE *file;
    long content_pos;
    long content_len;
};

const char *file_wrapper_url_protocol()
{
    return "file";
}

source_handle_t file_wrapper_open(const char *url, long long content_pos, void *priv_data)
{
    struct file_priv *priv = OS_CALLOC(1, sizeof(struct file_priv));
    FILE *file = NULL;

    if (priv == NULL)
        return NULL;

    OS_LOGD(TAG, "Opening file:%s, content_pos:%d", url, (int)content_pos);

    file = fopen(url, "rb");
    if (file == NULL) {
        OS_LOGE(TAG, "Failed to open file:%s", url);
        OS_FREE(priv);
        return NULL;
    }

    priv->url = url;
    priv->file = file;
    priv->content_pos = (long)content_pos;

    fseek(priv->file, 0, SEEK_END);
    priv->content_len = ftell(priv->file);
    fseek(priv->file, priv->content_pos, SEEK_SET);

    return priv;
}

int file_wrapper_read(source_handle_t handle, char *buffer, int size)
{
    struct file_priv *priv = (struct file_priv *)handle;
    if (priv->file) {
        if (priv->content_len > 0 && priv->content_pos >= priv->content_len) {
            OS_LOGD(TAG, "file read done: %d/%d", (int)priv->content_pos, (int)priv->content_len);
            return 0;
        }
        size_t bytes_read = fread(buffer, 1, size, priv->file);
        if (bytes_read > 0)
            priv->content_pos += bytes_read;
        return bytes_read;
    }
    return -1;
}

long long file_wrapper_content_pos(source_handle_t handle)
{
    struct file_priv *priv = (struct file_priv *)handle;
    if (priv->file)
        return priv->content_pos;
    return 0;
}

long long file_wrapper_content_len(source_handle_t handle)
{
    struct file_priv *priv = (struct file_priv *)handle;
    if (priv->file)
        return priv->content_len;
    return 0;
}

int file_wrapper_seek(source_handle_t handle, long offset)
{
    struct file_priv *priv = (struct file_priv *)handle;
    if (priv->file) {
        OS_LOGD(TAG, "Seeking file:%p, offset:%ld", priv->file, offset);
        int ret = fseek(priv->file, offset, SEEK_SET);
        if (ret == 0)
            priv->content_pos = offset;
        return ret;
    }
    return -1;
}

void file_wrapper_close(source_handle_t handle)
{
    struct file_priv *priv = (struct file_priv *)handle;
    if (priv->file) {
        OS_LOGD(TAG, "Closing file:%p", priv->file);
        fclose(priv->file);
        priv->file = NULL;
    }
    OS_FREE(priv);
}
