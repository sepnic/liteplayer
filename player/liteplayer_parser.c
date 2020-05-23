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
#include "audio_stream/http_stream.h"
#include "audio_stream/file_stream.h"
#include "audio_stream/sink_stream.h"
#include "audio_extractor/mp3_extractor.h"
#include "audio_extractor/aac_extractor.h"
#include "audio_extractor/m4a_extractor.h"
#include "audio_extractor/wav_extractor.h"

#include "liteplayer_config.h"
#include "liteplayer_parser.h"

#define TAG "[liteplayer]PARSER"

struct media_parser_priv {
    struct media_source_info source_info;
    struct media_codec_info codec_info;

    http_handle_t http_handle;
    file_handle_t file_handle;
    long offset;

    media_parser_state_cb listener;
    void *listener_priv;

    bool stop;
    os_mutex_t lock; // lock for listener
    os_cond_t cond;  // wait stop to exit mediaparser thread
};

static int media_parser_fetch(char *buf, int wanted_size, long offset, void *priv)
{
    struct media_parser_priv *parser = (struct media_parser_priv *)priv;
    int bytes_read = ESP_FAIL;

    if (parser->source_info.source_type == MEDIA_SOURCE_FILE) {
        if (parser->offset != offset) {
            OS_LOGD(TAG, "file seek position: %ld>>%ld", parser->offset, offset);
            if (parser->source_info.file_ops.seek(parser->file_handle, offset) != 0)
                return ESP_FAIL;
            parser->offset = offset;
        }
        bytes_read = parser->source_info.file_ops.read(parser->file_handle, buf, wanted_size);
    }
    else if (parser->source_info.source_type == MEDIA_SOURCE_HTTP) {
        if (parser->offset != offset) {
            OS_LOGD(TAG, "http seek position: %ld>>%ld", parser->offset, offset);
            if (parser->source_info.http_ops.seek(parser->http_handle, offset) != 0)
                return ESP_FAIL;
            parser->offset = offset;
        }
        bytes_read = parser->source_info.http_ops.read(parser->http_handle, buf, wanted_size);
    }

    if (bytes_read > 0)
        parser->offset += bytes_read;
    return bytes_read;
}

static int m4a_header_parse(struct media_source_info *source_info, struct media_codec_info *codec_info)
{
    if (source_info->source_type != MEDIA_SOURCE_FILE && source_info->source_type != MEDIA_SOURCE_HTTP)
        return ESP_FAIL;

    struct media_parser_priv m4a_priv = {
        .offset = 0,
        .file_handle = NULL,
        .http_handle = NULL,
    };
    memcpy(&m4a_priv.source_info, source_info, sizeof(struct media_source_info));
    int ret = ESP_FAIL;

    if (source_info->source_type == MEDIA_SOURCE_FILE) {
        m4a_priv.file_handle = source_info->file_ops.open(source_info->url,
                                                          FILE_READ,
                                                          0,
                                                          source_info->file_ops.file_priv);
        if (m4a_priv.file_handle == NULL)
            return ESP_FAIL;
    }
    else if (source_info->source_type == MEDIA_SOURCE_HTTP) {
        m4a_priv.http_handle = source_info->http_ops.open(source_info->url,
                                                          0,
                                                          source_info->http_ops.http_priv);
        if (m4a_priv.http_handle == NULL)
            return ESP_FAIL;
    }

    if (m4a_extractor(media_parser_fetch, &m4a_priv, &(codec_info->detail.m4a_info)) == 0) {
        codec_info->content_pos = codec_info->detail.m4a_info.mdat_offset;
    #if defined(AAC_ENABLE_SBR)
        codec_info->codec_samplerate = codec_info->detail.m4a_info.samplerate;
        codec_info->codec_channels = codec_info->detail.m4a_info.channels;
    #else
        codec_info->codec_samplerate = codec_info->detail.m4a_info.asc.samplerate;
        codec_info->codec_channels = codec_info->detail.m4a_info.asc.channels;
    #endif
        codec_info->codec_bits = codec_info->detail.m4a_info.bits;
        codec_info->duration_ms =
            (int)(codec_info->detail.m4a_info.duration/codec_info->detail.m4a_info.time_scale*1000);
        ret = ESP_OK;
    }

    if (source_info->source_type == MEDIA_SOURCE_FILE)
        source_info->file_ops.close(m4a_priv.file_handle);
    else if (source_info->source_type == MEDIA_SOURCE_HTTP)
        source_info->http_ops.close(m4a_priv.http_handle);

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
        OS_LOGV(TAG, "ID3 tag find with length[%d]", id3v2_len);
    }
    return frame_start_offset;
}

static int media_header_parse(struct media_source_info *source_info, struct media_codec_info *codec_info)
{
    if (source_info->source_type != MEDIA_SOURCE_FILE &&
        source_info->source_type != MEDIA_SOURCE_HTTP)
        return ESP_FAIL;

    http_handle_t client = NULL;
    file_handle_t file = NULL;
    long long filesize = 0;
    int bytes_read = 0;
    char *buf = audio_calloc(1, DEFAULT_MEDIA_PARSER_BUFFER_SIZE);
    int ret = ESP_FAIL;

    if (buf == NULL)
        goto parse_done;

    if (source_info->source_type == MEDIA_SOURCE_HTTP) {
        client = source_info->http_ops.open(source_info->url,
                                            0,
                                            source_info->http_ops.http_priv);
        if (client == NULL) {
            OS_LOGE(TAG, "Failed to connect http, url=%s", source_info->url);
            goto parse_done;
        }
        bytes_read = source_info->http_ops.read(client, buf, DEFAULT_MEDIA_PARSER_BUFFER_SIZE);
        if (bytes_read <= 0) {
            OS_LOGE(TAG, "Failed to read http");
            goto parse_done;
        }
        filesize = source_info->http_ops.filesize(client);
    }
    else if (source_info->source_type == MEDIA_SOURCE_FILE) {
        file = source_info->file_ops.open(source_info->url,
                                          FILE_READ,
                                          0,
                                          source_info->file_ops.file_priv);
        if (file == NULL) {
            OS_LOGE(TAG, "Failed to open file, url=%s", source_info->url);
            goto parse_done;
        }
        bytes_read = source_info->file_ops.read(file, buf, DEFAULT_MEDIA_PARSER_BUFFER_SIZE);
        if (bytes_read <= 0) {
            OS_LOGE(TAG, "Failed to read file");
            goto parse_done;
        }
        filesize = source_info->file_ops.filesize(file);
    }

    if (bytes_read < 64) {
        OS_LOGE(TAG, "Insufficient bytes read: %d", bytes_read);
        goto parse_done;
    }

    if (memcmp(&buf[4], "ftyp", 4) == 0) {
        OS_LOGV(TAG, "Found M4A media");
        codec_info->codec_type = AUDIO_CODEC_M4A;
    }
    else if (memcmp(&buf[0], "ID3", 3) == 0) {
        if (codec_info->codec_type == AUDIO_CODEC_MP3) {
            OS_LOGV(TAG, "Found MP3 media with ID3 tag");
        }
        else if (codec_info->codec_type == AUDIO_CODEC_AAC) {
            OS_LOGV(TAG, "Found AAC media with ID3 tag");
        }
        else {
            OS_LOGV(TAG, "Unknown type with ID3, assume codec is MP3");
            codec_info->codec_type = AUDIO_CODEC_MP3;
        }
    }
    else if ((buf[0] & 0xFF) == 0xFF && (buf[1] & 0xE0) == 0xE0) {
        if (codec_info->codec_type == AUDIO_CODEC_AAC &&
            (buf[0] & 0xFF) == 0xFF && (buf[1] & 0xF0) == 0xF0) {
            OS_LOGV(TAG, "Found AAC media raw data");
            codec_info->codec_type = AUDIO_CODEC_AAC;
        }
        else {
            OS_LOGV(TAG, "Found MP3 media raw data");
            codec_info->codec_type = AUDIO_CODEC_MP3;
        }
    }
    else if (memcmp(&buf[0], "RIFF", 4) == 0) {
        OS_LOGV(TAG, "Found wav media");
        codec_info->codec_type = AUDIO_CODEC_WAV;
    }

    int frame_start_offset = 0, bytes_per_sec = 0;
    int sample_rate = 0, channels = 0, bits = 0;
    int duration_ms = 0;

    if (codec_info->codec_type == AUDIO_CODEC_MP3) {
        frame_start_offset = get_start_offset(buf);
        struct mp3_info *info = &(codec_info->detail.mp3_info);
        int remain_size = bytes_read - frame_start_offset;

        if (remain_size >= 4) {
            if (mp3_parse_header(&buf[frame_start_offset], remain_size, info) != 0)
                goto parse_done;
        }
        else {
            bytes_read = 0;
            if (source_info->source_type == MEDIA_SOURCE_HTTP) {
                if (source_info->http_ops.seek(client, frame_start_offset) != 0)
                    goto parse_done;
                bytes_read = source_info->http_ops.read(client,
                                                        buf,
                                                        DEFAULT_MEDIA_PARSER_BUFFER_SIZE);
            }
            else if (source_info->source_type == MEDIA_SOURCE_FILE) {
                if (source_info->file_ops.seek(file, frame_start_offset) != 0)
                    goto parse_done;
                bytes_read = source_info->file_ops.read(file,
                                                        buf,
                                                        DEFAULT_MEDIA_PARSER_BUFFER_SIZE);
            }
            if (bytes_read < 4)
                goto parse_done;
            if (mp3_parse_header(buf, bytes_read, info) != 0)
                goto parse_done;
        }

        sample_rate = info->sample_rate;
        channels = info->channels;
        bits = 16;
        bytes_per_sec = info->bit_rate*1000/8;
        if (filesize > frame_start_offset)
            duration_ms = (filesize - frame_start_offset)*8/info->bit_rate;
    }
    else if (codec_info->codec_type == AUDIO_CODEC_AAC) {
        frame_start_offset = get_start_offset(buf);
        struct aac_info *info = &(codec_info->detail.aac_info);
        int remain_size = bytes_read - frame_start_offset;

        if (remain_size >= 9) {
            if (aac_parse_adts_frame(&buf[frame_start_offset], remain_size, info) != 0)
                goto parse_done;
        }
        else {
            bytes_read = 0;
            if (source_info->source_type == MEDIA_SOURCE_HTTP) {
                if (source_info->http_ops.seek(client, frame_start_offset) != 0)
                    goto parse_done;
                bytes_read = source_info->http_ops.read(client,
                                                        buf,
                                                        DEFAULT_MEDIA_PARSER_BUFFER_SIZE);
            }
            else if (source_info->source_type == MEDIA_SOURCE_FILE) {
                if (source_info->file_ops.seek(file, frame_start_offset) != 0)
                    goto parse_done;
                bytes_read = source_info->file_ops.read(file,
                                                        buf,
                                                        DEFAULT_MEDIA_PARSER_BUFFER_SIZE);
            }
            if (bytes_read < 9)
                goto parse_done;
            if (aac_parse_adts_frame(buf, bytes_read, info) != 0)
                goto parse_done;
        }

        sample_rate = info->sample_rate;
        channels = info->channels;
        bits = 16;
        //bytes_per_sec = info->bit_rate*1000/8;
        //if (filesize > frame_start_offset)
        //    duration_ms = (filesize - frame_start_offset)*8/info->bit_rate;
    }
    else if (codec_info->codec_type == AUDIO_CODEC_WAV) {
        struct wav_info *info = &(codec_info->detail.wav_info);
        if (wav_parse_header(buf, bytes_read, info) != 0)
            goto parse_done;

        frame_start_offset = info->dataOffset;
        sample_rate = info->sampleRate;
        channels = info->channels;
        bits = info->bits;
        bytes_per_sec = info->blockAlign*info->sampleRate;
        duration_ms = (int)(info->dataSize/info->blockAlign/info->sampleRate*1000);
    }
    else if (codec_info->codec_type == AUDIO_CODEC_M4A) {
        OS_LOGD(TAG, "Found M4A media without m4a suffix");
        ret = m4a_header_parse(source_info, codec_info);
        goto parse_done;
    }
    else {
        OS_LOGE(TAG, "Unknown codec type");
        goto parse_done;
    }

    codec_info->content_pos = frame_start_offset;
    codec_info->codec_samplerate = sample_rate;
    codec_info->codec_channels = channels;
    codec_info->codec_bits = bits;
    codec_info->bytes_per_sec = bytes_per_sec;
    codec_info->duration_ms = duration_ms;
    ret = ESP_OK;

parse_done:
    if (source_info->source_type == MEDIA_SOURCE_HTTP && client != NULL)
        source_info->http_ops.close(client);
    if (source_info->source_type == MEDIA_SOURCE_FILE && file != NULL)
        source_info->file_ops.close(file);
    if (buf != NULL)
        audio_free(buf);
    return ret;
}

static void media_parser_cleanup(struct media_parser_priv *priv)
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
    struct media_parser_priv *priv = (struct media_parser_priv *)arg;
    int ret = media_info_parse(&priv->source_info, &priv->codec_info);

    {
        OS_THREAD_MUTEX_LOCK(priv->lock);

        if (!priv->stop && priv->listener) {
            if (ret == ESP_OK)
                priv->listener(MEDIA_PARSER_SUCCEED, &priv->codec_info, priv->listener_priv);
            else
                priv->listener(MEDIA_PARSER_FAILED, &priv->codec_info, priv->listener_priv);
        }

        OS_LOGV(TAG, "Waiting stop command");
        while (!priv->stop)
            OS_THREAD_COND_WAIT(priv->cond, priv->lock);

        OS_THREAD_MUTEX_UNLOCK(priv->lock);
    }

    media_parser_cleanup(priv);
    OS_LOGD(TAG, "Media parser task leave");
    return NULL;
}

int media_info_parse(struct media_source_info *source_info, struct media_codec_info *codec_info)
{
    if (source_info == NULL || source_info->url == NULL || codec_info == NULL)
        return ESP_FAIL;

    if (strstr(source_info->url, ".m3u") != NULL) {
        char temp[256];
        int ret = m3u_get_first_url(source_info, temp, sizeof(temp));
        if (ret == 0) {
            const char *media_url = audio_strdup(&temp[0]);
            if (media_url != NULL) {
                audio_free(source_info->url);
                source_info->url = media_url;
                OS_LOGV(TAG, "M3U first url: %s", media_url);
            }
        }
    }

    if (strstr(source_info->url, "m4a") != NULL) {
        codec_info->codec_type = AUDIO_CODEC_M4A;
        if (m4a_header_parse(source_info, codec_info) == ESP_OK)
            goto parse_succeed;
        // if failed, go ahead to check real codec type
        codec_info->codec_type = AUDIO_CODEC_NONE;
    }
    else if (strstr(source_info->url, "mp3") != NULL)
        codec_info->codec_type = AUDIO_CODEC_MP3;
    else if (strstr(source_info->url, "wav") != NULL)
        codec_info->codec_type = AUDIO_CODEC_WAV;
    else if (strstr(source_info->url, "aac") != NULL)
        codec_info->codec_type = AUDIO_CODEC_AAC;
    if (media_header_parse(source_info, codec_info) != ESP_OK) {
        OS_LOGE(TAG, "Failed to parse file url:[%s]", source_info->url);
        return ESP_FAIL;
    }

parse_succeed:
    OS_LOGI(TAG, "MediaInfo: codec_type[%d], samplerate[%d], channels[%d], bits[%d], offset[%d], length[%d]",
             source_info->source_type,
             codec_info->codec_samplerate, codec_info->codec_channels, codec_info->codec_bits,
             codec_info->content_pos, codec_info->content_len);
    return ESP_OK;
}

media_parser_handle_t media_parser_start_async(struct media_source_info *source_info,
                                               media_parser_state_cb listener,
                                               void *listener_priv)
{
    struct media_parser_priv *priv = audio_calloc(1, sizeof(struct media_parser_priv));
    if (priv == NULL || source_info == NULL || source_info->url == NULL)
        goto start_failed;

    memcpy(&priv->source_info, source_info, sizeof(struct media_source_info));
    priv->listener = listener;
    priv->listener_priv = listener_priv;
    priv->lock = OS_THREAD_MUTEX_CREATE();
    priv->cond = OS_THREAD_COND_CREATE();
    priv->source_info.url = audio_strdup(source_info->url);
    if (priv->lock == NULL || priv->cond == NULL || priv->source_info.url == NULL)
        goto start_failed;

    struct os_threadattr attr = {
        .name = "ael-parser",
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
    struct media_parser_priv *priv = (struct media_parser_priv *)handle;
    if (priv == NULL)
        return;

    {
        OS_THREAD_MUTEX_LOCK(priv->lock);

        priv->stop = true;
        OS_THREAD_COND_SIGNAL(priv->cond);

        OS_THREAD_MUTEX_UNLOCK(priv->lock);
    }
}
