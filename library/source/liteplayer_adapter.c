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

#include "osal/os_thread.h"
#include "cutils/list.h"
#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "esp_adf/audio_common.h"
#include "liteplayer_config.h"
#include "liteplayer_adapter_internal.h"
#include "liteplayer_adapter.h"

#define TAG "[liteplayer]adapter"

#define DEFAULT_SOURCE_RINGBUF_SIZE (1024*32)
#define DEFAULT_SOURCE_URL_PROTOCOL "file"

struct liteplayer_adapter_priv {
    struct liteplayer_adapter adapter;
    struct listnode source_list;
    struct listnode sink_list;
    os_mutex lock;
};

struct source_wrapper_node {
    struct source_wrapper wrapper;
    struct listnode listnode;
};

struct sink_wrapper_node {
    struct sink_wrapper wrapper;
    struct listnode listnode;
};

struct file_wrapper_priv {
    const char *url;
    FILE *file;
    long content_pos;
    long content_len;
};

static const char *file_wrapper_protocol()
{
    return DEFAULT_SOURCE_URL_PROTOCOL;
}

static source_handle_t file_wrapper_open(const char *url, long long content_pos, void *priv_data)
{
    struct file_wrapper_priv *priv = audio_calloc(1, sizeof(struct file_wrapper_priv));
    FILE *file = NULL;
    if (priv == NULL)
        return NULL;

    OS_LOGD(TAG, "Opening file:%s, content_pos:%d", url, (int)content_pos);
    file = fopen(url, "rb");
    if (file == NULL) {
        OS_LOGE(TAG, "Failed to open file:%s", url);
        audio_free(priv);
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

static int file_wrapper_read(source_handle_t handle, char *buffer, int size)
{
    struct file_wrapper_priv *priv = (struct file_wrapper_priv *)handle;
    if (priv->file) {
        if (priv->content_len > 0 && priv->content_pos >= priv->content_len) {
            OS_LOGD(TAG, "File read done: %d/%d", (int)priv->content_pos, (int)priv->content_len);
            return 0;
        }
        size_t bytes_read = fread(buffer, 1, size, priv->file);
        if (bytes_read > 0)
            priv->content_pos += bytes_read;
        return bytes_read;
    }
    return -1;
}

static long long file_wrapper_filesize(source_handle_t handle)
{
    struct file_wrapper_priv *priv = (struct file_wrapper_priv *)handle;
    if (priv->file)
        return priv->content_len;
    return 0;
}

static int file_wrapper_seek(source_handle_t handle, long offset)
{
    struct file_wrapper_priv *priv = (struct file_wrapper_priv *)handle;
    if (priv->file) {
        int ret = fseek(priv->file, offset, SEEK_SET);
        if (ret == 0)
            priv->content_pos = offset;
        return ret;
    }
    return -1;
}

static void file_wrapper_close(source_handle_t handle)
{
    struct file_wrapper_priv *priv = (struct file_wrapper_priv *)handle;
    if (priv->file) {
        fclose(priv->file);
        priv->file = NULL;
    }
    audio_free(priv);
}

static int add_source_wrapper(liteplayer_adapter_handle_t self, struct source_wrapper *wrapper)
{
    if (wrapper == NULL || wrapper->procotol() == NULL)
        return ESP_FAIL;

    struct liteplayer_adapter_priv *priv = (struct liteplayer_adapter_priv *)self;
    struct source_wrapper_node *node = NULL;
    struct listnode *item;
    bool found = false;

    os_mutex_lock(priv->lock);

    list_for_each(item, &priv->source_list) {
        node = listnode_to_item(item, struct source_wrapper_node, listnode);
        if (strcasecmp(wrapper->procotol(), node->wrapper.procotol()) == 0) {
            found = true;
            break;
        }
    }

    if (!found) {
        node = audio_calloc(1, sizeof(struct source_wrapper_node));
        if (node == NULL) {
            os_mutex_unlock(priv->lock);
            return ESP_FAIL;
        }
        list_add_head(&priv->source_list, &node->listnode);
    }

    if (wrapper->ringbuf_size > 0)
        node->wrapper.ringbuf_size = wrapper->ringbuf_size;
    else
        node->wrapper.ringbuf_size = DEFAULT_SOURCE_RINGBUF_SIZE;
    node->wrapper.async_mode = wrapper->async_mode;
    node->wrapper.priv_data = wrapper->priv_data;
    node->wrapper.procotol = wrapper->procotol;
    node->wrapper.open = wrapper->open;
    node->wrapper.read = wrapper->read;
    node->wrapper.filesize = wrapper->filesize;
    node->wrapper.seek = wrapper->seek;
    node->wrapper.close = wrapper->close;

    os_mutex_unlock(priv->lock);
    return ESP_OK;
}

static struct source_wrapper *find_source_wrapper(liteplayer_adapter_handle_t self, const char *url)
{
    if (url == NULL)
        return NULL;

    struct liteplayer_adapter_priv *priv = (struct liteplayer_adapter_priv *)self;
    struct source_wrapper_node *node = NULL;
    struct listnode *item;

    os_mutex_lock(priv->lock);

    list_for_each(item, &priv->source_list) {
        node = listnode_to_item(item, struct source_wrapper_node, listnode);
        if (strncasecmp(url, node->wrapper.procotol(), strlen(node->wrapper.procotol())) == 0) {
            goto find_out;
        }
    }
    // if found no source wrapper, now we treat it as file url
    list_for_each(item, &priv->source_list) {
        node = listnode_to_item(item, struct source_wrapper_node, listnode);
        if (strcasecmp(DEFAULT_SOURCE_URL_PROTOCOL, node->wrapper.procotol()) == 0) {
            goto find_out;
        }
    }
    node = NULL; // no file wrapper is found

find_out:
    os_mutex_unlock(priv->lock);
    return node != NULL ? &node->wrapper : NULL;
}

int add_sink_wrapper(liteplayer_adapter_handle_t self, struct sink_wrapper *wrapper)
{
    if (wrapper == NULL || wrapper->name() == NULL)
        return ESP_FAIL;

    struct liteplayer_adapter_priv *priv = (struct liteplayer_adapter_priv *)self;
    struct sink_wrapper_node *node = NULL;
    struct listnode *item;
    bool found = false;

    os_mutex_lock(priv->lock);

    list_for_each(item, &priv->sink_list) {
        node = listnode_to_item(item, struct sink_wrapper_node, listnode);
        if (strcasecmp(wrapper->name(), node->wrapper.name()) == 0) {
            found = true;
            break;
        }
    }

    if (!found) {
        node = audio_calloc(1, sizeof(struct sink_wrapper_node));
        if (node == NULL) {
            os_mutex_unlock(priv->lock);
            return ESP_FAIL;
        }
        list_add_head(&priv->sink_list, &node->listnode);
    }

    node->wrapper.priv_data = wrapper->priv_data;
    node->wrapper.name = wrapper->name;
    node->wrapper.open = wrapper->open;
    node->wrapper.write = wrapper->write;
    node->wrapper.close = wrapper->close;

    os_mutex_unlock(priv->lock);
    return ESP_OK;
}

static struct sink_wrapper *find_sink_wrapper(liteplayer_adapter_handle_t self, const char *name)
{
    if (name == NULL)
        return NULL;

    struct liteplayer_adapter_priv *priv = (struct liteplayer_adapter_priv *)self;
    struct sink_wrapper_node *node = NULL;
    struct listnode *item;

    os_mutex_lock(priv->lock);

    list_for_each(item, &priv->sink_list) {
        node = listnode_to_item(item, struct sink_wrapper_node, listnode);
        if (strncasecmp(name, node->wrapper.name(), strlen(node->wrapper.name())) == 0) {
            goto find_out;
        }
    }
    // if found no sink wrapper, now we use the first one in the list
    if (!list_empty(&priv->sink_list))
        node = listnode_to_item(list_head(&priv->sink_list), struct sink_wrapper_node, listnode);
    else
        node = NULL;

find_out:
    os_mutex_unlock(priv->lock);
    return node != NULL ? &node->wrapper : NULL;
}

static void liteplayer_adapter_destory(liteplayer_adapter_handle_t self)
{
    struct liteplayer_adapter_priv *priv = (struct liteplayer_adapter_priv *)self;
    struct source_wrapper_node *source = NULL;
    struct sink_wrapper_node *sink = NULL;
    struct listnode *item, *tmp;

    list_for_each_safe(item, tmp, &priv->source_list) {
        source = listnode_to_item(item, struct source_wrapper_node, listnode);
        list_remove(item);
        audio_free(source);
    }

    list_for_each_safe(item, tmp, &priv->sink_list) {
        sink = listnode_to_item(item, struct sink_wrapper_node, listnode);
        list_remove(item);
        audio_free(sink);
    }

    os_mutex_destroy(priv->lock);
    audio_free(priv);
}

liteplayer_adapter_handle_t liteplayer_adapter_init()
{
    struct liteplayer_adapter_priv *priv = audio_calloc(1, sizeof(struct liteplayer_adapter_priv));
    if (priv == NULL)
        return NULL;
    
    priv->lock = os_mutex_create();
    if (priv->lock == NULL) {
        audio_free(priv);
        return NULL;
    }

    list_init(&priv->source_list);
    list_init(&priv->sink_list);

    priv->adapter.add_source_wrapper = add_source_wrapper;
    priv->adapter.find_source_wrapper = find_source_wrapper;
    priv->adapter.add_sink_wrapper = add_sink_wrapper;
    priv->adapter.find_sink_wrapper = find_sink_wrapper;
    priv->adapter.destory = liteplayer_adapter_destory;

    struct source_wrapper file_wrapper = {
        .async_mode = false,
        .ringbuf_size = DEFAULT_SOURCE_RINGBUF_SIZE,
        .priv_data = NULL,
        .procotol = file_wrapper_protocol,
        .open = file_wrapper_open,
        .read = file_wrapper_read,
        .filesize = file_wrapper_filesize,
        .seek = file_wrapper_seek,
        .close = file_wrapper_close,
    };
    add_source_wrapper((liteplayer_adapter_handle_t)priv, &file_wrapper);

    return (liteplayer_adapter_handle_t)priv;
}
