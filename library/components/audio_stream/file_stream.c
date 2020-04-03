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

#include "msgutils/os_logger.h"
#include "esp_adf/audio_common.h"
#include "esp_adf/audio_element.h"
#include "audio_extractor/wav_extractor.h"
#include "audio_resampler/audio_resampler.h"
#include "audio_stream/file_stream.h"

#define TAG "FILE_STREAM"

//#define ENABLE_SRC

#if defined(ENABLE_SRC)
#if !defined(CONFIG_SRC_OUT_CHANNELS)
#define CONFIG_SRC_OUT_CHANNELS 2
#endif
#if !defined(CONFIG_SRC_OUT_RATE)
#define CONFIG_SRC_OUT_RATE     44100
#endif
#if !defined(CONFIG_SRC_QUALITY)
#define CONFIG_SRC_QUALITY      8
#endif
#endif

#define FILE_PCM_SUFFIX_TYPE  "pcm"
#define FILE_WAV_SUFFIX_TYPE  "wav"

#define FILE_INPUT_TIMEOUT_MAX  30

typedef enum {
    STREAM_TYPE_UNKNOW,
    STREAM_TYPE_PCM,
    STREAM_TYPE_WAV,
} wr_stream_type_t;

typedef struct file_stream {
    file_stream_cfg_t config;
    file_handle_t file;
    wr_stream_type_t w_type;
#if defined(ENABLE_SRC)
    resample_converter_handle_t resampler;
    bool resampler_inited;
    bool resample_opened;
#endif
} file_stream_t;

static wr_stream_type_t get_type(const char *str)
{
    char *relt = strrchr(str, '.');
    if (relt != NULL) {
        relt ++;
        OS_LOGV(TAG, "result = %s", relt);
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

typedef struct file_default_priv {
    const char *url;
    file_mode_t mode;
    FILE *file;
    long long content_pos;
} file_default_priv_t;

static file_handle_t file_default_open(const char *url, file_mode_t mode, long long content_pos, void *file_priv)
{
    file_default_priv_t *priv = audio_calloc(1, sizeof(file_default_priv_t));
    FILE *file = NULL;

    if (priv == NULL)
        return NULL;

    if (mode == FILE_READ)
        file = fopen(url, "rb");
    else
        file = fopen(url, "wb+");

    if (file == NULL) {
        OS_LOGE(TAG, "Failed to open file:%s", url);
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

static int file_default_read(file_handle_t handle, char *buffer, int size)
{
    file_default_priv_t *priv = (file_default_priv_t *)handle;
    if (priv->file != NULL)
        return fread(buffer, 1, size, priv->file);
    else
        return -1;
}

static int file_default_write(file_handle_t handle, char *buffer, int size)
{
    file_default_priv_t *priv = (file_default_priv_t *)handle;
    if (priv->file != NULL)
        return fwrite(buffer, 1, size, priv->file);
    else
        return -1;
}

static int file_default_seek(file_handle_t handle, long offset)
{
    file_default_priv_t *priv = (file_default_priv_t *)handle;
    if (priv->file != NULL)
        return fseek(priv->file, offset, SEEK_SET);
    else
        return -1;
}

static void file_default_close(file_handle_t handle)
{
    file_default_priv_t *priv = (file_default_priv_t *)handle;
    if (priv->file != NULL) {
        if (priv->mode == FILE_WRITE)
            fflush(priv->file);
        fclose(priv->file);
        priv->file = NULL;
    }
    audio_free(priv);
}

static esp_err_t file_stream_open(audio_element_handle_t self)
{
    file_stream_t *file = (file_stream_t *)audio_element_getdata(self);
    file_stream_cfg_t *config = &file->config;
    const char *url = audio_element_get_uri(self);

    if (config->url == NULL) {
        config->url = (url != NULL) ? audio_strdup(url) : NULL;
        if (config->url == NULL)
            return ESP_FAIL;
    }

    if (file->file != NULL) {
        OS_LOGD(TAG, "File already opened");
        return ESP_OK;
    }

    OS_LOGD(TAG, "file_stream_open, url:%s", config->url);
    if (config->type == AUDIO_STREAM_READER) {
        audio_element_info_t info;
        audio_element_getinfo(self, &info);
        file->file = config->file_open(config->url, FILE_READ, info.byte_pos, config->file_priv);
        OS_LOGV(TAG, "current_pos:%d, total_bytes:%ld", (int)info.byte_pos, (int)info.total_bytes);
    } else if (config->type == AUDIO_STREAM_WRITER) {
        file->w_type = get_type(config->url);
        file->file = config->file_open(config->url, FILE_WRITE, 0, config->file_priv);
        if (file->file != NULL && STREAM_TYPE_WAV == file->w_type) {
            wav_header_t header;
            memset(&header, 0x0, sizeof(wav_header_t));
            config->file_write(file->file, (char *)&header, sizeof(header));
        }
    }

    if (file->file == NULL) {
        OS_LOGE(TAG, "Failed to open file %s", config->url);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static int file_stream_read(audio_element_handle_t self, char *buffer, int len, int timeout_ms, void *context)
{
    file_stream_t *file = (file_stream_t *)audio_element_getdata(self);
    file_stream_cfg_t *config = &file->config;
    audio_element_info_t info;
    audio_element_getinfo(self, &info);

    //OS_LOGV(TAG, "read len=%d, pos=%d/%d", len, (int)info.byte_pos, (int)info.total_bytes);
    if (info.total_bytes > 0 && info.byte_pos >= info.total_bytes) {
        audio_element_report_status(self, AEL_STATUS_INPUT_DONE);
        return ESP_OK;
    }

    int rlen = config->file_read(file->file, buffer, len);
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

static int file_stream_write(audio_element_handle_t self, char *buffer, int len, int timeout_ms, void *context)
{
    file_stream_t *file = (file_stream_t *)audio_element_getdata(self);
    file_stream_cfg_t *config = &file->config;
    audio_element_info_t info;
    audio_element_getinfo(self, &info);

#if defined(ENABLE_SRC)
    if (!file->resampler_inited) {
        if (info.in_samplerate != CONFIG_SRC_OUT_RATE || info.in_channels != CONFIG_SRC_OUT_CHANNELS) {
            resample_cfg_t cfg;
            cfg.in_channels = info.in_channels;
            cfg.in_rate = info.in_samplerate;
            cfg.out_channels = CONFIG_SRC_OUT_CHANNELS;
            cfg.out_rate = CONFIG_SRC_OUT_RATE;
            cfg.bits = info.bits;
            cfg.quality = CONFIG_SRC_QUALITY;
            if (file->resampler && file->resampler->open(file->resampler, &cfg) == 0)
                file->resample_opened = true;
        }
        file->resampler_inited = true;
    }
    if (file->resample_opened && file->resampler) {
        int ret = file->resampler->process(file->resampler, (const short *)buffer, len);
        if (ret == 0) {
            buffer = (char *)file->resampler->out_buf;
            len = file->resampler->out_bytes;
            info.out_channels = CONFIG_SRC_OUT_CHANNELS;
            info.out_samplerate = CONFIG_SRC_OUT_RATE;
        }
    }
#endif

    int wlen = config->file_write(file->file, buffer, len);
    //OS_LOGV(TAG, "write,%d, pos:%d", wlen, info.byte_pos);
    if (wlen > 0) {
        info.byte_pos += wlen;
        audio_element_setinfo(self, &info);
    }
    return wlen;
}

static int file_stream_process(audio_element_handle_t self, char *in_buffer, int in_len)
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

static esp_err_t file_stream_close(audio_element_handle_t self)
{
    file_stream_t *file = (file_stream_t *)audio_element_getdata(self);
    file_stream_cfg_t *config = &file->config;

    if (file->file == NULL)
        return ESP_FAIL;

    if (AUDIO_STREAM_WRITER == config->type && STREAM_TYPE_WAV == file->w_type) {
        wav_header_t header;
        memset(&header, 0x0, sizeof(wav_header_t));

        config->file_seek(file->file, 0);

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
        config->file_write(file->file, (char *)&header, sizeof(header));
    }

    if (AEL_STATE_PAUSED != audio_element_get_state(self)) {
#if defined(ENABLE_SRC)
        if (file->resample_opened && file->resampler) {
            file->resampler->close(file->resampler);
            file->resampler_inited = false;
            file->resample_opened = false;
        }
#endif

        config->file_close(file->file);
        file->file = NULL;

        audio_element_info_t info = {0};
        audio_element_getinfo(self, &info);
        info.byte_pos = 0;
        audio_element_setinfo(self, &info);
    }
    return ESP_OK;
}

static esp_err_t file_stream_destroy(audio_element_handle_t self)
{
    file_stream_t *file = (file_stream_t *)audio_element_getdata(self);
    file_stream_cfg_t *config = &file->config;

#if defined(ENABLE_SRC)
    if (file->resampler) {
        if (file->resample_opened) {
            file->resampler->close(file->resampler);
            file->resampler_inited = false;
            file->resample_opened = false;
        }
        file->resampler->destroy(file->resampler);
    }
#endif

    if (file->file != NULL) {
        config->file_close(file->file);
        file->file = NULL;
    }

    if (config->url != NULL)
        audio_free(config->url);
    audio_free(file);
    return ESP_OK;
}

audio_element_handle_t file_stream_init(file_stream_cfg_t *config)
{
    audio_element_handle_t el;
    file_stream_t *file = audio_calloc(1, sizeof(file_stream_t));

    AUDIO_MEM_CHECK(TAG, file, return NULL);

    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.open = file_stream_open;
    cfg.close = file_stream_close;
    cfg.process = file_stream_process;
    cfg.destroy = file_stream_destroy;
    cfg.task_stack = config->task_stack;
    cfg.task_prio = config->task_prio;
    cfg.out_rb_size = config->out_rb_size;
    cfg.buffer_len = config->buf_sz;
    if (cfg.buffer_len == 0)
        cfg.buffer_len = FILE_STREAM_BUF_SIZE;
    cfg.tag = "file";

    if (config->type == AUDIO_STREAM_WRITER)
        cfg.write = file_stream_write;
    else
        cfg.read = file_stream_read;

    if (config->file_open == NULL)
        config->file_open = file_default_open;
    if (config->file_read == NULL)
        config->file_read = file_default_read;
    if (config->file_write == NULL)
        config->file_write = file_default_write;
    if (config->file_seek == NULL)
        config->file_seek = file_default_seek;
    if (config->file_close == NULL)
        config->file_close = file_default_close;

    memcpy(&file->config, config, sizeof(file_stream_cfg_t));

    if (config->url != NULL)
        file->config.url = audio_strdup(config->url);

    el = audio_element_init(&cfg);
    AUDIO_MEM_CHECK(TAG, el, goto file_init_error);

#if defined(ENABLE_SRC)
    if (config->type == AUDIO_STREAM_WRITER) {
        file->resampler = audio_resampler_init();
        file->resampler_inited = false;
        file->resample_opened = false;

        audio_element_info_t info = {0};
        audio_element_getinfo(el, &info);
        info.out_samplerate = CONFIG_SRC_OUT_RATE;
        info.out_channels = CONFIG_SRC_OUT_CHANNELS;
        audio_element_setinfo(el, &info);
    }
#endif

    if (config->type == AUDIO_STREAM_WRITER)
        audio_element_set_input_timeout(el, FILE_INPUT_TIMEOUT_MAX);

    audio_element_setdata(el, file);
    return el;

file_init_error:
    audio_free(file);
    return NULL;
}
