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
#include <stdint.h>

#include "msgutils/ringbuf.h"
#include "msgutils/os_logger.h"
#include "esp_adf/audio_element.h"
#include "esp_adf/audio_pipeline.h"
#include "esp_adf/audio_event_iface.h"
#include "esp_adf/audio_common.h"
#include "audio_decoder/mp3_decoder.h"
#include "audio_decoder/aac_decoder.h"
#include "audio_decoder/m4a_decoder.h"
#include "audio_decoder/wav_decoder.h"
#include "audio_stream/sink_stream.h"

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
    os_mutex_t              state_lock;
    liteplayer_state_cb     state_listener;
    void                   *state_userdata;
    bool                    state_error;

    os_mutex_t              io_lock;

    struct file_wrapper     file_ops;
    struct http_wrapper     http_ops;
    struct sink_wrapper     sink_ops;

    audio_pipeline_handle_t pipeline;
    audio_element_handle_t  el_source;
    audio_element_handle_t  el_decoder;
    audio_element_handle_t  el_sink;
    ringbuf_handle_t        source_rb;

    enum media_source_type  source_type;
    media_source_handle_t   media_source;
    media_parser_handle_t   media_parser;
    struct media_codec_info codec_info;
    int                     threshold_ms;

    int                     sink_samplerate;
    int                     sink_channels;
    int                     sink_bits;
    long long               sink_position;

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

static void media_player_state_callback(liteplayer_handle_t handle, enum liteplayer_state state, int errcode)
{
    if (state == LITEPLAYER_ERROR) {
        if (!handle->state_error) {
            handle->state_error = true;
            if (handle->state_listener)
                handle->state_listener(LITEPLAYER_ERROR, errcode, handle->state_userdata);
        }
    }
    else {
        if (!handle->state_error || state == LITEPLAYER_IDLE || state == LITEPLAYER_STOPPED) {
            if (handle->state_listener)
                handle->state_listener(state, 0, handle->state_userdata);
        }
    }
}

static int audio_element_state_callback(audio_element_handle_t el, audio_event_iface_msg_t *msg, void *ctx)
{
    liteplayer_handle_t handle = (liteplayer_handle_t)ctx;

    OS_THREAD_MUTEX_LOCK(handle->state_lock);

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
                if (msg->source == (void *)handle->el_decoder) {
                    OS_LOGW(TAG, "[ %s-%s ] Receive inputtimeout event, filled/total: %d/%d",
                            media_source_tag(handle->source_type),
                            audio_element_get_tag(el),
                            rb_bytes_filled(handle->source_rb),
                            rb_get_size(handle->source_rb));
                }
                break;

            case AEL_STATUS_STATE_RUNNING:
                if (msg->source == (void *)handle->el_sink) {
                    OS_LOGD(TAG, "[ %s-%s ] Receive started event",
                            media_source_tag(handle->source_type), audio_element_get_tag(el));
                    //handle->state = LITEPLAYER_STARTED;
                    //media_player_state_callback(handle, LITEPLAYER_STARTED, 0);
                }
                break;

            case AEL_STATUS_STATE_PAUSED:
                if (msg->source == (void *)handle->el_sink) {
                    OS_LOGD(TAG, "[ %s-%s ] Receive paused event",
                            media_source_tag(handle->source_type), audio_element_get_tag(el));
                    //handle->state = LITEPLAYER_PAUSED;
                    //media_player_state_callback(handle, LITEPLAYER_PAUSED, 0);
                }
                break;

            case AEL_STATUS_INPUT_DONE:
                if (msg->source == (void *)handle->el_source) {
                    OS_LOGD(TAG, "[ %s-%s ] Receive inputdone event",
                            media_source_tag(handle->source_type), audio_element_get_tag(el));
                    if (handle->source_type == MEDIA_SOURCE_HTTP)
                        media_player_state_callback(handle, LITEPLAYER_NEARLYCOMPLETED, 0);
                }
                break;

            case AEL_STATUS_STATE_FINISHED:
                if (msg->source == (void *)handle->el_sink) {
                    OS_LOGD(TAG, "[ %s-%s ] Receive finished event",
                            media_source_tag(handle->source_type), audio_element_get_tag(el));
                    if (handle->state != LITEPLAYER_ERROR && handle->state != LITEPLAYER_STOPPED) {
                        handle->state = LITEPLAYER_COMPLETED;
                        media_player_state_callback(handle, LITEPLAYER_COMPLETED, 0);
                    }
                }
                break;

            case AEL_STATUS_STATE_STOPPED:
                if (msg->source == (void *)handle->el_sink) {
                    OS_LOGD(TAG, "[ %s-%s ] Receive stopped event",
                            media_source_tag(handle->source_type), audio_element_get_tag(el));
                    //handle->state = LITEPLAYER_STOPPED;
                    //media_player_state_callback(handle, LITEPLAYER_STOPPED, 0);
                }
                break;

            default:
                break;
            }
        }
        else if (msg->cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            if (msg->source == (void *)handle->el_decoder) {
                audio_element_info_t decoder_info = {0};
                audio_element_getinfo(handle->el_decoder, &decoder_info);
                OS_LOGD(TAG, "[ %s-%s ] Receive decoder info: samplerate=%d, ch=%d, bits=%d",
                        media_source_tag(handle->source_type), audio_element_get_tag(el),
                        decoder_info.out_samplerate, decoder_info.out_channels, decoder_info.bits);

                audio_element_info_t sink_info = {0};
                audio_element_getinfo(handle->el_sink, &sink_info);
                if (sink_info.in_samplerate != decoder_info.out_samplerate ||
                    sink_info.in_channels != decoder_info.out_channels) {
                    OS_LOGW(TAG, "Forcely update sink samplerate(%d>>%d), channels(%d>>%d)",
                             sink_info.in_samplerate, decoder_info.out_samplerate,
                             sink_info.in_channels, decoder_info.out_channels);
                    sink_info.in_channels = decoder_info.out_channels;
                    sink_info.in_samplerate = decoder_info.out_samplerate;
                    audio_element_setinfo(handle->el_sink, &sink_info);
                }
            }
            else if (msg->source == (void *)handle->el_sink) {
                audio_element_info_t sink_info = {0};
                audio_element_getinfo(handle->el_sink, &sink_info);
                OS_LOGD(TAG, "[ %s-%s ] Receive sink info: samplerate=%d, ch=%d, bits=%d",
                         media_source_tag(handle->source_type), audio_element_get_tag(el),
                         sink_info.out_samplerate, sink_info.out_channels, sink_info.bits);
                handle->sink_samplerate = sink_info.out_samplerate;
                handle->sink_channels = sink_info.out_channels;
                handle->sink_bits = sink_info.bits;
            }
        }
        else if (msg->cmd == AEL_MSG_CMD_REPORT_POSITION) {
            if (msg->source == (void *) handle->el_sink) {
                handle->sink_position = (long long)msg->data;
                //OS_LOGV(TAG, "[ %s-%s ] Receive postion info: pos=%ld",
                //         media_source_tag(handle->source_type), audio_element_get_tag(el),
                //         (long)handle->sink_position);
            }
        }
    }

    OS_THREAD_MUTEX_UNLOCK(handle->state_lock);
    return ESP_OK;
}

static void media_source_state_callback(enum media_source_state state, void *priv)
{
    liteplayer_handle_t handle = (liteplayer_handle_t)priv;

    OS_THREAD_MUTEX_LOCK(handle->state_lock);

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
                rb_get_threshold(handle->source_rb), rb_get_size(handle->source_rb));
        if (handle->source_type == MEDIA_SOURCE_HTTP)
            media_player_state_callback(handle, LITEPLAYER_CACHECOMPLETED, 0);
        break;
    default:
        break;
    }

    OS_THREAD_MUTEX_UNLOCK(handle->state_lock);
}

static void media_parser_state_callback(enum media_parser_state state, struct media_codec_info *codec_info, void *priv)
{
    liteplayer_handle_t handle = (liteplayer_handle_t)priv;

    OS_THREAD_MUTEX_LOCK(handle->state_lock);

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

    OS_THREAD_MUTEX_UNLOCK(handle->state_lock);
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
    if (handle->pipeline != NULL) {
        OS_LOGD(TAG, "Destroy audio pipeline");
        audio_pipeline_deinit(handle->pipeline);
        handle->pipeline = NULL;
        handle->el_source = NULL;
        handle->el_decoder = NULL;
        handle->el_sink = NULL;
    }

    if (handle->media_parser != NULL) {
        media_parser_stop(handle->media_parser);
        handle->media_parser = NULL;
    }

    if (handle->media_source != NULL) {
        media_source_stop(handle->media_source);
        handle->media_source = NULL;
    }

    if (handle->source_rb != NULL) {
        rb_destroy(handle->source_rb);
        handle->source_rb = NULL;
    }
}

static int main_pipeline_init(liteplayer_handle_t handle)
{
    {
        OS_LOGD(TAG, "[1.0] Create audio pipeline");
        audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
        handle->pipeline = audio_pipeline_init(&pipeline_cfg);
        AUDIO_MEM_CHECK(TAG, handle->pipeline, goto pipeline_fail);
    }

    {
        OS_LOGD(TAG, "[2.0] Create sink element");
        struct sink_stream_cfg sink_cfg = SINK_STREAM_CFG_DEFAULT();
        sink_cfg.task_prio                = DEFAULT_SINK_TASK_PRIO;
        sink_cfg.task_stack               = DEFAULT_SINK_TASK_STACKSIZE;
        sink_cfg.out_rb_size              = DEFAULT_SINK_RINGBUF_SIZE;
        sink_cfg.buf_sz                   = DEFAULT_SINK_BUFFER_SIZE;
        sink_cfg.in_channels              = handle->codec_info.codec_channels;
        sink_cfg.in_samplerate            = handle->codec_info.codec_samplerate;
        sink_cfg.out_samplerate           = DEFAULT_SINK_OUT_RATE;
        sink_cfg.out_channels             = DEFAULT_SINK_OUT_CHANNELS;
        sink_cfg.sink_priv                = handle->sink_ops.sink_priv;
        sink_cfg.sink_open                = handle->sink_ops.open;
        sink_cfg.sink_write               = handle->sink_ops.write;
        sink_cfg.sink_close               = handle->sink_ops.close;
        handle->el_sink = sink_stream_init(&sink_cfg);
        AUDIO_MEM_CHECK(TAG, handle->el_sink, goto pipeline_fail);

        audio_element_info_t info;
        audio_element_getinfo(handle->el_sink, &info);
        info.in_channels = handle->codec_info.codec_channels;
        info.in_samplerate = handle->codec_info.codec_samplerate;
        info.bits = 16;
        audio_element_setinfo(handle->el_sink, &info);
    }

    {
        OS_LOGD(TAG, "[2.1] Create decoder element");
        if (handle->codec_info.codec_type == AUDIO_CODEC_MP3) {
            struct mp3_decoder_cfg mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
            mp3_cfg.task_prio            = DEFAULT_DECODER_TASK_PRIO;
            mp3_cfg.task_stack           = DEFAULT_DECODER_TASK_STACKSIZE;
            mp3_cfg.out_rb_size          = DEFAULT_DECODER_RINGBUF_SIZE;
            mp3_cfg.mp3_info             = &(handle->codec_info.detail.mp3_info);
            handle->el_decoder = mp3_decoder_init(&mp3_cfg);
        }
        else if (handle->codec_info.codec_type == AUDIO_CODEC_AAC) {
            struct aac_decoder_cfg aac_cfg = DEFAULT_AAC_DECODER_CONFIG();
            aac_cfg.task_prio            = DEFAULT_DECODER_TASK_PRIO;
            aac_cfg.task_stack           = DEFAULT_DECODER_TASK_STACKSIZE;
            aac_cfg.out_rb_size          = DEFAULT_DECODER_RINGBUF_SIZE;
            handle->el_decoder = aac_decoder_init(&aac_cfg);
        }
        else if (handle->codec_info.codec_type == AUDIO_CODEC_M4A) {
            struct m4a_decoder_cfg m4a_cfg = DEFAULT_M4A_DECODER_CONFIG();
            m4a_cfg.task_prio            = DEFAULT_DECODER_TASK_PRIO;
            m4a_cfg.task_stack           = DEFAULT_DECODER_TASK_STACKSIZE;
            m4a_cfg.out_rb_size          = DEFAULT_DECODER_RINGBUF_SIZE;
            m4a_cfg.m4a_info             = &(handle->codec_info.detail.m4a_info);
            handle->el_decoder = m4a_decoder_init(&m4a_cfg);
        }
        else if (handle->codec_info.codec_type == AUDIO_CODEC_WAV) {
            struct wav_decoder_cfg wav_cfg = DEFAULT_WAV_DECODER_CONFIG();
            wav_cfg.task_prio            = DEFAULT_DECODER_TASK_PRIO;
            wav_cfg.task_stack           = DEFAULT_DECODER_TASK_STACKSIZE;
            wav_cfg.out_rb_size          = DEFAULT_DECODER_RINGBUF_SIZE;
            wav_cfg.wav_info             = &(handle->codec_info.detail.wav_info);
            handle->el_decoder = wav_decoder_init(&wav_cfg);
        }
        AUDIO_MEM_CHECK(TAG, handle->el_decoder, goto pipeline_fail);
    }

    {
        OS_LOGD(TAG, "[2.2] Create source element");
        if (handle->source_type == MEDIA_SOURCE_STREAM)
            handle->source_rb = rb_create(DEFAULT_MEDIA_SOURCE_STREAM_RINGBUF_SIZE);
        else if (handle->source_type == MEDIA_SOURCE_HTTP)
            handle->source_rb = rb_create(DEFAULT_MEDIA_SOURCE_HTTP_RINGBUF_SIZE);
        else if (handle->source_type == MEDIA_SOURCE_FILE)
            handle->source_rb = rb_create(DEFAULT_MEDIA_SOURCE_FILE_RINGBUF_SIZE);
        AUDIO_MEM_CHECK(TAG, handle->source_rb, goto pipeline_fail);

        audio_element_set_input_ringbuf(handle->el_decoder, handle->source_rb);

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
            handle->media_source = media_source_start(&info, handle->source_rb, media_source_state_callback, handle);
            AUDIO_MEM_CHECK(TAG, handle->media_source, goto pipeline_fail);
        }
        else if (handle->source_type == MEDIA_SOURCE_FILE) {
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
            handle->media_source = media_source_start(&info, handle->source_rb, media_source_state_callback, handle);
            AUDIO_MEM_CHECK(TAG, handle->media_source, goto pipeline_fail);
        }
    }

    {
        OS_LOGD(TAG, "[3.0] Register all elements to audio pipeline");
        audio_pipeline_register(handle->pipeline, handle->el_decoder, "decoder");
        audio_pipeline_register(handle->pipeline, handle->el_sink, "sink_w");

        OS_LOGD(TAG, "[3.1] Link elements together source_rb->decoder->sink_w");
        audio_pipeline_link(handle->pipeline, (const char *[]){"decoder", "sink_w"}, 2);

        OS_LOGD(TAG, "[3.2] Register event callback of all elements");
        audio_element_set_event_callback(handle->el_decoder, audio_element_state_callback, handle);
        audio_element_set_event_callback(handle->el_sink, audio_element_state_callback, handle);
    }

    {
        OS_LOGD(TAG, "[4.0] Run audio pipeline");
        if (audio_pipeline_run(handle->pipeline) != 0)
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
        handle->io_lock = OS_THREAD_MUTEX_CREATE();
        handle->state_lock = OS_THREAD_MUTEX_CREATE();
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
    if (handle == NULL || url == NULL)
        return ESP_FAIL;

    OS_LOGI(TAG, "Set player[%s] source:%s", media_source_tag(handle->source_type), url);

    OS_THREAD_MUTEX_LOCK(handle->io_lock);

    if (handle->state != LITEPLAYER_IDLE) {
        OS_LOGE(TAG, "Can't set source in state=[%d]", handle->state);
        OS_THREAD_MUTEX_UNLOCK(handle->io_lock);
        return ESP_FAIL;
    }

    handle->url = audio_strdup(url);
    handle->threshold_ms = threshold_ms;
    handle->state_error = false;

    if (strncmp(url, DEFAULT_STREAM_FIXED_URL, 11) == 0)
        handle->source_type = MEDIA_SOURCE_STREAM;
    else if (strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0)
        handle->source_type = MEDIA_SOURCE_HTTP;
    else
        handle->source_type = MEDIA_SOURCE_FILE;

    {
        OS_THREAD_MUTEX_LOCK(handle->state_lock);
        handle->state = LITEPLAYER_INITED;
        media_player_state_callback(handle, LITEPLAYER_INITED, 0);
        OS_THREAD_MUTEX_UNLOCK(handle->state_lock);
    }

    OS_THREAD_MUTEX_UNLOCK(handle->io_lock);
    return ESP_OK;
}

int liteplayer_prepare(liteplayer_handle_t handle)
{
    if (handle == NULL)
        return ESP_FAIL;

    OS_LOGI(TAG, "Preparing player[%s]", media_source_tag(handle->source_type));

    OS_THREAD_MUTEX_LOCK(handle->io_lock);

    if (handle->state != LITEPLAYER_INITED) {
        OS_LOGE(TAG, "Can't prepare in state=[%d]", handle->state);
        OS_THREAD_MUTEX_UNLOCK(handle->io_lock);
        return ESP_FAIL;
    }

    int ret = ESP_OK;

    if (handle->source_type == MEDIA_SOURCE_STREAM) {
        // TODO: Fixed mp3/16KHz/mono
        handle->codec_info.codec_type = DEFAULT_STREAM_FIXED_CODEC;
        handle->codec_info.codec_samplerate = DEFAULT_STREAM_FIXED_SAMPLERATE;
        handle->codec_info.codec_channels = DEFAULT_STREAM_FIXED_CHANNELS;
    }
    else if (handle->source_type == MEDIA_SOURCE_HTTP) {
        struct media_source_info source_info = {0};
        source_info.url = handle->url;
        source_info.source_type = MEDIA_SOURCE_HTTP;
        memcpy(&source_info.http_ops, &handle->http_ops, sizeof(struct http_wrapper));
        ret = media_info_parse(&source_info, &handle->codec_info);
    }
    else {
        struct media_source_info source_info = {0};
        source_info.url = handle->url;
        source_info.source_type = MEDIA_SOURCE_FILE;
        memcpy(&source_info.file_ops, &handle->file_ops, sizeof(struct file_wrapper));
        ret = media_info_parse(&source_info, &handle->codec_info);
    }

    if (ret == ESP_OK)
        ret = main_pipeline_init(handle);

    {
        OS_THREAD_MUTEX_LOCK(handle->state_lock);
        handle->state = (ret == ESP_OK) ? LITEPLAYER_PREPARED : LITEPLAYER_ERROR;
        media_player_state_callback(handle, handle->state, ret);
        OS_THREAD_MUTEX_UNLOCK(handle->state_lock);
    }

    OS_THREAD_MUTEX_UNLOCK(handle->io_lock);
    return ret;
}

int liteplayer_prepare_async(liteplayer_handle_t handle)
{
    if (handle == NULL)
        return ESP_FAIL;

    OS_LOGI(TAG, "Async preparing player[%s]", media_source_tag(handle->source_type));

    OS_THREAD_MUTEX_LOCK(handle->io_lock);

    if (handle->state != LITEPLAYER_INITED) {
        OS_LOGE(TAG, "Can't prepare in state=[%d]", handle->state);
        OS_THREAD_MUTEX_UNLOCK(handle->io_lock);
        return ESP_FAIL;
    }

    int ret = ESP_OK;

    if (handle->source_type == MEDIA_SOURCE_STREAM) {
        // TODO: Fixed mp3/16KHz/mono
        handle->codec_info.codec_type = DEFAULT_STREAM_FIXED_CODEC;
        handle->codec_info.codec_samplerate = DEFAULT_STREAM_FIXED_SAMPLERATE;
        handle->codec_info.codec_channels = DEFAULT_STREAM_FIXED_CHANNELS;
    }
    else if (handle->source_type == MEDIA_SOURCE_HTTP) {
        struct media_source_info source_info = {0};
        source_info.url = handle->url;
        source_info.source_type = MEDIA_SOURCE_HTTP;
        memcpy(&source_info.http_ops, &handle->http_ops, sizeof(struct http_wrapper));
        handle->media_parser = media_parser_start_async(&source_info, media_parser_state_callback, handle);
        if (handle->media_parser == NULL) {
            ret = ESP_FAIL;
            OS_THREAD_MUTEX_LOCK(handle->state_lock);
            handle->state = LITEPLAYER_ERROR;
            media_player_state_callback(handle, LITEPLAYER_ERROR, ret);
            OS_THREAD_MUTEX_UNLOCK(handle->state_lock);
        }
    }
    else {
        struct media_source_info source_info = {0};
        source_info.url = handle->url;
        source_info.source_type = MEDIA_SOURCE_FILE;
        memcpy(&source_info.file_ops, &handle->file_ops, sizeof(struct file_wrapper));
        ret = media_info_parse(&source_info, &handle->codec_info);
    }

    if (handle->source_type != MEDIA_SOURCE_HTTP) {
        if (ret == ESP_OK)
            ret = main_pipeline_init(handle);

        OS_THREAD_MUTEX_LOCK(handle->state_lock);
        handle->state = (ret == ESP_OK) ? LITEPLAYER_PREPARED : LITEPLAYER_ERROR;
        media_player_state_callback(handle, handle->state, ret);
        OS_THREAD_MUTEX_UNLOCK(handle->state_lock);
    }

    OS_THREAD_MUTEX_UNLOCK(handle->io_lock);
    return ret;
}

int liteplayer_write(liteplayer_handle_t handle, char *data, int size, bool final)
{
    if (handle == NULL)
        return ESP_FAIL;

    OS_LOGV(TAG, "Writing player[%s], size=%d, final=%d", media_source_tag(handle->source_type), size, final);

    ringbuf_handle_t source_rb = audio_element_get_input_ringbuf(handle->el_decoder);
    if (source_rb == NULL || handle->source_type != MEDIA_SOURCE_STREAM) {
        OS_LOGE(TAG, "Invalid source_rb or source_type");
        return ESP_FAIL;
    }

    OS_THREAD_MUTEX_LOCK(handle->io_lock);

    if (handle->state < LITEPLAYER_PREPARED || handle->state > LITEPLAYER_NEARLYCOMPLETED) {
        OS_THREAD_MUTEX_UNLOCK(handle->io_lock);
        OS_LOGE(TAG, "Can't write data in state=[%d]", handle->state);
        return ESP_FAIL;
    }

    int ret = ESP_OK;
    int byte_total = 0;
    while (byte_total < size) {
        int byte_write = rb_write(source_rb, data, size, 3000);
        if (byte_write < 0) {
            if (byte_write != RB_DONE) {
                ret = ESP_FAIL;
                OS_LOGE(TAG, "Wrong write to RB with error[%d]", byte_write);
            }
            goto write_done;
        }
        else {
            byte_total += byte_write;
        }
    }

write_done:
    if (final) {
        rb_done_write(source_rb);
        ret = ESP_OK;
    }

    OS_THREAD_MUTEX_UNLOCK(handle->io_lock);
    return ret;
}

int liteplayer_start(liteplayer_handle_t handle)
{
    if (handle == NULL)
        return ESP_FAIL;

    OS_LOGI(TAG, "Starting player[%s]", media_source_tag(handle->source_type));

    OS_THREAD_MUTEX_LOCK(handle->io_lock);

    if (handle->state != LITEPLAYER_PREPARED &&
        handle->state != LITEPLAYER_PAUSED &&
        handle->state != LITEPLAYER_SEEKCOMPLETED) {
        OS_LOGE(TAG, "Can't start in state=[%d]", handle->state);
        OS_THREAD_MUTEX_UNLOCK(handle->io_lock);
        return ESP_FAIL;
    }

    if (handle->media_parser != NULL) {
        media_parser_stop(handle->media_parser);
        handle->media_parser = NULL;
    }

    int ret = ESP_OK;

    if (handle->state == LITEPLAYER_PREPARED) {
        if (handle->pipeline == NULL)
            ret = main_pipeline_init(handle);
    }
    else {
        if (handle->pipeline == NULL)
            ret = ESP_FAIL;
    }
    if (ret == ESP_OK)
        ret = audio_pipeline_resume(handle->pipeline, 0, 0);

    {
        OS_THREAD_MUTEX_LOCK(handle->state_lock);
        handle->state = (ret == ESP_OK) ? LITEPLAYER_STARTED : LITEPLAYER_ERROR;
        media_player_state_callback(handle, handle->state, ret);
        OS_THREAD_MUTEX_UNLOCK(handle->state_lock);
    }

    OS_THREAD_MUTEX_UNLOCK(handle->io_lock);
    return ret;
}

int liteplayer_pause(liteplayer_handle_t handle)
{
    if (handle == NULL)
        return ESP_FAIL;

    OS_LOGI(TAG, "Pausing player[%s]", media_source_tag(handle->source_type));

    OS_THREAD_MUTEX_LOCK(handle->io_lock);

    if (handle->state != LITEPLAYER_STARTED) {
        OS_LOGE(TAG, "Can't pause in state=[%d]", handle->state);
        OS_THREAD_MUTEX_UNLOCK(handle->io_lock);
        return ESP_FAIL;
    }

    int ret = audio_pipeline_pause(handle->pipeline);

    {
        OS_THREAD_MUTEX_LOCK(handle->state_lock);
        handle->state = (ret == ESP_OK) ? LITEPLAYER_PAUSED : LITEPLAYER_ERROR;
        media_player_state_callback(handle, handle->state, ret);
        OS_THREAD_MUTEX_UNLOCK(handle->state_lock);
    }

    OS_THREAD_MUTEX_UNLOCK(handle->io_lock);
    return ret;
}

int liteplayer_resume(liteplayer_handle_t handle)
{
    if (handle == NULL)
        return ESP_FAIL;

    OS_LOGI(TAG, "Resuming player[%s]", media_source_tag(handle->source_type));

    OS_THREAD_MUTEX_LOCK(handle->io_lock);

    if (handle->state != LITEPLAYER_PAUSED && handle->state != LITEPLAYER_SEEKCOMPLETED) {
        OS_LOGE(TAG, "Can't resume in state=[%d]", handle->state);
        OS_THREAD_MUTEX_UNLOCK(handle->io_lock);
        return ESP_FAIL;
    }

    int ret = audio_pipeline_resume(handle->pipeline, 0, 0);

    {
        OS_THREAD_MUTEX_LOCK(handle->state_lock);
        handle->state = (ret == ESP_OK) ? LITEPLAYER_STARTED : LITEPLAYER_ERROR;
        media_player_state_callback(handle, handle->state, ret);
        OS_THREAD_MUTEX_UNLOCK(handle->state_lock);
    }

    OS_THREAD_MUTEX_UNLOCK(handle->io_lock);
    return ret;
}

int liteplayer_seek(liteplayer_handle_t handle, int msec)
{
    if (handle == NULL || msec < 0)
        return ESP_FAIL;

    OS_LOGI(TAG, "Seeking player[%s], offset=%d(s)", media_source_tag(handle->source_type), (int)(msec/1000));

    int ret = ESP_FAIL;
    bool state_sync = false;

    OS_THREAD_MUTEX_LOCK(handle->io_lock);

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

    if (handle->media_parser != NULL) {
        media_parser_stop(handle->media_parser);
        handle->media_parser = NULL;
    }

    if (handle->pipeline == NULL) {
        ret = main_pipeline_init(handle);
        if (ret != ESP_OK)
            goto seek_out;
    }
    else {
        ret = audio_pipeline_pause(handle->pipeline);
        if (ret != ESP_OK)
            goto seek_out;

        if (handle->media_source != NULL) {
            media_source_stop(handle->media_source);
            handle->media_source = NULL;
        }
        if (handle->source_rb != NULL) {
            rb_destroy(handle->source_rb);
            handle->source_rb = NULL;
        }

        if (handle->source_type == MEDIA_SOURCE_STREAM)
            handle->source_rb = rb_create(DEFAULT_MEDIA_SOURCE_STREAM_RINGBUF_SIZE);
        else if (handle->source_type == MEDIA_SOURCE_HTTP)
            handle->source_rb = rb_create(DEFAULT_MEDIA_SOURCE_HTTP_RINGBUF_SIZE);
        else if (handle->source_type == MEDIA_SOURCE_FILE)
            handle->source_rb = rb_create(DEFAULT_MEDIA_SOURCE_FILE_RINGBUF_SIZE);
        AUDIO_MEM_CHECK(TAG, handle->source_rb, goto seek_out);

        audio_element_set_input_ringbuf(handle->el_decoder, handle->source_rb);
        audio_element_reset_input_ringbuf(handle->el_sink);

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
            handle->media_source = media_source_start(&info, handle->source_rb, media_source_state_callback, handle);
            AUDIO_MEM_CHECK(TAG, handle->media_source, goto seek_out);
        }
        else if (handle->source_type == MEDIA_SOURCE_FILE) {
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
            handle->media_source = media_source_start(&info, handle->source_rb, media_source_state_callback, handle);
            AUDIO_MEM_CHECK(TAG, handle->media_source, goto seek_out);
        }
    }

    ret = audio_pipeline_seek(handle->pipeline, handle->seek_offset);
    if (ret != ESP_OK)
        goto seek_out;

seek_out:
    if (state_sync) {
        OS_THREAD_MUTEX_LOCK(handle->state_lock);
        handle->state = (ret == ESP_OK) ? LITEPLAYER_SEEKCOMPLETED : LITEPLAYER_ERROR;
        media_player_state_callback(handle, handle->state, ret);
        OS_THREAD_MUTEX_UNLOCK(handle->state_lock);
    }

    OS_THREAD_MUTEX_UNLOCK(handle->io_lock);
    return ret;
}

int liteplayer_stop(liteplayer_handle_t handle)
{
    if (handle == NULL)
        return ESP_FAIL;

    int ret = ESP_OK;

    OS_LOGI(TAG, "Stopping player[%s]", media_source_tag(handle->source_type));

    OS_THREAD_MUTEX_LOCK(handle->io_lock);

    if (handle->state == LITEPLAYER_ERROR)
        goto stop_out;

    if (handle->state < LITEPLAYER_PREPARED || handle->state > LITEPLAYER_COMPLETED) {
        OS_LOGE(TAG, "Can't stop in state=[%d]", handle->state);
        OS_THREAD_MUTEX_UNLOCK(handle->io_lock);
        return ESP_FAIL;
    }

    ret = audio_pipeline_stop(handle->pipeline);
    ret |= audio_pipeline_wait_for_stop(handle->pipeline);
    audio_element_reset_state(handle->el_decoder);
    audio_element_reset_state(handle->el_sink);
    audio_pipeline_reset_ringbuffer(handle->pipeline);
    audio_pipeline_reset_items_state(handle->pipeline);

stop_out:
    {
        OS_THREAD_MUTEX_LOCK(handle->state_lock);
        handle->state = LITEPLAYER_STOPPED;
        media_player_state_callback(handle, handle->state, ret);
        OS_THREAD_MUTEX_UNLOCK(handle->state_lock);
    }

    OS_THREAD_MUTEX_UNLOCK(handle->io_lock);
    return ret;
}

int liteplayer_reset(liteplayer_handle_t handle)
{
    if (handle == NULL)
        return ESP_FAIL;

    OS_LOGI(TAG, "Resetting player[%s]", media_source_tag(handle->source_type));

    OS_THREAD_MUTEX_LOCK(handle->io_lock);

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
    }
    else if (handle->codec_info.codec_type == AUDIO_CODEC_WAV) {
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
        OS_THREAD_MUTEX_LOCK(handle->state_lock);

        if (handle->state != LITEPLAYER_STOPPED) {
            handle->state = LITEPLAYER_STOPPED;
            media_player_state_callback(handle, LITEPLAYER_STOPPED, 0);
        }

        handle->state = LITEPLAYER_IDLE;
        media_player_state_callback(handle, LITEPLAYER_IDLE, 0);

        OS_THREAD_MUTEX_UNLOCK(handle->state_lock);
    }

    OS_THREAD_MUTEX_UNLOCK(handle->io_lock);
    return ESP_OK;
}

int liteplayer_get_available_size(liteplayer_handle_t handle)
{
    if (handle == NULL || handle->el_decoder == NULL)
        return 0;

    ringbuf_handle_t source_rb = audio_element_get_input_ringbuf(handle->el_decoder);
    if (source_rb != NULL)
        return rb_bytes_available(source_rb);
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

    if (samplerate == 0 || channels == 0 || bits == 0 || position == 0) {
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

    OS_THREAD_MUTEX_DESTROY(handle->state_lock);
    OS_THREAD_MUTEX_DESTROY(handle->io_lock);
    audio_free(handle);
}