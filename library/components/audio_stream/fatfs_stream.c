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
#include "esp_adf/audio_element.h"
#include "audio_extractor/wav_extractor.h"
#include "audio_resampler/audio_resampler.h"
#include "audio_stream/fatfs_stream.h"

#define TAG "FATFS_STREAM"

//#define ENABLE_SRC

#if defined(ENABLE_SRC)
#if !defined(DEFAULT_SRC_OUT_CHANNELS)
#define DEFAULT_SRC_OUT_CHANNELS 2
#endif
#if !defined(DEFAULT_SRC_OUT_RATE)
#define DEFAULT_SRC_OUT_RATE     44100
#endif
#if !defined(DEFAULT_SRC_QUALITY)
#define DEFAULT_SRC_QUALITY      8
#endif
#endif

#define FILE_PCM_SUFFIX_TYPE  "pcm"
#define FILE_WAV_SUFFIX_TYPE  "wav"

#define FATFS_INPUT_TIMEOUT_MAX  30

typedef enum {
    STREAM_TYPE_UNKNOW,
    STREAM_TYPE_PCM,
    STREAM_TYPE_WAV,
} wr_stream_type_t;

typedef struct fatfs_stream {
    fatfs_stream_cfg_t config;
    fatfs_handle_t file;
    wr_stream_type_t w_type;
#if defined(ENABLE_SRC)
    resample_converter_handle_t resampler;
    bool resampler_inited;
    bool resample_opened;
#endif
} fatfs_stream_t;

static wr_stream_type_t get_type(const char *str)
{
    char *relt = strrchr(str, '.');
    if (relt != NULL) {
        relt ++;
        ESP_LOGV(TAG, "result = %s", relt);
        if (strncasecmp(relt, FILE_PCM_SUFFIX_TYPE, 3) == 0)
            return STREAM_TYPE_PCM;
        else if (strncasecmp(relt, FILE_WAV_SUFFIX_TYPE, 3) == 0)
            return STREAM_TYPE_WAV;
        else
            return STREAM_TYPE_UNKNOW;
    } else {
        return STREAM_TYPE_UNKNOW;
    }
}

typedef struct fatfs_default_priv {
    const char *url;
    fatfs_mode_t mode;
    FILE *file;
    long long content_pos;
} fatfs_default_priv_t;

static fatfs_handle_t fatfs_default_open(const char *url, fatfs_mode_t mode, long long content_pos, void *fatfs_priv)
{
    fatfs_default_priv_t *priv = audio_calloc(1, sizeof(fatfs_default_priv_t));
    FILE *file = NULL;

    if (priv == NULL)
        return NULL;

    if (mode == FATFS_READ)
        file = fopen(url, "rb");
    else
        file = fopen(url, "wb+");

    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open file:%s", url);
        audio_free(priv);
        return NULL;
    }

    if (content_pos > 0)
        fseek(file, (long)content_pos, SEEK_SET);

    priv->url = url;
    priv->mode = mode;
    priv->file = file;
    priv->content_pos = content_pos;
    return priv;
}

static int fatfs_default_read(fatfs_handle_t handle, char *buffer, int size)
{
    fatfs_default_priv_t *priv = (fatfs_default_priv_t *)handle;
    if (priv->file != NULL)
        return fread(buffer, 1, size, priv->file);
    else
        return -1;
}

static int fatfs_default_write(fatfs_handle_t handle, char *buffer, int size)
{
    fatfs_default_priv_t *priv = (fatfs_default_priv_t *)handle;
    if (priv->file != NULL)
        return fwrite(buffer, 1, size, priv->file);
    else
        return -1;
}

static int fatfs_default_seek(fatfs_handle_t handle, long offset)
{
    fatfs_default_priv_t *priv = (fatfs_default_priv_t *)handle;
    if (priv->file != NULL)
        return fseek(priv->file, offset, SEEK_SET);
    else
        return -1;
}

static void fatfs_default_close(fatfs_handle_t handle)
{
    fatfs_default_priv_t *priv = (fatfs_default_priv_t *)handle;
    if (priv->file != NULL) {
        if (priv->mode == FATFS_WRITE)
            fflush(priv->file);
        fclose(priv->file);
        priv->file = NULL;
    }
    audio_free(priv);
}

static esp_err_t fatfs_stream_open(audio_element_handle_t self)
{
    fatfs_stream_t *fatfs = (fatfs_stream_t *)audio_element_getdata(self);
    fatfs_stream_cfg_t *config = &fatfs->config;
    const char *url = audio_element_get_uri(self);

    if (config->url == NULL) {
        config->url = (url != NULL) ? audio_strdup(url) : NULL;
        if (config->url == NULL)
            return ESP_FAIL;
    }

    if (fatfs->file != NULL) {
        ESP_LOGD(TAG, "File already opened");
        return ESP_OK;
    }

    ESP_LOGD(TAG, "fatfs_stream_open, url:%s", config->url);
    if (config->type == AUDIO_STREAM_READER) {
        audio_element_info_t info;
        audio_element_getinfo(self, &info);
        fatfs->file = config->fatfs_open(config->url, FATFS_READ, info.byte_pos, config->fatfs_priv);
        ESP_LOGV(TAG, "current_pos:%d, total_bytes:%ld", (int)info.byte_pos, (int)info.total_bytes);
    } else if (config->type == AUDIO_STREAM_WRITER) {
        fatfs->w_type = get_type(config->url);
        fatfs->file = config->fatfs_open(config->url, FATFS_WRITE, 0, config->fatfs_priv);
        if (fatfs->file != NULL && STREAM_TYPE_WAV == fatfs->w_type) {
            wav_header_t header;
            memset(&header, 0x0, sizeof(wav_header_t));
            config->fatfs_write(fatfs->file, (char *)&header, sizeof(header));
        }
    }

    if (fatfs->file == NULL) {
        ESP_LOGE(TAG, "Failed to open file %s", config->url);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static int fatfs_stream_read(audio_element_handle_t self, char *buffer, int len, int timeout_ms, void *context)
{
    fatfs_stream_t *fatfs = (fatfs_stream_t *)audio_element_getdata(self);
    fatfs_stream_cfg_t *config = &fatfs->config;
    audio_element_info_t info;
    audio_element_getinfo(self, &info);

    //ESP_LOGV(TAG, "read len=%d, pos=%d/%d", len, (int)info.byte_pos, (int)info.total_bytes);
    if (info.total_bytes > 0 && info.byte_pos >= info.total_bytes) {
        audio_element_report_status(self, AEL_STATUS_INPUT_DONE);
        return ESP_OK;
    }

    int rlen = config->fatfs_read(fatfs->file, buffer, len);
    if (rlen < 0) {
        return AEL_IO_DONE; // exit task if failed to read
    }
    else if (rlen == 0) {
        audio_element_report_status(self, AEL_STATUS_INPUT_DONE);
        return ESP_OK;
    }
    else {
        info.byte_pos += rlen;
        audio_element_setinfo(self, &info);
    }
    return rlen;
}

static int fatfs_stream_write(audio_element_handle_t self, char *buffer, int len, int timeout_ms, void *context)
{
    fatfs_stream_t *fatfs = (fatfs_stream_t *)audio_element_getdata(self);
    fatfs_stream_cfg_t *config = &fatfs->config;
    audio_element_info_t info;
    audio_element_getinfo(self, &info);

#if defined(ENABLE_SRC)
    if (!fatfs->resampler_inited) {
        if (info.in_samplerate != DEFAULT_SRC_OUT_RATE || info.in_channels != DEFAULT_SRC_OUT_CHANNELS) {
            resample_cfg_t cfg;
            cfg.in_channels = info.in_channels;
            cfg.in_rate = info.in_samplerate;
            cfg.out_channels = DEFAULT_SRC_OUT_CHANNELS;
            cfg.out_rate = DEFAULT_SRC_OUT_RATE;
            cfg.bits = info.bits;
            cfg.quality = DEFAULT_SRC_QUALITY;
            if (fatfs->resampler && fatfs->resampler->open(fatfs->resampler, &cfg) == 0)
                fatfs->resample_opened = true;
        }
        fatfs->resampler_inited = true;
    }
    if (fatfs->resample_opened && fatfs->resampler) {
        int ret = fatfs->resampler->process(fatfs->resampler, (const short *)buffer, len);
        if (ret == 0) {
            buffer = (char *)fatfs->resampler->out_buf;
            len = fatfs->resampler->out_bytes;
            info.out_channels = DEFAULT_SRC_OUT_CHANNELS;
            info.out_samplerate = DEFAULT_SRC_OUT_RATE;
        }
    }
#endif

    int wlen = config->fatfs_write(fatfs->file, buffer, len);
    //ESP_LOGV(TAG, "write,%d, pos:%d", wlen, info.byte_pos);
    if (wlen > 0) {
        info.byte_pos += wlen;
        audio_element_setinfo(self, &info);
    }
    return wlen;
}

static int fatfs_stream_process(audio_element_handle_t self, char *in_buffer, int in_len)
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

static esp_err_t fatfs_stream_close(audio_element_handle_t self)
{
    fatfs_stream_t *fatfs = (fatfs_stream_t *)audio_element_getdata(self);
    fatfs_stream_cfg_t *config = &fatfs->config;

    if (fatfs->file == NULL)
        return ESP_FAIL;

    if (AUDIO_STREAM_WRITER == config->type && STREAM_TYPE_WAV == fatfs->w_type) {
        wav_header_t header;
        memset(&header, 0x0, sizeof(wav_header_t));

        config->fatfs_seek(fatfs->file, 0);

        audio_element_info_t info;
        audio_element_getinfo(self, &info);
#if defined(ENABLE_SRC)
        wav_build_header(&header,
                          info.out_samplerate,
                          info.bits,
                          info.out_channels,
                          (int)info.byte_pos);
#else
        wav_build_header(&header,
                          info.in_samplerate,
                          info.bits,
                          info.in_channels,
                          (int)info.byte_pos);
#endif
        config->fatfs_write(fatfs->file, (char *)&header, sizeof(header));
    }

    if (AEL_STATE_PAUSED != audio_element_get_state(self)) {
#if defined(ENABLE_SRC)
        if (fatfs->resample_opened && fatfs->resampler) {
            fatfs->resampler->close(fatfs->resampler);
            fatfs->resampler_inited = false;
            fatfs->resample_opened = false;
        }
#endif

        config->fatfs_close(fatfs->file);
        fatfs->file = NULL;

        audio_element_report_info(self);
        audio_element_info_t info = {0};
        audio_element_getinfo(self, &info);
        info.byte_pos = 0;
        audio_element_setinfo(self, &info);
    }
    return ESP_OK;
}

static esp_err_t fatfs_stream_destroy(audio_element_handle_t self)
{
    fatfs_stream_t *fatfs = (fatfs_stream_t *)audio_element_getdata(self);
    fatfs_stream_cfg_t *config = &fatfs->config;

#if defined(ENABLE_SRC)
    if (fatfs->resampler) {
        if (fatfs->resample_opened) {
            fatfs->resampler->close(fatfs->resampler);
            fatfs->resampler_inited = false;
            fatfs->resample_opened = false;
        }
        fatfs->resampler->destroy(fatfs->resampler);
    }
#endif

    if (fatfs->file != NULL) {
        config->fatfs_close(fatfs->file);
        fatfs->file = NULL;
    }

    if (config->url != NULL)
        audio_free(config->url);
    audio_free(fatfs);
    return ESP_OK;
}

audio_element_handle_t fatfs_stream_init(fatfs_stream_cfg_t *config)
{
    audio_element_handle_t el;
    fatfs_stream_t *fatfs = audio_calloc(1, sizeof(fatfs_stream_t));

    AUDIO_MEM_CHECK(TAG, fatfs, return NULL);

    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.open = fatfs_stream_open;
    cfg.close = fatfs_stream_close;
    cfg.process = fatfs_stream_process;
    cfg.destroy = fatfs_stream_destroy;
    cfg.task_stack = config->task_stack;
    cfg.task_prio = config->task_prio;
    cfg.out_rb_size = config->out_rb_size;
    cfg.buffer_len = config->buf_sz;
    if (cfg.buffer_len == 0)
        cfg.buffer_len = FATFS_STREAM_BUF_SIZE;
    cfg.tag = "fatfs";

    if (config->type == AUDIO_STREAM_WRITER)
        cfg.write = fatfs_stream_write;
    else
        cfg.read = fatfs_stream_read;

    if (config->fatfs_open == NULL)
        config->fatfs_open = fatfs_default_open;
    if (config->fatfs_read == NULL)
        config->fatfs_read = fatfs_default_read;
    if (config->fatfs_write == NULL)
        config->fatfs_write = fatfs_default_write;
    if (config->fatfs_seek == NULL)
        config->fatfs_seek = fatfs_default_seek;
    if (config->fatfs_close == NULL)
        config->fatfs_close = fatfs_default_close;

    memcpy(&fatfs->config, config, sizeof(fatfs_stream_cfg_t));

    if (config->url != NULL)
        fatfs->config.url = audio_strdup(config->url);

    el = audio_element_init(&cfg);
    AUDIO_MEM_CHECK(TAG, el, goto fatfs_init_error);

#if defined(ENABLE_SRC)
    if (config->type == AUDIO_STREAM_WRITER) {
        fatfs->resampler = audio_resampler_init();
        fatfs->resampler_inited = false;
        fatfs->resample_opened = false;

        audio_element_info_t info = {0};
        audio_element_getinfo(el, &info);
        info.out_samplerate = DEFAULT_SRC_OUT_RATE;
        info.out_channels = DEFAULT_SRC_OUT_CHANNELS;
        audio_element_setinfo(el, &info);
    }
#endif

    if (config->type == AUDIO_STREAM_WRITER)
        audio_element_set_input_timeout(el, FATFS_INPUT_TIMEOUT_MAX);

    audio_element_setdata(el, fatfs);
    return el;

fatfs_init_error:
    audio_free(fatfs);
    return NULL;
}
