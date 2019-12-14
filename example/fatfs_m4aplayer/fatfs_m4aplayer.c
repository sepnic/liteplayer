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
#include <stdbool.h>
#include <string.h>

#include "msgutils/os_thread.h"
#include "msgutils/os_memory.h"
#include "esp_adf/esp_log.h"
#include "esp_adf/esp_err.h"
#include "esp_adf/audio_element.h"
#include "esp_adf/audio_pipeline.h"
#include "esp_adf/audio_event_iface.h"
#include "esp_adf/audio_common.h"
#include "audio_decoder/m4a_decoder.h"
#include "audio_extractor/m4a_extractor.h"
#include "audio_stream/fatfs_stream.h"

#define TAG "M4APLAYER"

#define DEFAULT_IN_AAC   "in.m4a"
#define DEFAULT_OUT_WAV  "out.wav"

typedef enum {
    PLAYER_EVENT_NONE = 0,
    PLAYER_EVENT_STARTED,
    PLAYER_EVENT_PAUSED,
    PLAYER_EVENT_FINISHED,
    PLAYER_EVENT_STOPPED,
    PLAYER_EVENT_ERROR,
} player_event_t;

typedef struct m4a_player* m4a_player_handle_t;
typedef esp_err_t (*player_event)(m4a_player_handle_t ap, player_event_t event);

typedef struct {
    player_event event_handler;
} m4a_player_config_t;

#define DEFAULT_M4A_PLAYER_CONFIG() { \
    .event_handler = NULL,            \
}

struct m4a_player {
    audio_pipeline_handle_t pipeline;
    audio_element_handle_t fatfs_stream_reader;
    audio_element_handle_t fatfs_stream_writer;
    audio_element_handle_t aac_decoder;
    player_event event_handler;
    m4a_info_t *m4a_info;
};

static bool g_player_stopped = false;

static esp_err_t audio_element_state_cb(audio_element_handle_t el, audio_event_iface_msg_t *msg, void *ctx)
{
    m4a_player_handle_t ap = (m4a_player_handle_t) ctx;

    if (msg->source_type == AUDIO_ELEMENT_TYPE_ELEMENT) {
        audio_element_status_t el_status = (audio_element_status_t)msg->data;

        if (msg->cmd == AEL_MSG_CMD_REPORT_STATUS) {
            switch(el_status) {
            case AEL_STATUS_ERROR_OPEN:
            case AEL_STATUS_ERROR_INPUT:
            case AEL_STATUS_ERROR_PROCESS:
            case AEL_STATUS_ERROR_OUTPUT:
            case AEL_STATUS_ERROR_CLOSE:
            case AEL_STATUS_ERROR_TIMEOUT:
            case AEL_STATUS_ERROR_UNKNOWN:
                ESP_LOGE(TAG, "[ * ] AEL error[%d]", el, el_status);
                if (ap->event_handler)
                    ap->event_handler(ap, PLAYER_EVENT_ERROR);
                break;

            case AEL_STATUS_STATE_RUNNING:
                if (msg->source == (void *) ap->aac_decoder) {
                    ESP_LOGI(TAG, "[ * ] Receive started event");
                    if (ap->event_handler)
                        ap->event_handler(ap, PLAYER_EVENT_STARTED);
                }
                break;

            case AEL_STATUS_STATE_PAUSED:
                if (msg->source == (void *) ap->aac_decoder) {
                    ESP_LOGI(TAG, "[ * ] Receive paused event");
                    if (ap->event_handler)
                        ap->event_handler(ap, PLAYER_EVENT_PAUSED);
                }
                break;

            case AEL_STATUS_STATE_FINISHED:
                if (msg->source == (void *)ap->fatfs_stream_writer) {
                    ESP_LOGI(TAG, "[ * ] Receive finished event");
                    if (ap->event_handler)
                        ap->event_handler(ap, PLAYER_EVENT_FINISHED);
                }
                break;

            case AEL_STATUS_STATE_STOPPED:
                if (msg->source == (void *)ap->fatfs_stream_writer) {
                    ESP_LOGI(TAG, "[ * ] Receive stopped event");
                    if (ap->event_handler)
                        ap->event_handler(ap, PLAYER_EVENT_STOPPED);
                }
                break;

            default:
                break;
            }
        }
        else if (msg->cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            if (msg->source == (void *) ap->aac_decoder) {
                audio_element_info_t info = {0};
                audio_element_getinfo(ap->aac_decoder, &info);
                ESP_LOGI(TAG, "[ * ] Receive music info from aac decoder, sample_rates=%d, bits=%d, ch=%d",
                         info.out_samplerate, info.bits, info.out_channels);
            }
        }
    }

    return ESP_OK;
}

typedef struct m4a_fetch_priv {
    FILE *file;
    long offset;
} m4a_fetch_priv_t;

static int m4a_fetch_data(char *buf, int wanted_size, long offset, void *priv)
{
    m4a_fetch_priv_t *data = (m4a_fetch_priv_t *)priv;
    int bytes_read = 0;

    if (data->offset != offset) {
        ESP_LOGD(TAG, "Seek position: %ld>>%ld", data->offset, offset);
        if (fseek(data->file, offset, SEEK_SET) != 0)
            return -1;
        data->offset = offset;
    }

    bytes_read = fread((void *)buf, 1, wanted_size, data->file);

    data->offset += bytes_read;
    return bytes_read;
}

static m4a_info_t *m4a_player_parse(const char *url)
{
    m4a_fetch_priv_t priv = {
        .file = NULL,
        .offset = 0,
    };
    m4a_info_t *m4a_info = audio_calloc(1, sizeof(m4a_info_t));

    if (m4a_info != NULL) {
        priv.file = fopen(url, "rb");
        if (priv.file == NULL)
            goto parse_failed;

        int ret = m4a_extractor(m4a_fetch_data, &priv, m4a_info);
        if (ret != 0)
            goto parse_failed;

        fclose(priv.file);
    }
    return m4a_info;

parse_failed:
    ESP_LOGE(TAG, "Failed to parse m4a header");
    if (priv.file != NULL)
        fclose(priv.file);
    audio_free(m4a_info);
    return NULL;
}

static void m4a_player_deinit(m4a_player_handle_t ap)
{
    if (ap == NULL)
        return;

    // Release all resources
    audio_pipeline_deinit(ap->pipeline);
    audio_free(ap->m4a_info);
    audio_free(ap);
}

static m4a_player_handle_t m4a_player_init(m4a_player_config_t *config, m4a_info_t *m4a_info)
{
    m4a_player_handle_t ap = audio_calloc(1, sizeof(struct m4a_player));
    AUDIO_MEM_CHECK(TAG, ap, return NULL);

    ESP_LOGI(TAG, "[1.0] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    ap->pipeline = audio_pipeline_init(&pipeline_cfg);
    AUDIO_MEM_CHECK(TAG, ap->pipeline, goto audio_init_failed);

    ESP_LOGI(TAG, "[2.0] Create fatfs stream to read data");
    fatfs_stream_cfg_t fatfs_reader_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_reader_cfg.type = AUDIO_STREAM_READER;
    ap->fatfs_stream_reader = fatfs_stream_init(&fatfs_reader_cfg);
    AUDIO_MEM_CHECK(TAG, ap->fatfs_stream_reader, goto audio_init_failed);

    ESP_LOGI(TAG, "[2.1] Create fatfs stream to write data");
    fatfs_stream_cfg_t fatfs_writer_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_writer_cfg.type = AUDIO_STREAM_WRITER;
    ap->fatfs_stream_writer = fatfs_stream_init(&fatfs_writer_cfg);
    AUDIO_MEM_CHECK(TAG, ap->fatfs_stream_writer, goto audio_init_failed);

    ESP_LOGI(TAG, "[2.2] Create m4a decoder to decode m4a file");
    m4a_decoder_cfg_t aac_cfg = DEFAULT_M4A_DECODER_CONFIG();
    aac_cfg.m4a_info = m4a_info;
    ap->aac_decoder = m4a_decoder_init(&aac_cfg);
    AUDIO_MEM_CHECK(TAG, ap->aac_decoder, goto audio_init_failed);

    ESP_LOGI(TAG, "[2.3] Register all elements to audio pipeline");
    audio_pipeline_register(ap->pipeline, ap->fatfs_stream_reader, "fatfs_r");
    audio_pipeline_register(ap->pipeline, ap->aac_decoder,         "m4a_dec");
    audio_pipeline_register(ap->pipeline, ap->fatfs_stream_writer, "fatfs_w");

    ESP_LOGI(TAG, "[2.4] Link elements together fatfs_stream_r->m4a_decoder->fatfs_stream_w");
    audio_pipeline_link(ap->pipeline, (const char *[]) {"fatfs_r", "m4a_dec", "fatfs_w"}, 3);

    ESP_LOGI(TAG, "[3.0] Register event callback of all elements");
    audio_element_set_event_callback(ap->fatfs_stream_reader, audio_element_state_cb, ap);
    audio_element_set_event_callback(ap->aac_decoder, audio_element_state_cb, ap);
    audio_element_set_event_callback(ap->fatfs_stream_writer, audio_element_state_cb, ap);

    ap->event_handler = config->event_handler;
    ap->m4a_info = m4a_info;
    return ap;

audio_init_failed:
    m4a_player_deinit(ap);
    return NULL;
}

static esp_err_t m4a_player_start(m4a_player_handle_t ap, const char *in_aac, const char *out_wav)
{
    if (in_aac != NULL && out_wav != NULL) {
        {
            audio_element_info_t write_info;
            audio_element_getinfo(ap->fatfs_stream_writer, &write_info);
            write_info.in_channels = ap->m4a_info->channels;
            write_info.in_samplerate = ap->m4a_info->samplerate;
            write_info.bits = ap->m4a_info->bits;
            audio_element_setinfo(ap->fatfs_stream_writer, &write_info);

            audio_element_info_t reader_info;
            audio_element_getinfo(ap->fatfs_stream_reader, &reader_info);
            reader_info.byte_pos = ap->m4a_info->mdatofs;
            audio_element_setinfo(ap->fatfs_stream_reader, &reader_info);
        }

        audio_element_set_uri(ap->fatfs_stream_reader, in_aac);
        audio_element_set_uri(ap->fatfs_stream_writer, out_wav);
        return audio_pipeline_run(ap->pipeline, 0.0, 0);
    }
    return ESP_FAIL;
}

/*static esp_err_t m4a_player_pause(m4a_player_handle_t ap)
{
    return audio_pipeline_pause(ap->pipeline);
}

static esp_err_t m4a_player_resume(m4a_player_handle_t ap)
{
    return audio_pipeline_resume(ap->pipeline, 0.0, 0);
}*/

static esp_err_t m4a_player_stop(m4a_player_handle_t ap)
{
    esp_err_t ret = ESP_OK;
    ret |= audio_pipeline_stop(ap->pipeline);
    ret |= audio_pipeline_wait_for_stop(ap->pipeline);
    audio_element_reset_state(ap->aac_decoder);
    audio_element_reset_state(ap->fatfs_stream_writer);
    audio_pipeline_reset_ringbuffer(ap->pipeline);
    audio_pipeline_reset_items_state(ap->pipeline);
    return ret;
}

static esp_err_t player_event_listerner(m4a_player_handle_t ap, player_event_t event)
{
    switch (event) {
    case PLAYER_EVENT_STARTED:
        ESP_LOGI(TAG, "-->PLAYER_EVENT_STARTED");
        break;

    case PLAYER_EVENT_PAUSED:
        ESP_LOGI(TAG, "-->PLAYER_EVENT_PAUSED");
        break;

    case PLAYER_EVENT_FINISHED:
        ESP_LOGI(TAG, "-->PLAYER_EVENT_FINISHED");
        m4a_player_stop(ap);
        break;

    case PLAYER_EVENT_STOPPED:
    case PLAYER_EVENT_ERROR:
    default:
        ESP_LOGI(TAG, "-->PLAYER_EVENT_STOPPED");
        g_player_stopped = true;
        break;
    }

    return ESP_OK;
}

int main(int argc, char *argv[])
{
    esp_err_t ret = ESP_OK;
    const char *in_aac  = DEFAULT_IN_AAC;
    const char *out_wav = DEFAULT_OUT_WAV;

    if (argc == 2) {
        in_aac = argv[1];
    }
    else if (argc == 3) {
        in_aac = argv[1];
        out_wav = argv[2];
    }

    m4a_info_t *m4a_info = m4a_player_parse(in_aac);
    if (m4a_info == NULL) {
        ESP_LOGE(TAG, "Failed to parse m4a");
        OS_MEMORY_DUMP();
        return -1;
    }

    m4a_player_config_t config = DEFAULT_M4A_PLAYER_CONFIG();
    config.event_handler = player_event_listerner;
    m4a_player_handle_t ap = m4a_player_init(&config, m4a_info);
    if (ap == NULL) {
        ESP_LOGE(TAG, "Failed to init audio player");
        OS_MEMORY_DUMP();
        return -1;
    }

    ret = m4a_player_start(ap, in_aac, out_wav);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start audio player");
        m4a_player_deinit(ap);
        OS_MEMORY_DUMP();
        return -1;
    }

    OS_MEMORY_DUMP();

    while (!g_player_stopped)
        OS_THREAD_SLEEP_MSEC(100);

    m4a_player_deinit(ap);

    OS_MEMORY_DUMP();
    return 0;
}
