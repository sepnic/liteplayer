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
#include <stdlib.h>
#include <string.h>

#include "cutils/log_helper.h"
#include "esp_adf/audio_common.h"
#include "esp_adf/audio_element.h"
#include "audio_resampler/audio_resampler.h"
#include "audio_stream/sink_stream.h"

#define TAG "[liteplayer]SINK_STREAM"

#define SINK_INPUT_TIMEOUT_MAX  50

struct sink_stream {
    struct sink_stream_cfg config;
    sink_handle_t out;
    bool first_write;
};

static esp_err_t sink_stream_open(audio_element_handle_t self)
{
    struct sink_stream *sink = (struct sink_stream *)audio_element_getdata(self);
    struct sink_stream_cfg *config = &sink->config;

    OS_LOGV(TAG, "Open sink stream");

    if (sink->out == NULL) {
        OS_LOGI(TAG, "Open sink stream out: rate:%d, channels:%d, bits:%d",
                config->samplerate, config->channels, config->bits);
        if (config->sink_open)
            sink->out = config->sink_open(config->samplerate, config->channels, config->bits, config->sink_priv);
        if (sink->out == NULL) {
            OS_LOGE(TAG, "Failed to open sink out");
            return AEL_IO_FAIL;
        }
    }

    return ESP_OK;
}

static esp_err_t sink_stream_close(audio_element_handle_t self)
{
    struct sink_stream *sink = (struct sink_stream *)audio_element_getdata(self);
    struct sink_stream_cfg *config = &sink->config;

    OS_LOGV(TAG, "Close sink stream");

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

static int sink_stream_write(audio_element_handle_t self, char *buffer, int len, int timeout_ms, void *context)
{
    struct sink_stream *sink = (struct sink_stream *)audio_element_getdata(self);
    struct sink_stream_cfg *config = &sink->config;
    int bytes_written = 0, bytes_wanted = 0, status = -1;

    if (!sink->first_write) {
        audio_element_info_t info = {0};
        audio_element_getinfo(self, &info);
        // If samplerate not matched with decoder info, reopen sink
        if ((info.samplerate != 0 && config->samplerate != info.samplerate) ||
            (info.channels != 0 && config->channels != info.channels) ||
            (info.bits != 0 && config->bits != info.bits)) {
            OS_LOGW(TAG, "sink rate:ch:bit (%d:%d:%d) not match decoder rate:ch:bit (%d:%d:%d), reopen sink",
                    config->samplerate, config->channels, config->bits, info.samplerate, info.channels, info.bits);
            config->samplerate = info.samplerate;
            config->channels = info.channels;
            config->bits = info.bits;
            sink_stream_close(self);
            sink_stream_open(self);
        }
        // Update element info once
        info.samplerate = config->samplerate;
        info.channels = config->channels;
        info.bits = config->bits;
        audio_element_setinfo(self, &info);
        audio_element_report_info(self);
        sink->first_write = true;
    }

    do {
        bytes_wanted = len - bytes_written;
        if (config->sink_write && sink->out != NULL)
            status = config->sink_write(sink->out, buffer, bytes_wanted);
        if (status < 0) {
            OS_LOGE(TAG, "Failed to write pcm, ret:%d", status);
            bytes_written = ESP_FAIL;
            break;
        } else {
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
        w_size = audio_element_output(self, in_buffer, r_size);
    } else {
        w_size = r_size;
    }
    return w_size;
}

static esp_err_t sink_stream_seek(audio_element_handle_t self, long long offset)
{
    // reset byte_pos when seek
    audio_element_info_t info = {0};
    audio_element_getinfo(self, &info);
    info.byte_pos = 0;
    audio_element_setinfo(self, &info);
    return ESP_OK;
}

static esp_err_t sink_stream_destroy(audio_element_handle_t self)
{
    struct sink_stream *sink = (struct sink_stream *)audio_element_getdata(self);
    struct sink_stream_cfg *config = &sink->config;

    OS_LOGV(TAG, "Destroy sink stream");

    if (config->sink_close && sink->out != NULL) {
        config->sink_close(sink->out);
        sink->out = NULL;
    }

    audio_free(sink);
    return ESP_OK;
}

audio_element_handle_t sink_stream_init(struct sink_stream_cfg *config)
{
    OS_LOGV(TAG, "Init sink stream");

    audio_element_handle_t el;
    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.open = sink_stream_open;
    cfg.close = sink_stream_close;
    cfg.destroy = sink_stream_destroy;
    cfg.process = sink_stream_process;
    cfg.seek = sink_stream_seek;
    cfg.task_stack = config->task_stack;
    cfg.task_prio = config->task_prio;
    cfg.out_rb_size = config->out_rb_size;
    cfg.buffer_len = config->buf_sz;
    cfg.tag = "sink";
    cfg.write = sink_stream_write;

    struct sink_stream *sink = audio_calloc(1, sizeof(struct sink_stream));
    AUDIO_MEM_CHECK(TAG, sink, return NULL);
    memcpy(&sink->config, config, sizeof(struct sink_stream_cfg));

    el = audio_element_init(&cfg);
    if (el == NULL) {
        OS_LOGE(TAG, "Failed to init sink audio element");
        audio_free(sink);
        return NULL;
    }
    audio_element_setdata(el, sink);

    audio_element_set_input_timeout(el, SINK_INPUT_TIMEOUT_MAX);
    return el;
}
