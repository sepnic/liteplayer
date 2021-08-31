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
#include <stdint.h>

#include "cutils/ringbuf.h"
#include "cutils/log_helper.h"
#include "esp_adf/audio_element.h"
#include "esp_adf/audio_pipeline.h"
#include "esp_adf/audio_event_iface.h"
#include "esp_adf/audio_common.h"
#include "audio_decoder/mp3_decoder.h"
#include "audio_decoder/aac_decoder.h"
#include "audio_decoder/m4a_decoder.h"
#include "audio_decoder/wav_decoder.h"

#include "liteplayer_adapter.h"
#include "liteplayer_config.h"
#include "liteplayer_source.h"
#include "liteplayer_parser.h"
#include "liteplayer_main.h"

#define TAG "[liteplayer]CORE"

struct liteplayer {
    const char             *url; // STREAM: /websocket/tts.mp3
                                 // HTTP  : http://..., https://...
                                 // FILE  : others
    enum liteplayer_state   state;
    os_mutex                state_lock;
    liteplayer_state_cb     state_listener;
    void                   *state_userdata;
    bool                    state_error;

    os_mutex                io_lock;

    struct file_wrapper     file_ops;
    struct http_wrapper     http_ops;
    struct sink_wrapper     sink_ops;

    audio_pipeline_handle_t audio_pipeline;
    audio_element_handle_t  ael_decoder;
    ringbuf_handle          source_ringbuf;
    sink_handle_t           sink_handle;

    enum media_source_type  source_type;
    media_source_handle_t   media_source_handle;
    media_parser_handle_t   media_parser_handle;
    struct media_codec_info codec_info;
    int                     threshold_ms;

    int                     sink_samplerate;
    int                     sink_channels;
    int                     sink_bits;
    long long               sink_position;
    bool                    sink_inited;

    int                     seek_time;
    long long               seek_offset;
};

static const char *media_source_tag(enum media_source_type source)
{
    switch (source) {
    case MEDIA_SOURCE_HTTP:
        return "HTTP";
    case MEDIA_SOURCE_FILE:
        return "FILE";
    case MEDIA_SOURCE_STREAM:
        return "STREAM";
    default:
        break;
    }
    return "Unknown";
}

static int audio_sink_open(audio_element_handle_t self, void *ctx)
{
    liteplayer_handle_t handle = (liteplayer_handle_t)ctx;
    if (!handle->sink_inited) {
        OS_LOGV(TAG, "Sink out not inited, abort opening");
        return 0;
    }
    OS_LOGI(TAG, "Opening sink out: rate:%d, channels:%d, bits:%d",
            handle->sink_samplerate, handle->sink_channels, handle->sink_bits);
    if (handle->sink_handle == NULL) {
        handle->sink_handle = handle->sink_ops.open(handle->sink_samplerate,
                                                    handle->sink_channels,
                                                    handle->sink_bits,
                                                    handle->sink_ops.sink_priv);
        if (handle->sink_handle == NULL) {
            OS_LOGE(TAG, "Failed to open sink out");
            return -1;
        }
    }
    return 0;
}
static void audio_sink_close(audio_element_handle_t self, void *ctx)
{
    liteplayer_handle_t handle = (liteplayer_handle_t)ctx;
    if (handle->sink_handle != NULL) {
        OS_LOGI(TAG, "Closing sink out");
        handle->sink_ops.close(handle->sink_handle);
        handle->sink_handle = NULL;
    }
    if (audio_element_get_state(self) != AEL_STATE_PAUSED) {
        handle->sink_position = 0;
    }
}

static int audio_sink_write(audio_element_handle_t self, char *buffer, int len, int timeout_ms, void *ctx)
{
    liteplayer_handle_t handle = (liteplayer_handle_t)ctx;
    if (!handle->sink_inited) {
        handle->sink_inited = true;
        if (audio_sink_open(self, ctx) != 0)
            return -1;
    }
    
    int bytes_written = 0, bytes_wanted = 0, ret = -1;
    do {
        bytes_wanted = len - bytes_written;
        ret = handle->sink_ops.write(handle->sink_handle, buffer, bytes_wanted);
        if (ret < 0) {
            OS_LOGE(TAG, "Failed to write pcm, ret:%d", ret);
            bytes_written = -1;
            break;
        } else {
            bytes_written += ret;
            buffer += ret;
        }
    } while (bytes_written < len);

    if (bytes_written > 0) {
        handle->sink_position += bytes_written;
    }
    return bytes_written;
}

static void media_player_state_callback(liteplayer_handle_t handle, enum liteplayer_state state, int errcode)
{
    if (state == LITEPLAYER_ERROR) {
        if (!handle->state_error) {
            handle->state_error = true;
            if (handle->state_listener)
                handle->state_listener(LITEPLAYER_ERROR, errcode, handle->state_userdata);
        }
    } else {
        if (!handle->state_error || state == LITEPLAYER_IDLE || state == LITEPLAYER_STOPPED) {
            if (handle->state_listener)
                handle->state_listener(state, 0, handle->state_userdata);
        }
    }
}

static int audio_element_state_callback(audio_element_handle_t el, audio_event_iface_msg_t *msg, void *ctx)
{
    liteplayer_handle_t handle = (liteplayer_handle_t)ctx;

    os_mutex_lock(handle->state_lock);

    if (msg->source_type == AUDIO_ELEMENT_TYPE_ELEMENT) {
        audio_element_status_t el_status = (audio_element_status_t)msg->data;

        if (msg->cmd == AEL_MSG_CMD_REPORT_STATUS) {
            switch (el_status) {
            case AEL_STATUS_ERROR_INPUT:
            case AEL_STATUS_ERROR_PROCESS:
            case AEL_STATUS_ERROR_OUTPUT:
            case AEL_STATUS_ERROR_UNKNOWN:
                OS_LOGE(TAG, "[ %s-%s ] Receive error[%d]",
                        media_source_tag(handle->source_type), audio_element_get_tag(el), el_status);
                handle->state = LITEPLAYER_ERROR;
                media_player_state_callback(handle, LITEPLAYER_ERROR, el_status);
                break;

            case AEL_STATUS_ERROR_TIMEOUT:
                if (msg->source == (void *)handle->ael_decoder) {
                    OS_LOGW(TAG, "[ %s-%s ] Receive inputtimeout event, filled/total: %d/%d",
                            media_source_tag(handle->source_type),
                            audio_element_get_tag(el),
                            rb_bytes_filled(handle->source_ringbuf),
                            rb_get_size(handle->source_ringbuf));
                }
                break;

            case AEL_STATUS_STATE_RUNNING:
                if (msg->source == (void *)handle->ael_decoder) {
                    OS_LOGD(TAG, "[ %s-%s ] Receive started event",
                            media_source_tag(handle->source_type), audio_element_get_tag(el));
                    //handle->state = LITEPLAYER_STARTED;
                    //media_player_state_callback(handle, LITEPLAYER_STARTED, 0);
                }
                break;

            case AEL_STATUS_STATE_PAUSED:
                if (msg->source == (void *)handle->ael_decoder) {
                    OS_LOGD(TAG, "[ %s-%s ] Receive paused event",
                            media_source_tag(handle->source_type), audio_element_get_tag(el));
                    //handle->state = LITEPLAYER_PAUSED;
                    //media_player_state_callback(handle, LITEPLAYER_PAUSED, 0);
                }
                break;

            case AEL_STATUS_STATE_FINISHED:
                if (msg->source == (void *)handle->ael_decoder) {
                    OS_LOGD(TAG, "[ %s-%s ] Receive finished event",
                            media_source_tag(handle->source_type), audio_element_get_tag(el));
                    if (handle->state != LITEPLAYER_ERROR && handle->state != LITEPLAYER_STOPPED) {
                        handle->state = LITEPLAYER_COMPLETED;
                        media_player_state_callback(handle, LITEPLAYER_COMPLETED, 0);
                    }
                }
                break;

            case AEL_STATUS_STATE_STOPPED:
                if (msg->source == (void *)handle->ael_decoder) {
                    OS_LOGD(TAG, "[ %s-%s ] Receive stopped event",
                            media_source_tag(handle->source_type), audio_element_get_tag(el));
                    //handle->state = LITEPLAYER_STOPPED;
                    //media_player_state_callback(handle, LITEPLAYER_STOPPED, 0);
                }
                break;

            default:
                break;
            }
        } else if (msg->cmd == AEL_MSG_CMD_REPORT_INFO) {
            if (msg->source == (void *)handle->ael_decoder) {
                audio_element_info_t info = {0};
                audio_element_getinfo(handle->ael_decoder, &info);
                OS_LOGI(TAG, "[ %s-%s ] Receive codec info: samplerate=%d, ch=%d, bits=%d",
                        media_source_tag(handle->source_type), audio_element_get_tag(el),
                        info.samplerate, info.channels, info.bits);
                handle->sink_samplerate = info.samplerate;
                handle->sink_channels = info.channels;
                handle->sink_bits = info.bits;
            }
        }
    }

    os_mutex_unlock(handle->state_lock);
    return ESP_OK;
}

static void media_source_state_callback(enum media_source_state state, void *priv)
{
    liteplayer_handle_t handle = (liteplayer_handle_t)priv;

    os_mutex_lock(handle->state_lock);

    switch (state) {
    case MEDIA_SOURCE_READ_FAILED:
    case MEDIA_SOURCE_WRITE_FAILED:
        OS_LOGE(TAG, "[ %s-SOURCE ] Receive error[%d]", media_source_tag(handle->source_type), state);
        handle->state = LITEPLAYER_ERROR;
        media_player_state_callback(handle, LITEPLAYER_ERROR, state);
        break;
    case MEDIA_SOURCE_READ_DONE:
        OS_LOGD(TAG, "[ %s-SOURCE ] Receive inputdone event", media_source_tag(handle->source_type));
        if (handle->source_type == MEDIA_SOURCE_HTTP)
            media_player_state_callback(handle, LITEPLAYER_NEARLYCOMPLETED, 0);
        break;
    case MEDIA_SOURCE_REACH_THRESHOLD:
        OS_LOGI(TAG, "[ %s-SOURCE ] Receive threshold event: threshold/total=%d/%d", media_source_tag(handle->source_type),
                rb_get_threshold(handle->source_ringbuf), rb_get_size(handle->source_ringbuf));
        if (handle->source_type == MEDIA_SOURCE_HTTP)
            media_player_state_callback(handle, LITEPLAYER_CACHECOMPLETED, 0);
        break;
    default:
        break;
    }

    os_mutex_unlock(handle->state_lock);
}

static void media_parser_state_callback(enum media_parser_state state, struct media_codec_info *codec_info, void *priv)
{
    liteplayer_handle_t handle = (liteplayer_handle_t)priv;

    os_mutex_lock(handle->state_lock);

    switch (state) {
    case MEDIA_PARSER_FAILED:
        OS_LOGE(TAG, "[ %s-PARSER ] Receive error[%d]", media_source_tag(handle->source_type), state);
        handle->state = LITEPLAYER_ERROR;
        media_player_state_callback(handle, LITEPLAYER_ERROR, MEDIA_PARSER_FAILED);
        break;
    case MEDIA_PARSER_SUCCEED:
        OS_LOGD(TAG, "[ %s-PARSER ] Receive prepared event", media_source_tag(handle->source_type));
        memcpy(&handle->codec_info, codec_info, sizeof(struct media_codec_info));
        handle->state = LITEPLAYER_PREPARED;
        media_player_state_callback(handle, LITEPLAYER_PREPARED, 0);
        break;
    default:
        break;
    }

    os_mutex_unlock(handle->state_lock);
}

static int media_source_get_threshold(liteplayer_handle_t handle, int threshold_ms)
{
    int threshold_size = 0;
    if (threshold_ms <= 0)
        return threshold_size;

    if (threshold_ms > handle->codec_info.duration_ms)
        threshold_ms = handle->codec_info.duration_ms;

    switch (handle->codec_info.codec_type) {
    case AUDIO_CODEC_WAV:
    case AUDIO_CODEC_MP3: {
        threshold_size = (handle->codec_info.bytes_per_sec*(threshold_ms/1000));
        break;
    }
    case AUDIO_CODEC_M4A: {
        unsigned int sample_index = 0;
        unsigned int sample_offset = 0;
        if (m4a_get_seek_offset(threshold_ms, &(handle->codec_info.detail.m4a_info), &sample_index, &sample_offset) == 0)
            threshold_size = (int)sample_offset;
        break;
    }
    default:
        break;
    }

    return threshold_size;
}

static void main_pipeline_deinit(liteplayer_handle_t handle)
{
    if (handle->audio_pipeline != NULL) {
        OS_LOGD(TAG, "Destroy audio pipeline");
        audio_pipeline_deinit(handle->audio_pipeline);
        handle->audio_pipeline = NULL;
        handle->ael_decoder = NULL;
    }

    if (handle->media_parser_handle != NULL) {
        media_parser_stop(handle->media_parser_handle);
        handle->media_parser_handle = NULL;
    }

    if (handle->media_source_handle != NULL) {
        media_source_stop(handle->media_source_handle);
        handle->media_source_handle = NULL;
    }

    if (handle->source_ringbuf != NULL) {
        rb_destroy(handle->source_ringbuf);
        handle->source_ringbuf = NULL;
    }
}

static int main_pipeline_init(liteplayer_handle_t handle)
{
    {
        OS_LOGD(TAG, "[1.0] Create audio pipeline");
        audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
        handle->audio_pipeline = audio_pipeline_init(&pipeline_cfg);
        AUDIO_MEM_CHECK(TAG, handle->audio_pipeline, goto pipeline_fail);
    }

    {
        OS_LOGD(TAG, "[2.0] Create decoder element");
        switch (handle->codec_info.codec_type) {
        case AUDIO_CODEC_MP3: {
            struct mp3_decoder_cfg mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
            mp3_cfg.task_prio            = DEFAULT_DECODER_TASK_PRIO;
            mp3_cfg.task_stack           = DEFAULT_DECODER_TASK_STACKSIZE;
            mp3_cfg.out_rb_size          = DEFAULT_DECODER_RINGBUF_SIZE;
            mp3_cfg.mp3_info             = &(handle->codec_info.detail.mp3_info);
            handle->ael_decoder = mp3_decoder_init(&mp3_cfg);
            break;
        }
        case AUDIO_CODEC_AAC: {
            struct aac_decoder_cfg aac_cfg = DEFAULT_AAC_DECODER_CONFIG();
            aac_cfg.task_prio            = DEFAULT_DECODER_TASK_PRIO;
            aac_cfg.task_stack           = DEFAULT_DECODER_TASK_STACKSIZE;
            aac_cfg.out_rb_size          = DEFAULT_DECODER_RINGBUF_SIZE;
            aac_cfg.aac_info             = &(handle->codec_info.detail.aac_info);
            handle->ael_decoder = aac_decoder_init(&aac_cfg);
            break;
        }
        case AUDIO_CODEC_M4A: {
            struct m4a_decoder_cfg m4a_cfg = DEFAULT_M4A_DECODER_CONFIG();
            m4a_cfg.task_prio            = DEFAULT_DECODER_TASK_PRIO;
            m4a_cfg.task_stack           = DEFAULT_DECODER_TASK_STACKSIZE;
            m4a_cfg.out_rb_size          = DEFAULT_DECODER_RINGBUF_SIZE;
            m4a_cfg.m4a_info             = &(handle->codec_info.detail.m4a_info);
            handle->ael_decoder = m4a_decoder_init(&m4a_cfg);
            break;
        }
        case AUDIO_CODEC_WAV: {
            struct wav_decoder_cfg wav_cfg = DEFAULT_WAV_DECODER_CONFIG();
            wav_cfg.task_prio            = DEFAULT_DECODER_TASK_PRIO;
            wav_cfg.task_stack           = DEFAULT_DECODER_TASK_STACKSIZE;
            wav_cfg.out_rb_size          = DEFAULT_DECODER_RINGBUF_SIZE;
            wav_cfg.wav_info             = &(handle->codec_info.detail.wav_info);
            handle->ael_decoder = wav_decoder_init(&wav_cfg);
            break;
        }
        case AUDIO_CODEC_OPUS: {
            //struct opus_decoder_cfg opus_cfg = DEFAULT_OPUS_DECODER_CONFIG();
            //opus_cfg.task_prio            = DEFAULT_DECODER_TASK_PRIO;
            //opus_cfg.task_stack           = DEFAULT_DECODER_TASK_STACKSIZE;
            //opus_cfg.out_rb_size          = DEFAULT_DECODER_RINGBUF_SIZE;
            //opus_cfg.opus_info            = &(handle->codec_info.detail.opus_info);
            //handle->ael_decoder = opus_decoder_init(&opus_cfg);
            break;
        }
        case AUDIO_CODEC_FLAC: {
            //struct flac_decoder_cfg flac_cfg = DEFAULT_FLAC_DECODER_CONFIG();
            //flac_cfg.task_prio            = DEFAULT_DECODER_TASK_PRIO;
            //flac_cfg.task_stack           = DEFAULT_DECODER_TASK_STACKSIZE;
            //flac_cfg.out_rb_size          = DEFAULT_DECODER_RINGBUF_SIZE;
            //flac_cfg.flac_info            = &(handle->codec_info.detail.flac_info);
            //handle->ael_decoder = flac_decoder_init(&flac_cfg);
            break;
        }
        default:
            break;
        }
        AUDIO_MEM_CHECK(TAG, handle->ael_decoder, goto pipeline_fail);
    }

    {
        OS_LOGD(TAG, "[2.1] Create sink element");
        handle->sink_position = 0;
        handle->sink_samplerate = handle->codec_info.codec_samplerate;
        handle->sink_channels = handle->codec_info.codec_channels;
        handle->sink_bits = handle->codec_info.codec_bits;
        stream_callback_t audio_sink = {
            .open = audio_sink_open,
            .close = audio_sink_close,
            .fill = audio_sink_write,
            .ctx = handle,
        };
        audio_element_set_write_cb(handle->ael_decoder, &audio_sink);
    }

    {
        OS_LOGD(TAG, "[2.2] Create source element");
        if (handle->source_type == MEDIA_SOURCE_STREAM)
            handle->source_ringbuf = rb_create(DEFAULT_MEDIA_SOURCE_STREAM_RINGBUF_SIZE);
        else if (handle->source_type == MEDIA_SOURCE_HTTP)
            handle->source_ringbuf = rb_create(DEFAULT_MEDIA_SOURCE_HTTP_RINGBUF_SIZE);
        else if (handle->source_type == MEDIA_SOURCE_FILE)
            handle->source_ringbuf = rb_create(DEFAULT_MEDIA_SOURCE_FILE_RINGBUF_SIZE);
        AUDIO_MEM_CHECK(TAG, handle->source_ringbuf, goto pipeline_fail);

        audio_element_set_input_ringbuf(handle->ael_decoder, handle->source_ringbuf);

        if (handle->source_type == MEDIA_SOURCE_HTTP) {
            struct media_source_info info = {
                .url = handle->url,
                .source_type = MEDIA_SOURCE_HTTP,
                .http_ops = {
                    .http_priv = handle->http_ops.http_priv,
                    .open = handle->http_ops.open,
                    .read = handle->http_ops.read,
                    .seek = handle->http_ops.seek,
                    .close = handle->http_ops.close,
                },
                .content_pos = handle->codec_info.content_pos + handle->seek_offset,
                .threshold_size = media_source_get_threshold(handle, handle->threshold_ms),
            };
            handle->media_source_handle = media_source_start(&info, handle->source_ringbuf, media_source_state_callback, handle);
            AUDIO_MEM_CHECK(TAG, handle->media_source_handle, goto pipeline_fail);
        } else if (handle->source_type == MEDIA_SOURCE_FILE) {
            struct media_source_info info = {
                .url = handle->url,
                .source_type = MEDIA_SOURCE_FILE,
                .file_ops = {
                    .file_priv = handle->file_ops.file_priv,
                    .open = handle->file_ops.open,
                    .read = handle->file_ops.read,
                    .seek = handle->file_ops.seek,
                    .close = handle->file_ops.close,
                },
                .content_pos = handle->codec_info.content_pos + handle->seek_offset,
                .threshold_size = media_source_get_threshold(handle, handle->threshold_ms),
            };
            handle->media_source_handle = media_source_start(&info, handle->source_ringbuf, media_source_state_callback, handle);
            AUDIO_MEM_CHECK(TAG, handle->media_source_handle, goto pipeline_fail);
        }
    }

    {
        OS_LOGD(TAG, "[3.0] Register all elements to audio pipeline");
        audio_pipeline_register(handle->audio_pipeline, handle->ael_decoder, "decoder");

        OS_LOGD(TAG, "[3.1] Link elements together source_ringbuf->decoder->sink_w");
        audio_pipeline_link(handle->audio_pipeline, (const char *[]){"decoder"}, 1);

        OS_LOGD(TAG, "[3.2] Register event callback of all elements");
        audio_element_set_event_callback(handle->ael_decoder, audio_element_state_callback, handle);
    }

    {
        OS_LOGD(TAG, "[4.0] Run audio pipeline");
        if (audio_pipeline_run(handle->audio_pipeline) != 0)
            goto pipeline_fail;
    }

    return ESP_OK;

pipeline_fail:
    main_pipeline_deinit(handle);
    return ESP_FAIL;
}

liteplayer_handle_t liteplayer_create()
{
    liteplayer_handle_t handle = (liteplayer_handle_t)audio_calloc(1, sizeof(struct liteplayer));
    if (handle != NULL) {
        handle->state = LITEPLAYER_IDLE;
        handle->io_lock = os_mutex_create();
        handle->state_lock = os_mutex_create();
        if (handle->io_lock == NULL || handle->state_lock == NULL) {
            audio_free(handle);
            return NULL;
        }
    }
    return handle;
}

int liteplayer_register_file_wrapper(liteplayer_handle_t handle, struct file_wrapper *file_ops)
{
    if (handle == NULL || file_ops == NULL)
        return ESP_FAIL;
    memcpy(&handle->file_ops, file_ops, sizeof(struct file_wrapper));
    return ESP_OK;
}

int liteplayer_register_http_wrapper(liteplayer_handle_t handle, struct http_wrapper *http_ops)
{
    if (handle == NULL || http_ops == NULL)
        return ESP_FAIL;
    memcpy(&handle->http_ops, http_ops, sizeof(struct http_wrapper));
    return ESP_OK;
}

int liteplayer_register_sink_wrapper(liteplayer_handle_t handle, struct sink_wrapper *sink_ops)
{
    if (handle == NULL || sink_ops == NULL)
        return ESP_FAIL;
    memcpy(&handle->sink_ops, sink_ops, sizeof(struct sink_wrapper));
    return ESP_OK;
}

int liteplayer_register_state_listener(liteplayer_handle_t handle, liteplayer_state_cb listener, void *listener_priv)
{
    if (handle == NULL || listener == NULL)
        return ESP_FAIL;
    handle->state_listener = listener;
    handle->state_userdata = listener_priv;
    return ESP_OK;
}

int liteplayer_set_data_source(liteplayer_handle_t handle, const char *url, int threshold_ms)
{
    if (handle == NULL)
        return ESP_FAIL;

#define DEFAULT_TTS_FIXED_URL "tts_16KHZ_mono.mp3"
    if (url == NULL)
        url = DEFAULT_TTS_FIXED_URL;

    OS_LOGI(TAG, "Set player[%s] source:%s", media_source_tag(handle->source_type), url);

    os_mutex_lock(handle->io_lock);

    if (handle->state != LITEPLAYER_IDLE) {
        OS_LOGE(TAG, "Can't set source in state=[%d]", handle->state);
        os_mutex_unlock(handle->io_lock);
        return ESP_FAIL;
    }

    handle->url = audio_strdup(url);
    handle->threshold_ms = threshold_ms;
    handle->state_error = false;

    if (strncmp(url, DEFAULT_TTS_FIXED_URL, strlen(DEFAULT_TTS_FIXED_URL)) == 0)
        handle->source_type = MEDIA_SOURCE_STREAM;
    else if (strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0)
        handle->source_type = MEDIA_SOURCE_HTTP;
    else
        handle->source_type = MEDIA_SOURCE_FILE;

    {
        os_mutex_lock(handle->state_lock);
        handle->state = LITEPLAYER_INITED;
        media_player_state_callback(handle, LITEPLAYER_INITED, 0);
        os_mutex_unlock(handle->state_lock);
    }

    os_mutex_unlock(handle->io_lock);
    return ESP_OK;
}

int liteplayer_prepare(liteplayer_handle_t handle)
{
    if (handle == NULL)
        return ESP_FAIL;

    OS_LOGI(TAG, "Preparing player[%s]", media_source_tag(handle->source_type));

    os_mutex_lock(handle->io_lock);

    if (handle->state != LITEPLAYER_INITED) {
        OS_LOGE(TAG, "Can't prepare in state=[%d]", handle->state);
        os_mutex_unlock(handle->io_lock);
        return ESP_FAIL;
    }

    int ret = ESP_OK;

    if (handle->source_type == MEDIA_SOURCE_STREAM) {
        // TODO: Fixed mp3/16KHz/mono
        handle->codec_info.codec_type = DEFAULT_TTS_FIXED_CODEC;
        handle->codec_info.codec_samplerate = DEFAULT_TTS_FIXED_SAMPLERATE;
        handle->codec_info.codec_channels = DEFAULT_TTS_FIXED_CHANNELS;
        handle->codec_info.codec_bits = 16;
    } else if (handle->source_type == MEDIA_SOURCE_HTTP) {
        struct media_source_info source_info = {0};
        source_info.url = handle->url;
        source_info.source_type = MEDIA_SOURCE_HTTP;
        memcpy(&source_info.http_ops, &handle->http_ops, sizeof(struct http_wrapper));
        ret = media_info_parse(&source_info, &handle->codec_info);
    } else {
        struct media_source_info source_info = {0};
        source_info.url = handle->url;
        source_info.source_type = MEDIA_SOURCE_FILE;
        memcpy(&source_info.file_ops, &handle->file_ops, sizeof(struct file_wrapper));
        ret = media_info_parse(&source_info, &handle->codec_info);
    }

    if (ret == ESP_OK)
        ret = main_pipeline_init(handle);

    {
        os_mutex_lock(handle->state_lock);
        handle->state = (ret == ESP_OK) ? LITEPLAYER_PREPARED : LITEPLAYER_ERROR;
        media_player_state_callback(handle, handle->state, ret);
        os_mutex_unlock(handle->state_lock);
    }

    os_mutex_unlock(handle->io_lock);
    return ret;
}

int liteplayer_prepare_async(liteplayer_handle_t handle)
{
    if (handle == NULL)
        return ESP_FAIL;

    OS_LOGI(TAG, "Async preparing player[%s]", media_source_tag(handle->source_type));

    os_mutex_lock(handle->io_lock);

    if (handle->state != LITEPLAYER_INITED) {
        OS_LOGE(TAG, "Can't prepare in state=[%d]", handle->state);
        os_mutex_unlock(handle->io_lock);
        return ESP_FAIL;
    }

    int ret = ESP_OK;

    if (handle->source_type == MEDIA_SOURCE_STREAM) {
        // TODO: Fixed mp3/16KHz/mono
        handle->codec_info.codec_type = DEFAULT_TTS_FIXED_CODEC;
        handle->codec_info.codec_samplerate = DEFAULT_TTS_FIXED_SAMPLERATE;
        handle->codec_info.codec_channels = DEFAULT_TTS_FIXED_CHANNELS;
        handle->codec_info.codec_bits = 16;
    } else if (handle->source_type == MEDIA_SOURCE_HTTP) {
        struct media_source_info source_info = {0};
        source_info.url = handle->url;
        source_info.source_type = MEDIA_SOURCE_HTTP;
        memcpy(&source_info.http_ops, &handle->http_ops, sizeof(struct http_wrapper));
        handle->media_parser_handle = media_parser_start_async(&source_info, media_parser_state_callback, handle);
        if (handle->media_parser_handle == NULL) {
            ret = ESP_FAIL;
            os_mutex_lock(handle->state_lock);
            handle->state = LITEPLAYER_ERROR;
            media_player_state_callback(handle, LITEPLAYER_ERROR, ret);
            os_mutex_unlock(handle->state_lock);
        }
    } else {
        struct media_source_info source_info = {0};
        source_info.url = handle->url;
        source_info.source_type = MEDIA_SOURCE_FILE;
        memcpy(&source_info.file_ops, &handle->file_ops, sizeof(struct file_wrapper));
        ret = media_info_parse(&source_info, &handle->codec_info);
    }

    if (handle->source_type != MEDIA_SOURCE_HTTP) {
        if (ret == ESP_OK)
            ret = main_pipeline_init(handle);

        os_mutex_lock(handle->state_lock);
        handle->state = (ret == ESP_OK) ? LITEPLAYER_PREPARED : LITEPLAYER_ERROR;
        media_player_state_callback(handle, handle->state, ret);
        os_mutex_unlock(handle->state_lock);
    }

    os_mutex_unlock(handle->io_lock);
    return ret;
}

int liteplayer_write(liteplayer_handle_t handle, char *data, int size, bool final)
{
    if (handle == NULL)
        return ESP_FAIL;

    OS_LOGV(TAG, "Writing player[%s], size=%d, final=%d", media_source_tag(handle->source_type), size, final);

    ringbuf_handle source_ringbuf = audio_element_get_input_ringbuf(handle->ael_decoder);
    if (source_ringbuf == NULL || handle->source_type != MEDIA_SOURCE_STREAM) {
        OS_LOGE(TAG, "Invalid source_ringbuf or source_type");
        return ESP_FAIL;
    }

    os_mutex_lock(handle->io_lock);

    if (handle->state < LITEPLAYER_PREPARED || handle->state > LITEPLAYER_NEARLYCOMPLETED) {
        os_mutex_unlock(handle->io_lock);
        OS_LOGE(TAG, "Can't write data in state=[%d]", handle->state);
        return ESP_FAIL;
    }

    int ret = ESP_OK;
    int byte_total = 0;
    while (byte_total < size) {
        int byte_write = rb_write(source_ringbuf, data, size, 3000);
        if (byte_write < 0) {
            if (byte_write != RB_DONE) {
                ret = ESP_FAIL;
                OS_LOGE(TAG, "Wrong write to RB with error[%d]", byte_write);
            }
            goto write_done;
        } else {
            byte_total += byte_write;
        }
    }

write_done:
    if (final) {
        rb_done_write(source_ringbuf);
        ret = ESP_OK;
    }

    os_mutex_unlock(handle->io_lock);
    return ret;
}

int liteplayer_start(liteplayer_handle_t handle)
{
    if (handle == NULL)
        return ESP_FAIL;

    OS_LOGI(TAG, "Starting player[%s]", media_source_tag(handle->source_type));

    os_mutex_lock(handle->io_lock);

    if (handle->state != LITEPLAYER_PREPARED &&
        handle->state != LITEPLAYER_PAUSED &&
        handle->state != LITEPLAYER_SEEKCOMPLETED) {
        OS_LOGE(TAG, "Can't start in state=[%d]", handle->state);
        os_mutex_unlock(handle->io_lock);
        return ESP_FAIL;
    }

    if (handle->media_parser_handle != NULL) {
        media_parser_stop(handle->media_parser_handle);
        handle->media_parser_handle = NULL;
    }

    int ret = ESP_OK;

    if (handle->state == LITEPLAYER_PREPARED) {
        if (handle->audio_pipeline == NULL)
            ret = main_pipeline_init(handle);
    } else {
        if (handle->audio_pipeline == NULL)
            ret = ESP_FAIL;
    }
    if (ret == ESP_OK)
        ret = audio_pipeline_resume(handle->audio_pipeline, 0, 0);

    {
        os_mutex_lock(handle->state_lock);
        handle->state = (ret == ESP_OK) ? LITEPLAYER_STARTED : LITEPLAYER_ERROR;
        media_player_state_callback(handle, handle->state, ret);
        os_mutex_unlock(handle->state_lock);
    }

    os_mutex_unlock(handle->io_lock);
    return ret;
}

int liteplayer_pause(liteplayer_handle_t handle)
{
    if (handle == NULL)
        return ESP_FAIL;

    OS_LOGI(TAG, "Pausing player[%s]", media_source_tag(handle->source_type));

    os_mutex_lock(handle->io_lock);

    if (handle->state != LITEPLAYER_STARTED) {
        OS_LOGE(TAG, "Can't pause in state=[%d]", handle->state);
        os_mutex_unlock(handle->io_lock);
        return ESP_FAIL;
    }

    int ret = audio_pipeline_pause(handle->audio_pipeline);

    {
        os_mutex_lock(handle->state_lock);
        handle->state = (ret == ESP_OK) ? LITEPLAYER_PAUSED : LITEPLAYER_ERROR;
        media_player_state_callback(handle, handle->state, ret);
        os_mutex_unlock(handle->state_lock);
    }

    os_mutex_unlock(handle->io_lock);
    return ret;
}

int liteplayer_resume(liteplayer_handle_t handle)
{
    if (handle == NULL)
        return ESP_FAIL;

    OS_LOGI(TAG, "Resuming player[%s]", media_source_tag(handle->source_type));

    os_mutex_lock(handle->io_lock);

    if (handle->state != LITEPLAYER_PAUSED && handle->state != LITEPLAYER_SEEKCOMPLETED) {
        OS_LOGE(TAG, "Can't resume in state=[%d]", handle->state);
        os_mutex_unlock(handle->io_lock);
        return ESP_FAIL;
    }

    int ret = audio_pipeline_resume(handle->audio_pipeline, 0, 0);

    {
        os_mutex_lock(handle->state_lock);
        handle->state = (ret == ESP_OK) ? LITEPLAYER_STARTED : LITEPLAYER_ERROR;
        media_player_state_callback(handle, handle->state, ret);
        os_mutex_unlock(handle->state_lock);
    }

    os_mutex_unlock(handle->io_lock);
    return ret;
}

int liteplayer_seek(liteplayer_handle_t handle, int msec)
{
    if (handle == NULL || msec < 0)
        return ESP_FAIL;

    OS_LOGI(TAG, "Seeking player[%s], offset=%d(s)", media_source_tag(handle->source_type), (int)(msec/1000));

    int ret = ESP_FAIL;
    bool state_sync = false;

    os_mutex_lock(handle->io_lock);

    if (handle->state < LITEPLAYER_PREPARED || handle->state > LITEPLAYER_NEARLYCOMPLETED) {
        OS_LOGE(TAG, "Can't seek in state=[%d]", handle->state);
        ret = ESP_OK;
        goto seek_out;
    }

    if (msec >= handle->codec_info.duration_ms) {
        OS_LOGE(TAG, "Invalid seek time");
        ret = ESP_OK;
        goto seek_out;
    }

    long long offset = 0;
    switch (handle->codec_info.codec_type) {
    case AUDIO_CODEC_WAV:
    case AUDIO_CODEC_MP3: {
        offset = (handle->codec_info.bytes_per_sec*(msec/1000));
        break;
    }
    case AUDIO_CODEC_M4A: {
        unsigned int sample_index = 0;
        unsigned int sample_offset = 0;
        if (m4a_get_seek_offset(msec, &(handle->codec_info.detail.m4a_info), &sample_index, &sample_offset) != 0) {
            ret = ESP_OK;
            goto seek_out;
        }
        offset = (long long)sample_offset - handle->codec_info.content_pos;
        handle->codec_info.detail.m4a_info.stsz_samplesize_index = sample_index;
        break;
    }
    default:
        OS_LOGE(TAG, "Unsupported seek for codec: %d", handle->codec_info.codec_type);
        ret = ESP_OK;
        goto seek_out;
    }
    if (handle->codec_info.content_len > 0 && offset >= handle->codec_info.content_len) {
        OS_LOGE(TAG, "Invalid seek offset");
        ret = ESP_OK;
        goto seek_out;
    }

    handle->seek_time = (msec/1000)*1000;
    handle->seek_offset = offset;
    handle->sink_position = 0;

    state_sync = true;

    if (handle->media_parser_handle != NULL) {
        media_parser_stop(handle->media_parser_handle);
        handle->media_parser_handle = NULL;
    }

    if (handle->audio_pipeline == NULL) {
        ret = main_pipeline_init(handle);
        if (ret != ESP_OK)
            goto seek_out;
    } else {
        ret = audio_pipeline_pause(handle->audio_pipeline);
        if (ret != ESP_OK)
            goto seek_out;

        if (handle->media_source_handle != NULL) {
            media_source_stop(handle->media_source_handle);
            handle->media_source_handle = NULL;
        }
        if (handle->source_ringbuf != NULL) {
            rb_destroy(handle->source_ringbuf);
            handle->source_ringbuf = NULL;
        }

        if (handle->source_type == MEDIA_SOURCE_STREAM)
            handle->source_ringbuf = rb_create(DEFAULT_MEDIA_SOURCE_STREAM_RINGBUF_SIZE);
        else if (handle->source_type == MEDIA_SOURCE_HTTP)
            handle->source_ringbuf = rb_create(DEFAULT_MEDIA_SOURCE_HTTP_RINGBUF_SIZE);
        else if (handle->source_type == MEDIA_SOURCE_FILE)
            handle->source_ringbuf = rb_create(DEFAULT_MEDIA_SOURCE_FILE_RINGBUF_SIZE);
        AUDIO_MEM_CHECK(TAG, handle->source_ringbuf, goto seek_out);

        audio_element_set_input_ringbuf(handle->ael_decoder, handle->source_ringbuf);

        if (handle->source_type == MEDIA_SOURCE_HTTP) {
            struct media_source_info info = {
                .url = handle->url,
                .source_type = MEDIA_SOURCE_HTTP,
                .http_ops = {
                    .http_priv = handle->http_ops.http_priv,
                    .open = handle->http_ops.open,
                    .read = handle->http_ops.read,
                    .seek = handle->http_ops.seek,
                    .close = handle->http_ops.close,
                },
                .content_pos = handle->codec_info.content_pos + handle->seek_offset,
                .threshold_size = 0,
            };
            handle->media_source_handle = media_source_start(&info, handle->source_ringbuf, media_source_state_callback, handle);
            AUDIO_MEM_CHECK(TAG, handle->media_source_handle, goto seek_out);
        } else if (handle->source_type == MEDIA_SOURCE_FILE) {
            struct media_source_info info = {
                .url = handle->url,
                .source_type = MEDIA_SOURCE_FILE,
                .file_ops = {
                    .file_priv = handle->file_ops.file_priv,
                    .open = handle->file_ops.open,
                    .read = handle->file_ops.read,
                    .seek = handle->file_ops.seek,
                    .close = handle->file_ops.close,
                },
                .content_pos = handle->codec_info.content_pos + handle->seek_offset,
                .threshold_size = 0,
            };
            handle->media_source_handle = media_source_start(&info, handle->source_ringbuf, media_source_state_callback, handle);
            AUDIO_MEM_CHECK(TAG, handle->media_source_handle, goto seek_out);
        }
    }

    ret = audio_pipeline_seek(handle->audio_pipeline, handle->seek_offset);
    if (ret != ESP_OK)
        goto seek_out;

seek_out:
    if (state_sync) {
        os_mutex_lock(handle->state_lock);
        handle->state = (ret == ESP_OK) ? LITEPLAYER_SEEKCOMPLETED : LITEPLAYER_ERROR;
        media_player_state_callback(handle, handle->state, ret);
        os_mutex_unlock(handle->state_lock);
    }

    os_mutex_unlock(handle->io_lock);
    return ret;
}

int liteplayer_stop(liteplayer_handle_t handle)
{
    if (handle == NULL)
        return ESP_FAIL;

    int ret = ESP_OK;

    OS_LOGI(TAG, "Stopping player[%s]", media_source_tag(handle->source_type));

    os_mutex_lock(handle->io_lock);

    if (handle->state == LITEPLAYER_ERROR)
        goto stop_out;

    if (handle->state < LITEPLAYER_PREPARED || handle->state > LITEPLAYER_COMPLETED) {
        OS_LOGE(TAG, "Can't stop in state=[%d]", handle->state);
        os_mutex_unlock(handle->io_lock);
        return ESP_FAIL;
    }

    ret = audio_pipeline_stop(handle->audio_pipeline);
    ret |= audio_pipeline_wait_for_stop(handle->audio_pipeline);
    audio_element_reset_state(handle->ael_decoder);
    audio_pipeline_reset_ringbuffer(handle->audio_pipeline);
    audio_pipeline_reset_items_state(handle->audio_pipeline);

stop_out:
    {
        os_mutex_lock(handle->state_lock);
        handle->state = LITEPLAYER_STOPPED;
        media_player_state_callback(handle, handle->state, ret);
        os_mutex_unlock(handle->state_lock);
    }

    os_mutex_unlock(handle->io_lock);
    return ret;
}

int liteplayer_reset(liteplayer_handle_t handle)
{
    if (handle == NULL)
        return ESP_FAIL;

    OS_LOGI(TAG, "Resetting player[%s]", media_source_tag(handle->source_type));

    os_mutex_lock(handle->io_lock);

    main_pipeline_deinit(handle);

    if (handle->url != NULL) {
        audio_free(handle->url);
        handle->url = NULL;
    }

    if (handle->codec_info.codec_type == AUDIO_CODEC_M4A) {
        if (handle->codec_info.detail.m4a_info.stsz_samplesize != NULL)
            audio_free(handle->codec_info.detail.m4a_info.stsz_samplesize);
        if (handle->codec_info.detail.m4a_info.stts_time2sample != NULL)
            audio_free(handle->codec_info.detail.m4a_info.stts_time2sample);
        if (handle->codec_info.detail.m4a_info.stsc_sample2chunk != NULL)
            audio_free(handle->codec_info.detail.m4a_info.stsc_sample2chunk);
        if (handle->codec_info.detail.m4a_info.stco_chunk2offset != NULL)
            audio_free(handle->codec_info.detail.m4a_info.stco_chunk2offset);
    } else if (handle->codec_info.codec_type == AUDIO_CODEC_WAV) {
        if (handle->codec_info.detail.wav_info.header_buff != NULL)
            audio_free(handle->codec_info.detail.wav_info.header_buff);
    }

    handle->source_type = MEDIA_SOURCE_UNKNOWN;
    handle->state_error = false;
    handle->sink_samplerate = 0;
    handle->sink_channels = 0;
    handle->sink_bits = 0;
    handle->sink_position = 0;
    handle->seek_time = 0;
    handle->seek_offset = 0;
    memset(&handle->codec_info, 0x0, sizeof(handle->codec_info));

    {
        os_mutex_lock(handle->state_lock);

        if (handle->state != LITEPLAYER_STOPPED) {
            handle->state = LITEPLAYER_STOPPED;
            media_player_state_callback(handle, LITEPLAYER_STOPPED, 0);
        }

        handle->state = LITEPLAYER_IDLE;
        media_player_state_callback(handle, LITEPLAYER_IDLE, 0);

        os_mutex_unlock(handle->state_lock);
    }

    os_mutex_unlock(handle->io_lock);
    return ESP_OK;
}

int liteplayer_get_available_size(liteplayer_handle_t handle)
{
    if (handle == NULL || handle->ael_decoder == NULL)
        return 0;

    ringbuf_handle source_ringbuf = audio_element_get_input_ringbuf(handle->ael_decoder);
    if (source_ringbuf != NULL)
        return rb_bytes_available(source_ringbuf);
    else
        return 0;
}

int liteplayer_get_position(liteplayer_handle_t handle, int *msec)
{
    if (handle == NULL || msec == NULL)
        return ESP_FAIL;

    int samplerate = handle->sink_samplerate;
    int channels = handle->sink_channels;
    int bits = handle->sink_bits;
    long long position = handle->sink_position;
    int seek_time = handle->seek_time;

    if (samplerate == 0 || channels == 0 || bits == 0) {
        *msec = 0;
        return ESP_OK;
    }

    int bytes_per_sample = channels * bits / 8;
    long long out_samples = position / bytes_per_sample;
    *msec = (int)(out_samples/(samplerate/1000) + seek_time);
    return ESP_OK;
}

int liteplayer_get_duration(liteplayer_handle_t handle, int *msec)
{
    if (handle == NULL || msec == NULL)
        return ESP_FAIL;

    if (handle->state < LITEPLAYER_PREPARED)
        return ESP_FAIL;

    *msec = handle->codec_info.duration_ms;
    return ESP_OK;
}

void liteplayer_destroy(liteplayer_handle_t handle)
{
    if (handle == NULL)
        return;

    if (handle->state != LITEPLAYER_IDLE)
        liteplayer_reset(handle);

    os_mutex_destroy(handle->state_lock);
    os_mutex_destroy(handle->io_lock);
    audio_free(handle);
}
