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
#include <stdlib.h>
#include <string.h>

#include "msgutils/os_logger.h"
#include "esp_adf/audio_common.h"
#include "esp_adf/audio_element.h"
#include "audio_resampler/audio_resampler.h"
#include "audio_stream/http_stream.h"

#define TAG "HTTP_STREAM"

typedef struct http_stream {
    http_stream_cfg_t   config;
    http_handle_t       client;
} http_stream_t;

static esp_err_t http_stream_open(audio_element_handle_t self)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(self);
    http_stream_cfg_t *config = &http->config;
    const char *url = audio_element_get_uri(self);

    if (config->url == NULL) {
        config->url = (url != NULL) ? audio_strdup(url) : NULL;
        if (config->url == NULL)
            return ESP_FAIL;
    }
    return ESP_OK;
}

static int http_stream_read(audio_element_handle_t self, char *buffer, int len, int timeout_ms, void *context)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(self);
    http_stream_cfg_t *config = &http->config;
    int rlen = 0;
    audio_element_info_t info;
    audio_element_getinfo(self, &info);

    if (http->client == NULL) {
        if (config->http_open)
            http->client = config->http_open(config->url, info.byte_pos, config->http_priv);
        if (http->client == NULL) {
            OS_LOGE(TAG, "Failed to connect httpclient");
            return AEL_IO_FAIL;
        }
        OS_LOGD(TAG, "httpclient connected: url:%s, current_pos:%d, total_bytes:%d",
                 config->url, (int)info.byte_pos, (int)info.total_bytes);
    }

    //OS_LOGV(TAG, "read len=%d, pos=%d/%d", len, (int)info.byte_pos, (int)info.total_bytes);
    if (info.total_bytes > 0 && info.byte_pos >= info.total_bytes) {
        audio_element_report_status(self, AEL_STATUS_INPUT_DONE);
        return ESP_OK;
    }

    if (config->http_read) {
        rlen = config->http_read(http->client, buffer, len);
        if (rlen < 0)
            return AEL_IO_FAIL;
    }
    else {
        return AEL_IO_FAIL;
    }

    if (rlen > 0) {
        info.byte_pos += rlen;
        audio_element_setinfo(self, &info);
    }
    else if (info.total_bytes == 0 && rlen == 0) {
        audio_element_report_status(self, AEL_STATUS_INPUT_DONE);
        return ESP_OK;
    }

    return rlen;
}

static int http_stream_process(audio_element_handle_t self, char *in_buffer, int in_len)
{
    int w_size = 0;
    int r_size = audio_element_input(self, in_buffer, in_len); 
    if (r_size > 0) {
        w_size = audio_element_output(self, in_buffer, r_size);
    } else if (r_size == AEL_IO_TIMEOUT) {
        OS_LOGW(TAG, "HTTP timeout, mark as IO_DONE");
        w_size = AEL_IO_DONE;
    } else {
        w_size = r_size;
    }
    return w_size;
}

static esp_err_t http_stream_close(audio_element_handle_t self)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(self);
    http_stream_cfg_t *config = &http->config;

    OS_LOGV(TAG, "Close http stream");

    if (audio_element_get_state(self) != AEL_STATE_PAUSED) {
        if (config->http_close && http->client != NULL) {
            config->http_close(http->client);
            http->client = NULL;
        }

        audio_element_info_t info = {0};
        audio_element_getinfo(self, &info);
        info.byte_pos = 0;
        audio_element_setinfo(self, &info);
    }
    return ESP_OK;
}

static esp_err_t http_stream_destroy(audio_element_handle_t self)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(self);
    http_stream_cfg_t *config = &http->config;

    OS_LOGV(TAG, "Destroy http stream");

    if (config->http_close && http->client != NULL) {
        config->http_close(http->client);
        http->client = NULL;
    }

    if (config->url != NULL)
        audio_free(config->url);

    audio_free(http);
    return ESP_OK;
}

audio_element_handle_t http_stream_init(http_stream_cfg_t *config)
{
    OS_LOGV(TAG, "Init http stream");

    audio_element_handle_t el;
    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.open = http_stream_open;
    cfg.close = http_stream_close;
    cfg.process = http_stream_process;
    cfg.destroy = http_stream_destroy;
    cfg.task_stack = config->task_stack;
    cfg.task_prio = config->task_prio;
    cfg.out_rb_size = config->out_rb_size;
    cfg.buffer_len = config->buf_sz;
    cfg.tag = "http";
    cfg.read = http_stream_read;

    http_stream_t *http = audio_calloc(1, sizeof(http_stream_t));
    AUDIO_MEM_CHECK(TAG, http, return NULL);
    memcpy(&http->config, config, sizeof(http_stream_cfg_t));

    if (config->url != NULL)
        http->config.url = audio_strdup(config->url);

    el = audio_element_init(&cfg);
    if (el == NULL) {
        OS_LOGE(TAG, "Failed to init http audio element");
        audio_free(http);
        return NULL;
    }

    audio_element_setdata(el, http);
    return el;
}
