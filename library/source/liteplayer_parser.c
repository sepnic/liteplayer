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

#include "cutils/log_helper.h"
#include "esp_adf/audio_common.h"
#include "audio_extractor/mp3_extractor.h"
#include "audio_extractor/aac_extractor.h"
#include "audio_extractor/m4a_extractor.h"
#include "audio_extractor/wav_extractor.h"

#include "liteplayer_config.h"
#include "liteplayer_parser.h"

#define TAG "[liteplayer]parser"

#define DEFAULT_MEDIA_PARSER_BUFFER_SIZE 2048

struct media_parser_priv {
    struct media_source_info source;
    struct media_codec_info codec;

    source_handle_t source_handle;
    long offset;

    media_parser_state_cb listener;
    void *listener_priv;

    bool stop;
    os_mutex lock; // lock for listener
    os_cond cond;  // wait stop to exit mediaparser thread
};

static int media_parser_fetch(char *buf, int wanted_size, long offset, void *priv)
{
    struct media_parser_priv *parser = (struct media_parser_priv *)priv;
    int bytes_read = ESP_FAIL;

    if (parser->offset != offset) {
        OS_LOGD(TAG, "seek source position: %ld>>%ld", parser->offset, offset);
        if (parser->source.source_ops->seek(parser->source_handle, offset) != 0)
            return ESP_FAIL;
        parser->offset = offset;
    }
    bytes_read = parser->source.source_ops->read(parser->source_handle, buf, wanted_size);

    if (bytes_read > 0)
        parser->offset += bytes_read;
    return bytes_read;
}

static int m4a_header_parse(struct media_source_info *source, struct media_codec_info *codec)
{
    struct media_parser_priv m4a_priv = {
        .offset = 0,
        .source_handle = NULL,
    };
    memcpy(&m4a_priv.source, source, sizeof(struct media_source_info));
    int ret = ESP_FAIL;

    m4a_priv.source_handle = source->source_ops->open(source->url, 0, source->source_ops->priv_data);
    if (m4a_priv.source_handle == NULL)
        return ESP_FAIL;

    if (m4a_extractor(media_parser_fetch, &m4a_priv, &(codec->detail.m4a_info)) == 0) {
        codec->content_pos = codec->detail.m4a_info.mdat_offset;
    #if defined(ENABLE_DECODER_AAC_SBR)
        codec->codec_samplerate = codec->detail.m4a_info.samplerate;
        codec->codec_channels = codec->detail.m4a_info.channels;
    #else
        codec->codec_samplerate = codec->detail.m4a_info.asc.samplerate;
        codec->codec_channels = codec->detail.m4a_info.asc.channels;
    #endif
        codec->codec_bits = codec->detail.m4a_info.bits;
        codec->duration_ms =
            (int)(codec->detail.m4a_info.duration/codec->detail.m4a_info.time_scale*1000);
        ret = ESP_OK;
    }

    source->source_ops->close(m4a_priv.source_handle);
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

static audio_codec_t get_codec_type(const char *url, char *buf)
{
    audio_codec_t codec = AUDIO_CODEC_NONE;
    if (memcmp(&buf[4], "ftyp", 4) == 0) {
        OS_LOGV(TAG, "Found M4A media");
        codec = AUDIO_CODEC_M4A;
    } else if (memcmp(&buf[0], "ID3", 3) == 0) {
        if (strstr(url, "mp3") != NULL) {
            OS_LOGV(TAG, "Found MP3 media with ID3 tag");
            codec = AUDIO_CODEC_MP3;
        } else if (strstr(url, "aac") != NULL) {
            OS_LOGV(TAG, "Found AAC media with ID3 tag");
            codec = AUDIO_CODEC_AAC;
        } else {
            OS_LOGV(TAG, "Unknown type with ID3, assume codec is MP3");
            codec = AUDIO_CODEC_MP3;
        }
    } else if ((buf[0] & 0xFF) == 0xFF && (buf[1] & 0xE0) == 0xE0) {
        if (strstr(url, "aac") != NULL &&
            (buf[0] & 0xFF) == 0xFF && (buf[1] & 0xF0) == 0xF0) {
            OS_LOGV(TAG, "Found AAC media raw data");
            codec = AUDIO_CODEC_AAC;
        } else {
            OS_LOGV(TAG, "Found MP3 media raw data");
            codec = AUDIO_CODEC_MP3;
        }
    } else if (memcmp(&buf[0], "RIFF", 4) == 0) {
        OS_LOGV(TAG, "Found wav media");
        codec = AUDIO_CODEC_WAV;
    }
    // todo: support flac/opus
    return codec;
}

static int media_header_parse(struct media_source_info *source, struct media_codec_info *codec)
{
    source_handle_t source_handle = NULL;
    long long content_len = 0;
    int bytes_read = 0;
    char buf[DEFAULT_MEDIA_PARSER_BUFFER_SIZE];
    int ret = ESP_FAIL;

    source_handle = source->source_ops->open(source->url, 0, source->source_ops->priv_data);
    if (source_handle == NULL) {
        OS_LOGE(TAG, "Failed to open source, url=%s", source->url);
        goto parse_done;
    }
    bytes_read = source->source_ops->read(source_handle, buf, sizeof(buf));
    if (bytes_read <= 0) {
        OS_LOGE(TAG, "Failed to read source");
        goto parse_done;
    }
    content_len = source->source_ops->content_len(source_handle);

    if (bytes_read < 64) {
        OS_LOGE(TAG, "Insufficient bytes read: %d", bytes_read);
        goto parse_done;
    }

    int frame_start_offset = 0, bytes_per_sec = 0;
    int sample_rate = 0, channels = 0, bits = 0;
    int duration_ms = 0;

    codec->codec_type = get_codec_type(source->url, buf);
    switch (codec->codec_type) {
    case AUDIO_CODEC_MP3: {
        frame_start_offset = get_start_offset(buf);
        struct mp3_info *info = &(codec->detail.mp3_info);
        int remain_size = bytes_read - frame_start_offset;

        if (remain_size >= 4) {
            if (mp3_parse_header(&buf[frame_start_offset], remain_size, info) != 0)
                goto parse_done;
        } else {
            bytes_read = 0;
            if (source->source_ops->seek(source_handle, frame_start_offset) != 0)
                goto parse_done;
            bytes_read = source->source_ops->read(source_handle, buf, sizeof(buf));
            if (bytes_read < 4)
                goto parse_done;
            if (mp3_parse_header(buf, bytes_read, info) != 0)
                goto parse_done;
        }

        sample_rate = info->sample_rate;
        channels = info->channels;
        bits = 16;
        bytes_per_sec = info->bit_rate*1000/8;
        if (content_len > frame_start_offset)
            duration_ms = (content_len - frame_start_offset)*8/info->bit_rate;
        break;
    }
    case AUDIO_CODEC_AAC: {
        frame_start_offset = get_start_offset(buf);
        struct aac_info *info = &(codec->detail.aac_info);
        int remain_size = bytes_read - frame_start_offset;

        if (remain_size >= 9) {
            if (aac_parse_adts_frame(&buf[frame_start_offset], remain_size, info) != 0)
                goto parse_done;
        } else {
            bytes_read = 0;
            if (source->source_ops->seek(source_handle, frame_start_offset) != 0)
                goto parse_done;
            bytes_read = source->source_ops->read(source_handle, buf, sizeof(buf));
            if (bytes_read < 9)
                goto parse_done;
            if (aac_parse_adts_frame(buf, bytes_read, info) != 0)
                goto parse_done;
        }

        sample_rate = info->sample_rate;
        channels = info->channels;
        bits = 16;
        //bytes_per_sec = info->bit_rate*1000/8;
        //if (content_len > frame_start_offset)
        //    duration_ms = (content_len - frame_start_offset)*8/info->bit_rate;
        break;
    }
    case AUDIO_CODEC_M4A: {
        OS_LOGD(TAG, "Found M4A media without m4a suffix");
        ret = m4a_header_parse(source, codec);
        goto parse_done;
        break;
    }
    case AUDIO_CODEC_WAV: {
        struct wav_info *info = &(codec->detail.wav_info);
        if (wav_parse_header(buf, bytes_read, info) != 0)
            goto parse_done;

        frame_start_offset = info->dataOffset;
        sample_rate = info->sampleRate;
        channels = info->channels;
        bits = info->bits;
        bytes_per_sec = info->blockAlign*info->sampleRate;
        duration_ms = (int)(info->dataSize/info->blockAlign/info->sampleRate*1000);
        break;
    }
    case AUDIO_CODEC_OPUS:
        OS_LOGE(TAG, "OPUS: unsupported codec");
        goto parse_done;
        break;
    case AUDIO_CODEC_FLAC:
        OS_LOGE(TAG, "FLAC: unsupported codec");
        goto parse_done;
        break;
    default:
        OS_LOGE(TAG, "Unknown codec type");
        goto parse_done;
        break;
    }

    codec->content_pos = frame_start_offset;
    codec->codec_samplerate = sample_rate;
    codec->codec_channels = channels;
    codec->codec_bits = bits;
    codec->bytes_per_sec = bytes_per_sec;
    codec->duration_ms = duration_ms;
    ret = ESP_OK;

parse_done:
    if (source_handle != NULL)
        source->source_ops->close(source_handle);
    return ret;
}

static void media_parser_cleanup(struct media_parser_priv *priv)
{
    if (priv->lock != NULL)
        os_mutex_destroy(priv->lock);
    if (priv->cond != NULL)
        os_cond_destroy(priv->cond);
    if (priv->source.url != NULL)
        audio_free(priv->source.url);
    audio_free(priv);
}

static void *media_parser_thread(void *arg)
{
    struct media_parser_priv *priv = (struct media_parser_priv *)arg;
    int ret = media_parser_get_codec_info(&priv->source, &priv->codec);

    {
        os_mutex_lock(priv->lock);

        if (!priv->stop && priv->listener) {
            if (ret == ESP_OK)
                priv->listener(MEDIA_PARSER_SUCCEED, &priv->codec, priv->listener_priv);
            else
                priv->listener(MEDIA_PARSER_FAILED, &priv->codec, priv->listener_priv);
        }

        OS_LOGV(TAG, "Waiting stop command");
        while (!priv->stop)
            os_cond_wait(priv->cond, priv->lock);

        os_mutex_unlock(priv->lock);
    }

    media_parser_cleanup(priv);
    OS_LOGD(TAG, "Media parser task leave");
    return NULL;
}

int media_parser_get_codec_info(struct media_source_info *source, struct media_codec_info *codec)
{
    if (source == NULL || source->url == NULL || codec == NULL)
        return ESP_FAIL;

    if (strstr(source->url, ".m3u") != NULL) {
        char temp[256];
        int ret = m3u_get_first_url(source, temp, sizeof(temp));
        if (ret == 0) {
            const char *media_url = audio_strdup(&temp[0]);
            if (media_url != NULL) {
                audio_free(source->url);
                source->url = media_url;
                OS_LOGV(TAG, "M3U first url: %s", media_url);
            }
        }
    }

    if (strstr(source->url, "m4a") != NULL) {
        codec->codec_type = AUDIO_CODEC_M4A;
        if (m4a_header_parse(source, codec) == ESP_OK)
            goto parse_succeed;
        // if failed, go ahead to check real codec type
    }

    if (media_header_parse(source, codec) != ESP_OK) {
        OS_LOGE(TAG, "Failed to parse url:[%s]", source->url);
        return ESP_FAIL;
    }

parse_succeed:
    OS_LOGI(TAG, "MediaInfo: codec_type[%d], samplerate[%d], channels[%d], bits[%d], offset[%ld], length[%ld]",
             codec->codec_type,
             codec->codec_samplerate, codec->codec_channels, codec->codec_bits,
             codec->content_pos, codec->content_len);
    return ESP_OK;
}

long long media_parser_get_seek_offset(struct media_codec_info *codec, int seek_msec)
{
    if (codec == NULL || seek_msec < 0)
        return -1;

    long long offset = -1;
    switch (codec->codec_type) {
    case AUDIO_CODEC_WAV:
    case AUDIO_CODEC_MP3: {
        offset = (codec->bytes_per_sec*(seek_msec/1000));
        break;
    }
    case AUDIO_CODEC_M4A: {
        unsigned int sample_index = 0;
        unsigned int sample_offset = 0;
        if (m4a_get_seek_offset(seek_msec, &(codec->detail.m4a_info), &sample_index, &sample_offset) != 0) {
            break;
        }
        offset = (long long)sample_offset - codec->content_pos;
        codec->detail.m4a_info.stsz_samplesize_index = sample_index;
        break;
    }
    default:
        OS_LOGE(TAG, "Unsupported seek for codec: %d", codec->codec_type);
        break;
    }
    if (codec->content_len > 0 && offset >= codec->content_len) {
        OS_LOGE(TAG, "Invalid seek offset");
        offset = -1;
    }
    return offset;
}

media_parser_handle_t media_parser_start_async(struct media_source_info *source,
                                               media_parser_state_cb listener,
                                               void *listener_priv)
{
    struct media_parser_priv *priv = audio_calloc(1, sizeof(struct media_parser_priv));
    if (priv == NULL || source == NULL || source->url == NULL)
        goto start_failed;

    memcpy(&priv->source, source, sizeof(struct media_source_info));
    priv->listener = listener;
    priv->listener_priv = listener_priv;
    priv->lock = os_mutex_create();
    priv->cond = os_cond_create();
    priv->source.url = audio_strdup(source->url);
    if (priv->lock == NULL || priv->cond == NULL || priv->source.url == NULL)
        goto start_failed;

    struct os_thread_attr attr = {
        .name = "ael-parser",
        .priority = DEFAULT_MEDIA_PARSER_TASK_PRIO,
        .stacksize = DEFAULT_MEDIA_PARSER_TASK_STACKSIZE,
        .joinable = false,
    };
    os_thread id = os_thread_create(&attr, media_parser_thread, priv);
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
        os_mutex_lock(priv->lock);
        priv->stop = true;
        os_cond_signal(priv->cond);
        os_mutex_unlock(priv->lock);
    }
}
