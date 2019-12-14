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
#include "audio_stream/alsa_stream.h"

#define TAG "ALSA_STREAM"

//#define ENABLE_SRC

#if !defined(DEFAULT_SRC_QUALITY)
#define DEFAULT_SRC_QUALITY      8
#endif

#define ALSA_INPUT_TIMEOUT_MAX  30

typedef struct alsa_stream {
    alsa_stream_cfg_t   config;
    alsa_handle_t       out;
#if defined(ENABLE_SRC)
    resample_converter_handle_t resampler;
    bool resampler_inited;
    bool resample_opened;
#endif
} alsa_stream_t;

static esp_err_t alsa_stream_open(audio_element_handle_t self)
{
    alsa_stream_t *alsa = (alsa_stream_t *)audio_element_getdata(self);
    alsa_stream_cfg_t *config = &alsa->config;

    ESP_LOGV(TAG, "Open alsa stream %s", config->type == AUDIO_STREAM_WRITER ? "out" : "in");

#if defined(ENABLE_SRC)
    if (!alsa->resampler_inited) {
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
            cfg.quality = DEFAULT_SRC_QUALITY;
            if (alsa->resampler && alsa->resampler->open(alsa->resampler, &cfg) == 0) {
                alsa->resample_opened = true;
            }
            else {
                ESP_LOGE(TAG, "Failed to open resampler");
                return AEL_IO_FAIL;
            }
        }
        alsa->resampler_inited = true;
    }
#endif

    if (config->type == AUDIO_STREAM_WRITER && alsa->out == NULL) {
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
        ESP_LOGI(TAG, "Open alsa stream out: rate:%d, channels:%d", config->out_samplerate, config->out_channels);
        if (config->alsa_open)
            alsa->out = config->alsa_open(config->out_samplerate, config->out_channels, config->alsa_priv);
        if (alsa->out == NULL) {
            ESP_LOGE(TAG, "Failed to open alsa out");
            return AEL_IO_FAIL;
        }
    }

    return ESP_OK;
}

static int alsa_stream_read(audio_element_handle_t self, char *buffer, int len, int timeout_ms, void *context)
{
    // TODO
    return ESP_FAIL;
}

static int alsa_stream_write(audio_element_handle_t self, char *buffer, int len, int timeout_ms, void *context)
{
    alsa_stream_t *alsa = (alsa_stream_t *)audio_element_getdata(self);
    alsa_stream_cfg_t *config = &alsa->config;
    int bytes_written = 0, bytes_wanted = 0, status = -1;

    do {
        bytes_wanted = len - bytes_written;
        if (config->alsa_write && alsa->out != NULL)
            status = config->alsa_write(alsa->out, buffer, bytes_wanted);
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

static int alsa_stream_process(audio_element_handle_t self, char *in_buffer, int in_len)
{
    int r_size = audio_element_input(self, in_buffer, in_len);
    int w_size = 0;
    if (r_size > 0) {
#if defined(ENABLE_SRC)
        alsa_stream_t *alsa = (alsa_stream_t *)audio_element_getdata(self);
        if (alsa->resample_opened && alsa->resampler) {
            int ret = alsa->resampler->process(alsa->resampler, (const short *)in_buffer, r_size);
            if (ret == 0) {
                in_buffer = (char *)alsa->resampler->out_buf;
                r_size = alsa->resampler->out_bytes;
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

static esp_err_t alsa_stream_close(audio_element_handle_t self)
{
    alsa_stream_t *alsa = (alsa_stream_t *)audio_element_getdata(self);
    alsa_stream_cfg_t *config = &alsa->config;

    ESP_LOGV(TAG, "Close alsa stream %s", config->type == AUDIO_STREAM_WRITER ? "out" : "in");

#if defined(ENABLE_SRC)
    if (alsa->resample_opened && alsa->resampler) {
        alsa->resampler->close(alsa->resampler);
        alsa->resampler_inited = false;
        alsa->resample_opened = false;
    }
#endif

    if (config->type == AUDIO_STREAM_WRITER) {
        if (config->alsa_close && alsa->out != NULL) {
            config->alsa_close(alsa->out);
            alsa->out = NULL;
        }
    }

    if (audio_element_get_state(self) != AEL_STATE_PAUSED) {
        audio_element_info_t info = {0};
        audio_element_getinfo(self, &info);
        info.byte_pos = 0;
        audio_element_setinfo(self, &info);
    }
    return ESP_OK;
}

static esp_err_t alsa_stream_destroy(audio_element_handle_t self)
{
    alsa_stream_t *alsa = (alsa_stream_t *)audio_element_getdata(self);
    alsa_stream_cfg_t *config = &alsa->config;

    ESP_LOGV(TAG, "Destroy alsa stream %s", config->type == AUDIO_STREAM_WRITER ? "out" : "in");

#if defined(ENABLE_SRC)
    if (alsa->resampler)
        alsa->resampler->destroy(alsa->resampler);
#endif

    if (config->type == AUDIO_STREAM_WRITER) {
        if (config->alsa_close && alsa->out != NULL) {
            config->alsa_close(alsa->out);
            alsa->out = NULL;
        }
    }

    audio_free(alsa);
    return ESP_OK;
}

audio_element_handle_t alsa_stream_init(alsa_stream_cfg_t *config)
{
    ESP_LOGV(TAG, "Init alsa stream %s", config->type == AUDIO_STREAM_WRITER ? "out" : "in");

    audio_element_handle_t el;
    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.open = alsa_stream_open;
    cfg.close = alsa_stream_close;
    cfg.process = alsa_stream_process;
    cfg.destroy = alsa_stream_destroy;
    cfg.task_stack = config->task_stack;
    cfg.task_prio = config->task_prio;
    cfg.out_rb_size = config->out_rb_size;
    cfg.buffer_len = config->buf_sz;
    cfg.tag = "alsa";

    if (config->type == AUDIO_STREAM_WRITER) {
        cfg.write = alsa_stream_write;
    }
    else if (config->type == AUDIO_STREAM_READER) {
        cfg.read = alsa_stream_read;
        // TODO
        return NULL;
    }

    alsa_stream_t *alsa = audio_calloc(1, sizeof(alsa_stream_t));
    AUDIO_MEM_CHECK(TAG, alsa, return NULL);
    memcpy(&alsa->config, config, sizeof(alsa_stream_cfg_t));

    el = audio_element_init(&cfg);
    if (el == NULL) {
        ESP_LOGE(TAG, "Failed to init alsa audio element");
        audio_free(alsa);
        return NULL;
    }
    audio_element_setdata(el, alsa);

#if defined(ENABLE_SRC)
    alsa->resampler = audio_resampler_init();
    alsa->resampler_inited = false;
    alsa->resample_opened = false;

    audio_element_info_t info = {0};
    audio_element_getinfo(el, &info);
    info.out_samplerate = config->out_samplerate;
    info.out_channels   = config->out_channels;
    audio_element_setinfo(el, &info);
#endif

    audio_element_set_input_timeout(el, ALSA_INPUT_TIMEOUT_MAX);
    return el;
}
