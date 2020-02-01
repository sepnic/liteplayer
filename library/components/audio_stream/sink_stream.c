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

#include "esp_adf/esp_log.h"
#include "esp_adf/audio_common.h"
#include "esp_adf/audio_element.h"
#include "audio_resampler/audio_resampler.h"
#include "audio_stream/sink_stream.h"

#define TAG "SINK_STREAM"

//#define ENABLE_SRC

#if !defined(CONFIG_SRC_QUALITY)
#define CONFIG_SRC_QUALITY      8
#endif

#define SINK_INPUT_TIMEOUT_MAX  50

typedef struct sink_stream {
    sink_stream_cfg_t   config;
    sink_handle_t       out;
#if defined(ENABLE_SRC)
    resample_converter_handle_t resampler;
    bool resampler_inited;
    bool resample_opened;
#endif
} sink_stream_t;

static esp_err_t sink_stream_open(audio_element_handle_t self)
{
    sink_stream_t *sink = (sink_stream_t *)audio_element_getdata(self);
    sink_stream_cfg_t *config = &sink->config;

    ESP_LOGV(TAG, "Open sink stream");

#if defined(ENABLE_SRC)
    if (!sink->resampler_inited) {
        audio_element_info_t info = {0};
        audio_element_getinfo(self, &info);
        ESP_LOGI(TAG, "Input: channels=%d, rate=%d, output:channels=%d, rate=%d",
                 info.in_channels, info.in_samplerate, info.out_channels, info.out_samplerate);
        if ((info.in_samplerate != 0 && info.in_samplerate != info.out_samplerate) ||
            (info.in_channels != 0 && info.in_channels != info.out_channels)) {
            resample_cfg_t cfg;
            cfg.in_channels = info.in_channels;
            cfg.in_rate = info.in_samplerate;
            cfg.out_channels = info.out_channels;
            cfg.out_rate = info.out_samplerate;
            cfg.bits = info.bits;
            cfg.quality = CONFIG_SRC_QUALITY;
            if (sink->resampler && sink->resampler->open(sink->resampler, &cfg) == 0) {
                sink->resample_opened = true;
            }
            else {
                ESP_LOGE(TAG, "Failed to open resampler");
                return AEL_IO_FAIL;
            }
        }
        sink->resampler_inited = true;
    }
#endif

    if (sink->out == NULL) {
        audio_element_info_t info = {0};
        audio_element_getinfo(self, &info);
#if defined(ENABLE_SRC)
        config->out_samplerate = info.out_samplerate;
        config->out_channels = info.out_channels;
#else
        config->out_samplerate = info.in_samplerate;
        config->out_channels = info.in_channels;
        info.out_samplerate = info.in_samplerate;
        info.out_channels = info.in_channels;
        audio_element_setinfo(self, &info);
#endif
        ESP_LOGI(TAG, "Open sink stream out: rate:%d, channels:%d", config->out_samplerate, config->out_channels);
        if (config->sink_open)
            sink->out = config->sink_open(config->out_samplerate, config->out_channels, config->sink_priv);
        if (sink->out == NULL) {
            ESP_LOGE(TAG, "Failed to open sink out");
            return AEL_IO_FAIL;
        }
    }

    return ESP_OK;
}

static int sink_stream_write(audio_element_handle_t self, char *buffer, int len, int timeout_ms, void *context)
{
    sink_stream_t *sink = (sink_stream_t *)audio_element_getdata(self);
    sink_stream_cfg_t *config = &sink->config;
    int bytes_written = 0, bytes_wanted = 0, status = -1;

    do {
        bytes_wanted = len - bytes_written;
        if (config->sink_write && sink->out != NULL)
            status = config->sink_write(sink->out, buffer, bytes_wanted);
        if (status < 0) {
            ESP_LOGE(TAG, "Failed to write pcm, ret:%d", status);
            break;
        }
        else {
            bytes_written += status;
            buffer += status;
        }
    } while (bytes_written < len);

    if (bytes_written > 0) {
        audio_element_info_t info = {0};
        audio_element_getinfo(self, &info);
        info.byte_pos += bytes_written;
        audio_element_setinfo(self, &info);
        audio_element_report_pos(self);
    }
    return bytes_written;
}

static int sink_stream_process(audio_element_handle_t self, char *in_buffer, int in_len)
{
    int r_size = audio_element_input(self, in_buffer, in_len);
    int w_size = 0;
    if (r_size > 0) {
#if defined(ENABLE_SRC)
        sink_stream_t *sink = (sink_stream_t *)audio_element_getdata(self);
        if (sink->resample_opened && sink->resampler) {
            int ret = sink->resampler->process(sink->resampler, (const short *)in_buffer, r_size);
            if (ret == 0) {
                in_buffer = (char *)sink->resampler->out_buf;
                r_size = sink->resampler->out_bytes;
            }
            else {
                ESP_LOGE(TAG, "Failed to process resampler");
                return AEL_IO_FAIL;
            }
        }
#endif
        w_size = audio_element_output(self, in_buffer, r_size);
    } else {
        w_size = r_size;
    }
    return w_size;
}

static esp_err_t sink_stream_close(audio_element_handle_t self)
{
    sink_stream_t *sink = (sink_stream_t *)audio_element_getdata(self);
    sink_stream_cfg_t *config = &sink->config;

    ESP_LOGV(TAG, "Close sink stream");

#if defined(ENABLE_SRC)
    if (sink->resample_opened && sink->resampler) {
        sink->resampler->close(sink->resampler);
        sink->resampler_inited = false;
        sink->resample_opened = false;
    }
#endif

    if (config->sink_close && sink->out != NULL) {
        config->sink_close(sink->out);
        sink->out = NULL;
    }

    if (audio_element_get_state(self) != AEL_STATE_PAUSED) {
        audio_element_info_t info = {0};
        audio_element_getinfo(self, &info);
        info.byte_pos = 0;
        audio_element_setinfo(self, &info);
    }
    return ESP_OK;
}

static esp_err_t sink_stream_destroy(audio_element_handle_t self)
{
    sink_stream_t *sink = (sink_stream_t *)audio_element_getdata(self);
    sink_stream_cfg_t *config = &sink->config;

    ESP_LOGV(TAG, "Destroy sink stream");

#if defined(ENABLE_SRC)
    if (sink->resampler)
        sink->resampler->destroy(sink->resampler);
#endif

    if (config->sink_close && sink->out != NULL) {
        config->sink_close(sink->out);
        sink->out = NULL;
    }

    audio_free(sink);
    return ESP_OK;
}

audio_element_handle_t sink_stream_init(sink_stream_cfg_t *config)
{
    ESP_LOGV(TAG, "Init sink stream");

    audio_element_handle_t el;
    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.open = sink_stream_open;
    cfg.close = sink_stream_close;
    cfg.process = sink_stream_process;
    cfg.destroy = sink_stream_destroy;
    cfg.task_stack = config->task_stack;
    cfg.task_prio = config->task_prio;
    cfg.out_rb_size = config->out_rb_size;
    cfg.buffer_len = config->buf_sz;
    cfg.tag = "sink";

    if (config->type == AUDIO_STREAM_WRITER) {
        cfg.write = sink_stream_write;
    }
    else if (config->type == AUDIO_STREAM_READER) {
        ESP_LOGE(TAG, "Unvalid stream type (AUDIO_STREAM_READER) for sink");
        return NULL;
    }

    sink_stream_t *sink = audio_calloc(1, sizeof(sink_stream_t));
    AUDIO_MEM_CHECK(TAG, sink, return NULL);
    memcpy(&sink->config, config, sizeof(sink_stream_cfg_t));

    el = audio_element_init(&cfg);
    if (el == NULL) {
        ESP_LOGE(TAG, "Failed to init sink audio element");
        audio_free(sink);
        return NULL;
    }
    audio_element_setdata(el, sink);

#if defined(ENABLE_SRC)
    sink->resampler = audio_resampler_init();
    sink->resampler_inited = false;
    sink->resample_opened = false;

    audio_element_info_t info = {0};
    audio_element_getinfo(el, &info);
    info.out_samplerate = config->out_samplerate;
    info.out_channels   = config->out_channels;
    audio_element_setinfo(el, &info);
#endif

    audio_element_set_input_timeout(el, SINK_INPUT_TIMEOUT_MAX);
    return el;
}
