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
#include "audio_stream/http_stream.h"
#include "audio_stream/fatfs_stream.h"
#include "audio_stream/sink_stream.h"
#include "audio_extractor/mp3_extractor.h"
#include "audio_extractor/aac_extractor.h"
#include "audio_extractor/m4a_extractor.h"
#include "audio_extractor/wav_extractor.h"

#include "liteplayer_config.h"
#include "liteplayer_parser.h"

#define TAG "liteplayerparser"

typedef struct media_parser_priv {
    media_source_info_t source_info;
    media_codec_info_t media_info;

    http_handle_t http_handle;
    fatfs_handle_t fatfs_handle;
    long offset;

    media_parser_state_cb listener;
    void *listener_priv;

    bool stop;
    os_mutex_t lock; // lock for listener
    os_cond_t cond;  // wait stop to exit mediaparser thread
} media_parser_priv_t;

static int media_parser_fetch(char *buf, int wanted_size, long offset, void *priv)
{
    media_parser_priv_t *parser = (media_parser_priv_t *)priv;
    int bytes_read = ESP_FAIL;

    if (parser->source_info.source_type == MEDIA_SOURCE_FILE) {
        if (parser->offset != offset) {
            ESP_LOGD(TAG, "fatfs seek position: %ld>>%ld", parser->offset, offset);
            if (parser->source_info.fatfs_wrapper.seek(parser->fatfs_handle, offset) != 0)
                return ESP_FAIL;
            parser->offset = offset;
        }
        bytes_read = parser->source_info.fatfs_wrapper.read(parser->fatfs_handle, buf, wanted_size);
    }
    else if (parser->source_info.source_type == MEDIA_SOURCE_HTTP) {
        if (parser->offset != offset) {
            ESP_LOGD(TAG, "http seek position: %ld>>%ld", parser->offset, offset);
            if (parser->source_info.http_wrapper.seek(parser->http_handle, offset) != 0)
                return ESP_FAIL;
            parser->offset = offset;
        }
        bytes_read = parser->source_info.http_wrapper.read(parser->http_handle, buf, wanted_size);
    }

    if (bytes_read > 0)
        parser->offset += bytes_read;
    return bytes_read;
}

static int m4a_header_parse(media_source_info_t *source_info, media_codec_info_t *media_info)
{
    if (source_info->source_type != MEDIA_SOURCE_FILE &&
        source_info->source_type != MEDIA_SOURCE_HTTP)
        return ESP_FAIL;

    media_parser_priv_t m4a_priv = {
        .offset = 0,
        .fatfs_handle = NULL,
        .http_handle = NULL,
    };
    memcpy(&m4a_priv.source_info, source_info, sizeof(media_source_info_t));
    int ret = ESP_FAIL;

    if (source_info->source_type == MEDIA_SOURCE_FILE) {
        m4a_priv.fatfs_handle = source_info->fatfs_wrapper.open(source_info->url,
                                                                FATFS_READ,
                                                                0,
                                                                source_info->fatfs_wrapper.fatfs_priv);
        if (m4a_priv.fatfs_handle == NULL)
            return ESP_FAIL;
    }
    else if (source_info->source_type == MEDIA_SOURCE_HTTP) {
        m4a_priv.http_handle = source_info->http_wrapper.open(source_info->url,
                                                              0,
                                                              source_info->http_wrapper.http_priv);
        if (m4a_priv.http_handle == NULL)
            return ESP_FAIL;
    }

    if (m4a_extractor(media_parser_fetch, &m4a_priv, &media_info->m4a_info) == 0) {
        media_info->content_pos = media_info->m4a_info.mdatofs;
        media_info->codec_samplerate = media_info->m4a_info.samplerate;
        media_info->codec_channels = media_info->m4a_info.channels;
        media_info->duration_ms = (long long)media_info->m4a_info.duration*1000/media_info->m4a_info.timescale;
        ret = ESP_OK;
    }

    if (source_info->source_type == MEDIA_SOURCE_FILE)
        source_info->fatfs_wrapper.close(m4a_priv.fatfs_handle);
    else if (source_info->source_type == MEDIA_SOURCE_HTTP)
        source_info->http_wrapper.close(m4a_priv.http_handle);

    return ret;
}

static int get_start_offset(char *buf)
{
    int frame_start_offset = 0;
    if (strncmp((const char *)buf, "ID3", 3) == 0) {
        int id3v2_len =
                ((((int)(buf[6])) & 0x7F) << 21) +
                ((((int)(buf[7])) & 0x7F) << 14) +
                ((((int)(buf[8])) & 0x7F) <<  7) +
                 (((int)(buf[9])) & 0x7F);

        frame_start_offset = id3v2_len + 10;
        ESP_LOGV(TAG, "ID3 tag find with length[%d]", id3v2_len);
    }
    return frame_start_offset;
}

static int media_header_parse(media_source_info_t *source_info, media_codec_info_t *media_info)
{
    if (source_info->source_type != MEDIA_SOURCE_FILE &&
        source_info->source_type != MEDIA_SOURCE_HTTP)
        return ESP_FAIL;

    http_handle_t client = NULL;
    fatfs_handle_t file = NULL;
    long long filesize = 0;
    int bytes_read = 0;
    char *buf = audio_calloc(1, DEFAULT_MEDIA_PARSER_BUFFER_SIZE);
    int ret = ESP_FAIL;

    if (buf == NULL)
        goto parse_done;

    if (source_info->source_type == MEDIA_SOURCE_HTTP) {
        client = source_info->http_wrapper.open(source_info->url,
                                                0,
                                                source_info->http_wrapper.http_priv);
        if (client == NULL) {
            ESP_LOGE(TAG, "Failed to connect http, url=%s", source_info->url);
            goto parse_done;
        }
        bytes_read = source_info->http_wrapper.read(client, buf, DEFAULT_MEDIA_PARSER_BUFFER_SIZE);
        if (bytes_read <= 0) {
            ESP_LOGE(TAG, "Failed to read http");
            goto parse_done;
        }
        filesize = source_info->http_wrapper.filesize(client);
    }
    else if (source_info->source_type == MEDIA_SOURCE_FILE) {
        file = source_info->fatfs_wrapper.open(source_info->url,
                                               FATFS_READ,
                                               0,
                                               source_info->fatfs_wrapper.fatfs_priv);
        if (file == NULL) {
            ESP_LOGE(TAG, "Failed to open file, url=%s", source_info->url);
            goto parse_done;
        }
        bytes_read = source_info->fatfs_wrapper.read(file, buf, DEFAULT_MEDIA_PARSER_BUFFER_SIZE);
        if (bytes_read <= 0) {
            ESP_LOGE(TAG, "Failed to read file");
            goto parse_done;
        }
        filesize = source_info->fatfs_wrapper.filesize(file);
    }

    if (bytes_read < 64) {
        ESP_LOGE(TAG, "Insufficient bytes read: %d", bytes_read);
        goto parse_done;
    }

    if (memcmp(&buf[4], "ftyp", 4) == 0) {
        ESP_LOGV(TAG, "Found M4A media");
        media_info->codec_type = AUDIO_CODEC_M4A;
    }
    else if (memcmp(&buf[0], "ID3", 3) == 0) {
        if (media_info->codec_type == AUDIO_CODEC_MP3) {
            ESP_LOGV(TAG, "Found MP3 media with ID3 tag");
        }
        else if (media_info->codec_type == AUDIO_CODEC_AAC) {
            ESP_LOGV(TAG, "Found AAC media with ID3 tag");
        }
    }
    else if ((buf[0] & 0xFF) == 0xFF && (buf[1] & 0xE0) == 0xE0) {
        if (media_info->codec_type == AUDIO_CODEC_AAC &&
            (buf[0] & 0xFF) == 0xFF && (buf[1] & 0xF0) == 0xF0) {
            ESP_LOGV(TAG, "Found AAC media raw data");
            media_info->codec_type = AUDIO_CODEC_AAC;
        }
        else {
            ESP_LOGV(TAG, "Found MP3 media raw data");
            media_info->codec_type = AUDIO_CODEC_MP3;
        }
    }
    else if (memcmp(&buf[0], "RIFF", 4) == 0) {
        ESP_LOGV(TAG, "Found wav media");
        media_info->codec_type = AUDIO_CODEC_WAV;
    }

    int frame_start_offset = 0;
    int sample_rate = 0, channels = 0;
    long long duration_ms = 0;

    if (media_info->codec_type == AUDIO_CODEC_MP3) {
        frame_start_offset = get_start_offset(buf);
        mp3_info_t info = {0};
        int remain_size = bytes_read - frame_start_offset;

        if (remain_size >= 4) {
            if (mp3_parse_header(&buf[frame_start_offset], remain_size, &info) != 0)
                goto parse_done;
        }
        else {
            bytes_read = 0;
            if (source_info->source_type == MEDIA_SOURCE_HTTP) {
                if (source_info->http_wrapper.seek(client, frame_start_offset) != 0)
                    goto parse_done;
                bytes_read = source_info->http_wrapper.read(client,
                                                            buf,
                                                            DEFAULT_MEDIA_PARSER_BUFFER_SIZE);
            }
            else if (source_info->source_type == MEDIA_SOURCE_FILE) {
                if (source_info->fatfs_wrapper.seek(file, frame_start_offset) != 0)
                    goto parse_done;
                bytes_read = source_info->fatfs_wrapper.read(file,
                                                             buf,
                                                             DEFAULT_MEDIA_PARSER_BUFFER_SIZE);
            }
            if (bytes_read < 4)
                goto parse_done;
            if (mp3_parse_header(buf, bytes_read, &info) != 0)
                goto parse_done;
        }

        sample_rate = info.sample_rate;
        channels = info.channels;
        if (filesize > frame_start_offset)
            duration_ms = (filesize - frame_start_offset)*8/info.bit_rate;
    }
    else if (media_info->codec_type == AUDIO_CODEC_AAC) {
        frame_start_offset = get_start_offset(buf);
        aac_info_t info = {0};
        int remain_size = bytes_read - frame_start_offset;

        if (remain_size >= 9) {
            if (aac_parse_adts_frame(&buf[frame_start_offset], remain_size, &info) != 0)
                goto parse_done;
        }
        else {
            bytes_read = 0;
            if (source_info->source_type == MEDIA_SOURCE_HTTP) {
                if (source_info->http_wrapper.seek(client, frame_start_offset) != 0)
                    goto parse_done;
                bytes_read = source_info->http_wrapper.read(client,
                                                            buf,
                                                            DEFAULT_MEDIA_PARSER_BUFFER_SIZE);
            }
            else if (source_info->source_type == MEDIA_SOURCE_FILE) {
                if (source_info->fatfs_wrapper.seek(file, frame_start_offset) != 0)
                    goto parse_done;
                bytes_read = source_info->fatfs_wrapper.read(file,
                                                             buf,
                                                             DEFAULT_MEDIA_PARSER_BUFFER_SIZE);
            }
            if (bytes_read < 9)
                goto parse_done;
            if (aac_parse_adts_frame(buf, bytes_read, &info) != 0)
                goto parse_done;
        }

        sample_rate = info.sample_rate;
        channels = info.channels;
        //if (filesize > frame_start_offset)
        //    duration_ms = (filesize - frame_start_offset)*8/info.bit_rate;
    }
    else if (media_info->codec_type == AUDIO_CODEC_WAV) {
        wav_info_t info = {0};
        if (wav_parse_header(buf, bytes_read, &info) != 0)
            goto parse_done;

        sample_rate = info.sampleRate;
        channels = info.channels;
        duration_ms = (long long)info.dataSize*1000/info.blockAlign/info.sampleRate;
    }
    else {
        goto parse_done;
    }

    media_info->content_pos = frame_start_offset;
    media_info->codec_samplerate = sample_rate;
    media_info->codec_channels = channels;
    media_info->duration_ms = duration_ms;
    ret = ESP_OK;

parse_done:
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse media info");
    }
    if (source_info->source_type == MEDIA_SOURCE_HTTP && client != NULL)
        source_info->http_wrapper.close(client);
    if (source_info->source_type == MEDIA_SOURCE_FILE && file != NULL)
        source_info->fatfs_wrapper.close(file);
    if (buf != NULL)
        audio_free(buf);
    return ret;
}

static void media_parser_cleanup(media_parser_priv_t *priv)
{
    if (priv->lock != NULL)
        OS_THREAD_MUTEX_DESTROY(priv->lock);
    if (priv->cond != NULL)
        OS_THREAD_COND_DESTROY(priv->cond);
    if (priv->source_info.url != NULL)
        audio_free(priv->source_info.url);
    audio_free(priv);
}

static void *media_parser_thread(void *arg)
{
    media_parser_priv_t *priv = (media_parser_priv_t *)arg;
    int ret = media_info_parse(&priv->source_info, &priv->media_info);

    OS_THREAD_MUTEX_LOCK(priv->lock);
    if (!priv->stop && priv->listener) {
        if (ret == ESP_OK)
            priv->listener(MEDIA_PARSER_SUCCEED, &priv->media_info, priv->listener_priv);
        else
            priv->listener(MEDIA_PARSER_FAILED, &priv->media_info, priv->listener_priv);
    }
    OS_THREAD_MUTEX_UNLOCK(priv->lock);

    while (!priv->stop)
        OS_THREAD_COND_WAIT(priv->cond, priv->lock);
    media_parser_cleanup(priv);

    ESP_LOGD(TAG, "Media parser task leave");
    return NULL;
}

int media_info_parse(media_source_info_t *source_info, media_codec_info_t *media_info)
{
    if (source_info == NULL || source_info->url == NULL || media_info == NULL)
        return ESP_FAIL;

    if (strstr(source_info->url, ".m3u") != NULL) {
        char temp[256];
        int ret = m3u_get_first_url(source_info, temp, sizeof(temp));
        if (ret == 0) {
            const char *media_url = audio_strdup(&temp[0]);
            if (media_url != NULL) {
                audio_free(source_info->url);
                source_info->url = media_url;
                ESP_LOGV(TAG, "M3U first url: %s", media_url);
            }
        }
    }

    if (source_info->source_type == MEDIA_SOURCE_HTTP) {
        if (strstr(source_info->url, "m4a") != NULL) {
            media_info->codec_type = AUDIO_CODEC_M4A;
            if (m4a_header_parse(source_info, media_info) == ESP_OK)
                goto parse_succeed;
            // if failed, go ahead to check real codec type
            media_info->codec_type = AUDIO_CODEC_NONE;
        }
        else if (strstr(source_info->url, "mp3") != NULL)
            media_info->codec_type = AUDIO_CODEC_MP3;
        else if (strstr(source_info->url, "wav") != NULL)
            media_info->codec_type = AUDIO_CODEC_WAV;
        else if (strstr(source_info->url, "aac") != NULL)
            media_info->codec_type = AUDIO_CODEC_AAC;
        if (media_header_parse(source_info, media_info) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to parse http url:[%s]", source_info->url);
            return ESP_FAIL;
        }
    }
    else {
        if (strstr(source_info->url, "m4a") != NULL) {
            media_info->codec_type = AUDIO_CODEC_M4A;
            if (m4a_header_parse(source_info, media_info) == ESP_OK)
                goto parse_succeed;
            // if failed, go ahead to check real codec type
            media_info->codec_type = AUDIO_CODEC_NONE;
        }
        else if (strstr(source_info->url, "mp3") != NULL)
            media_info->codec_type = AUDIO_CODEC_MP3;
        else if (strstr(source_info->url, "wav") != NULL)
            media_info->codec_type = AUDIO_CODEC_WAV;
        else if (strstr(source_info->url, "aac") != NULL)
            media_info->codec_type = AUDIO_CODEC_AAC;
        if (media_header_parse(source_info, media_info) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to parse file url:[%s]", source_info->url);
            return ESP_FAIL;
        }
    }

parse_succeed:
    ESP_LOGI(TAG, "MediaInfo: codec_type[%d], samplerate[%d], channels[%d], offset[%d], length[%d]",
             source_info->source_type, media_info->codec_samplerate, media_info->codec_channels,
             media_info->content_pos, media_info->content_len);
    return ESP_OK;
}

media_parser_handle_t media_parser_start_async(media_source_info_t *source_info,
                                               media_parser_state_cb listener,
                                               void *listener_priv)
{
    media_parser_priv_t *priv = audio_calloc(1, sizeof(media_parser_priv_t));
    if (priv == NULL || source_info == NULL || source_info->url == NULL)
        goto start_failed;

    memcpy(&priv->source_info, source_info, sizeof(media_source_info_t));
    priv->listener = listener;
    priv->listener_priv = listener_priv;
    priv->lock = OS_THREAD_MUTEX_CREATE();
    priv->cond = OS_THREAD_COND_CREATE();
    priv->source_info.url = audio_strdup(source_info->url);
    if (priv->lock == NULL || priv->cond == NULL || priv->source_info.url == NULL)
        goto start_failed;

    struct os_threadattr attr = {
        .name = "ael_parser",
        .priority = DEFAULT_MEDIA_PARSER_TASK_PRIO,
        .stacksize = DEFAULT_MEDIA_PARSER_TASK_STACKSIZE,
        .joinable = false,
    };
    os_thread_t id = OS_THREAD_CREATE(&attr, media_parser_thread, priv);
    if (id == NULL)
        goto start_failed;

    return priv;

start_failed:
    if (priv != NULL)
        media_parser_cleanup(priv);
    return NULL;
}

void media_parser_stop(media_parser_handle_t handle)
{
    media_parser_priv_t *priv = (media_parser_priv_t *)handle;
    if (priv == NULL)
        return;

    OS_THREAD_MUTEX_LOCK(priv->lock);
    priv->stop = true;
    OS_THREAD_COND_SIGNAL(priv->cond);
    OS_THREAD_MUTEX_UNLOCK(priv->lock);
}
