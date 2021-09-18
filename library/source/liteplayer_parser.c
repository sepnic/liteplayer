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

#define DEFAULT_MEDIA_PARSER_BUFFER_SIZE    (2048+1)
#define DEFAULT_MEDIA_PARSER_DISCARD_MAX    (1024*512)
#define DEFAULT_MEDIA_PARSER_WRITE_TIMEOUT  (200)

struct media_parser_priv {
    struct media_source_info source;
    struct media_codec_info codec;
    char header_buffer[DEFAULT_MEDIA_PARSER_BUFFER_SIZE];
    int header_size;
    char reuse_buffer[DEFAULT_MEDIA_PARSER_BUFFER_SIZE];
    int reuse_size;
    int ringbuf_size;

    media_parser_state_cb listener;
    void *listener_priv;
    struct media_source_info *listener_source;

    bool stop;
    os_mutex lock; // lock for listener
    os_cond cond;  // wait stop to exit mediaparser thread
};

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

static int media_parser_fetch(char *buf, int wanted_size, long offset, void *arg)
{
    struct media_parser_priv *priv = (struct media_parser_priv *)arg;
    int bytes_read = ESP_FAIL;
    long content_pos = (long)priv->source.source_ops->content_pos(priv->source.source_handle);

    if (wanted_size > sizeof(priv->reuse_buffer)) {
        OS_LOGW(TAG, "Extractor wanted %d bytes, bigger than parser buffer size (%d)",
                wanted_size, (int)sizeof(priv->reuse_buffer));
        OS_LOGW(TAG, "Media source may reopen file again, it's best to reduce extractor buffer");
    }

    if (priv->header_size == content_pos && offset < priv->header_size) {
        int bytes_remain = priv->header_size - offset;
        if (bytes_remain >= wanted_size) {
            memcpy(priv->reuse_buffer, priv->header_buffer, priv->header_size);
            priv->reuse_size = priv->header_size;
            memcpy(buf, &priv->header_buffer[offset], wanted_size);
            return wanted_size;
        } else {
            memcpy(buf, &priv->header_buffer[offset], bytes_remain);
            wanted_size -= bytes_remain;
            bytes_read = priv->source.source_ops->read(priv->source.source_handle,
                    &buf[bytes_remain], wanted_size);
            if (bytes_read > 0) {
                bytes_read += bytes_remain;
                if (bytes_read <= sizeof(priv->reuse_buffer)) {
                    memcpy(priv->reuse_buffer, buf, bytes_read);
                    priv->reuse_size = bytes_read;
                } else {
                    memcpy(priv->reuse_buffer, &buf[bytes_read-sizeof(priv->reuse_buffer)],
                            sizeof(priv->reuse_buffer));
                    priv->reuse_size = sizeof(priv->reuse_buffer);
                }
                return bytes_read;
            } else {
                OS_LOGW(TAG, "Failed to read more bytes: %d/%d, return remaining %d bytes",
                        bytes_read, wanted_size, bytes_remain);
                memcpy(priv->reuse_buffer, priv->header_buffer, priv->header_size);
                priv->reuse_size = priv->header_size;
                return bytes_remain;
            }
        }
    }

    //content_pos = (long)priv->source.source_ops->content_pos(priv->source.source_handle);
    if (content_pos != offset) {
        if ((offset > content_pos) &&
            (offset - content_pos) <= DEFAULT_MEDIA_PARSER_DISCARD_MAX) {
            int bytes_discard = offset - content_pos;
            OS_LOGD(TAG, "Discarding %d bytes to reach new offset", bytes_discard);
            while (bytes_discard > 0) {
                priv->reuse_size = priv->source.source_ops->read(priv->source.source_handle,
                        priv->reuse_buffer, sizeof(priv->reuse_buffer));
                if (priv->reuse_size > 0) {
                    bytes_discard -= priv->reuse_size;
                } else {
                    OS_LOGE(TAG, "Failed to read, bytes_read: %d", priv->reuse_size);
                    return ESP_FAIL;
                }
            }
            content_pos = (long)priv->source.source_ops->content_pos(priv->source.source_handle);
            int bytes_remain = content_pos - offset;
            if (bytes_remain >= wanted_size) {
                memcpy(buf, &priv->reuse_buffer[priv->reuse_size - bytes_remain], wanted_size);
                return wanted_size;
            } else if (bytes_remain >= 256 && bytes_remain >= wanted_size/4) { // todo
                OS_LOGW(TAG, "Extractor want %d bytes, but now we can only provide %d bytes",
                        wanted_size, bytes_remain);
                memcpy(buf, &priv->reuse_buffer[priv->reuse_size - bytes_remain], bytes_remain);
                return bytes_remain;
            } else {
                OS_LOGW(TAG, "Remain %d bytes is too little, will fallthrough to seek", bytes_remain);
            }
        }

        OS_LOGD(TAG, "Seeking %ld>>%ld", content_pos, offset);
        if (priv->source.source_ops->seek(priv->source.source_handle, offset) != 0)
            return ESP_FAIL;
    }

    bytes_read = priv->source.source_ops->read(priv->source.source_handle, buf, wanted_size);
    if (bytes_read > 0) {
        if (bytes_read <= sizeof(priv->reuse_buffer)) {
            memcpy(priv->reuse_buffer, buf, bytes_read);
            priv->reuse_size = bytes_read;
        } else {
            memcpy(priv->reuse_buffer, &buf[bytes_read-sizeof(priv->reuse_buffer)],
                    sizeof(priv->reuse_buffer));
            priv->reuse_size = sizeof(priv->reuse_buffer);
        }
    } else if (bytes_read < 0) {
        OS_LOGE(TAG, "Failed to read wanted %d bytes, bytes_read(%d)", wanted_size, bytes_read);
    }
    return bytes_read;
}

static int media_parser_extract(struct media_parser_priv *priv)
{
    int ret = ESP_FAIL;
    struct media_codec_info *codec = &priv->codec;
    int read_size = sizeof(priv->header_buffer);

    if (read_size > priv->ringbuf_size)
        read_size = priv->ringbuf_size;
    priv->header_size =
        priv->source.source_ops->read(priv->source.source_handle, priv->header_buffer, read_size);
    if (priv->header_size < 256) {
        OS_LOGE(TAG, "Insufficient bytes read: %d", priv->header_size);
        return ESP_FAIL;
    }

    codec->codec_type = get_codec_type(priv->source.url, priv->header_buffer);
    switch (codec->codec_type) {
    case AUDIO_CODEC_MP3: {
        if (mp3_extractor(media_parser_fetch, priv, &(codec->detail.mp3_info)) == 0) {
            codec->codec_samplerate = codec->detail.mp3_info.sample_rate;
            codec->codec_channels = codec->detail.mp3_info.channels;
            codec->codec_bits = 16;
            codec->content_pos = codec->detail.mp3_info.frame_start_offset;
            codec->content_len = priv->source.source_ops->content_len(priv->source.source_handle);
            codec->bytes_per_sec = codec->detail.mp3_info.bit_rate*1000/8;
            codec->duration_ms = (codec->content_len - codec->content_pos)*8/codec->detail.mp3_info.bit_rate;
            ret = ESP_OK;
        }
        break;
    }

    case AUDIO_CODEC_AAC: {
        if (aac_extractor(media_parser_fetch, priv, &(codec->detail.aac_info)) == 0) {
            codec->codec_samplerate = codec->detail.aac_info.sample_rate;
            codec->codec_channels = codec->detail.aac_info.channels;
            codec->codec_bits = 16;
            codec->content_pos = codec->detail.aac_info.frame_start_offset;
            codec->content_len = priv->source.source_ops->content_len(priv->source.source_handle);
            //codec->bytes_per_sec = codec->detail.aac_info.bit_rate*1000/8;
            //codec->duration_ms = (codec->content_len - codec->content_pos)*8/codec->detail.aac_info.bit_rate;
            ret = ESP_OK;
        }
        break;
    }

    case AUDIO_CODEC_M4A:
        if (m4a_extractor(media_parser_fetch, priv, &(codec->detail.m4a_info)) == 0) {
            codec->content_pos = codec->detail.m4a_info.mdat_offset;
            codec->content_len = priv->source.source_ops->content_len(priv->source.source_handle);
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
        break;

    case AUDIO_CODEC_WAV: {
        if (wav_extractor(media_parser_fetch, priv, &(codec->detail.wav_info)) == 0) {
            codec->codec_samplerate = codec->detail.wav_info.sampleRate;
            codec->codec_channels = codec->detail.wav_info.channels;
            codec->codec_bits = codec->detail.wav_info.bits;
            codec->content_pos = codec->detail.wav_info.dataOffset;
            codec->content_len = priv->source.source_ops->content_len(priv->source.source_handle);
            codec->bytes_per_sec = codec->detail.wav_info.blockAlign*codec->detail.wav_info.sampleRate;
            codec->duration_ms = (int)(codec->detail.wav_info.dataSize/codec->detail.wav_info.blockAlign/codec->detail.wav_info.sampleRate*1000);
            ret = ESP_OK;
        }
        break;
    }

    default:
        break;
    }

    return ret;
}

static int media_parser_main(struct media_parser_priv *priv)
{
    priv->source.source_handle =
        priv->source.source_ops->open(priv->source.url, 0, priv->source.source_ops->priv_data);
    if (priv->source.source_handle == NULL)
        return ESP_FAIL;

    bool reuse_handle = false;
    int ret = media_parser_extract(priv);
    if (ret == ESP_OK) {
        OS_LOGI(TAG, "MediaInfo: codec_type[%d], samplerate[%d], channels[%d], bits[%d], pos[%ld], len[%ld], duration[%dms]",
                priv->codec.codec_type, priv->codec.codec_samplerate, priv->codec.codec_channels, priv->codec.codec_bits,
                priv->codec.content_pos, priv->codec.content_len, priv->codec.duration_ms);
    } else {
        OS_LOGE(TAG, "Failed to parse url:[%s]", priv->source.url);
    }

    if (ret == ESP_OK) {
        long content_pos = (long)priv->source.source_ops->content_pos(priv->source.source_handle);
        OS_LOGV(TAG, "content_pos=%ld, frame_start_offset=%ld", content_pos, priv->codec.content_pos);

        if (priv->codec.content_pos > content_pos &&
            (priv->codec.content_pos - content_pos) <= DEFAULT_MEDIA_PARSER_DISCARD_MAX) {
            int bytes_discard = priv->codec.content_pos - content_pos;
            OS_LOGD(TAG, "Try to discard %d bytes to reach frame_start_offset", bytes_discard);
            while (bytes_discard > 0) {
                priv->reuse_size = priv->source.source_ops->read(priv->source.source_handle,
                        priv->reuse_buffer, sizeof(priv->reuse_buffer));
                if (priv->reuse_size > 0)
                    bytes_discard -= priv->reuse_size;
                else
                    goto reuse_out;
            }
            content_pos = (long)priv->source.source_ops->content_pos(priv->source.source_handle);
            OS_LOGV(TAG, "content_pos=%ld, frame_start_offset=%ld", content_pos, priv->codec.content_pos);
        }

        if (content_pos < priv->codec.content_pos ||
            (content_pos - priv->codec.content_pos) > priv->reuse_size)
            goto reuse_out;

        if (priv->lock != NULL)
            os_mutex_lock(priv->lock);

        if (!priv->stop) {
            int bytes_written = content_pos - priv->codec.content_pos;
            rb_reset(priv->source.out_ringbuf);
            if (bytes_written == 0) {
                OS_LOGD(TAG, "Mediasource will reuse source handle, content_pos: %ld", content_pos);
                reuse_handle = true;
            } else if (rb_get_size(priv->source.out_ringbuf) >= bytes_written) {
                OS_LOGD(TAG, "Mediasource will reuse source handle, save remaing %d bytes in ringbuf", bytes_written);
                int ret = rb_write_chunk(priv->source.out_ringbuf,
                                         &priv->reuse_buffer[priv->reuse_size-bytes_written],
                                         bytes_written,
                                         DEFAULT_MEDIA_PARSER_WRITE_TIMEOUT);
                if (ret == bytes_written)
                    reuse_handle = true;
            }
        }

        if (priv->lock != NULL)
            os_mutex_unlock(priv->lock);
    }

reuse_out:
    if (!reuse_handle || priv->stop) {
        priv->source.source_ops->close(priv->source.source_handle);
        priv->source.source_handle = NULL;
    }
    return ret;
}

int media_parser_get_codec_info(struct media_source_info *source, struct media_codec_info *codec)
{
    if (source == NULL || source->url == NULL || source->out_ringbuf == NULL || codec == NULL)
        return ESP_FAIL;

    struct media_parser_priv *priv = audio_calloc(1, sizeof(struct media_parser_priv));
    if (priv == NULL)
        return ESP_FAIL;
    memcpy(&priv->source, source, sizeof(struct media_source_info));
    priv->ringbuf_size = rb_get_size(source->out_ringbuf);

    bool free_url = false;
    if (strstr(priv->source.url, ".m3u") != NULL) {
        char temp[256];
        int ret = m3u_get_first_url(source, temp, sizeof(temp));
        if (ret == 0) {
            const char *media_url = audio_strdup(&temp[0]);
            if (media_url != NULL) {
                priv->source.url = media_url;
                free_url = true;
                OS_LOGV(TAG, "M3U first url: %s", media_url);
            }
        }
    }

    int ret = media_parser_main(priv);
    // update source handle for media source, we will reuse this handle
    source->source_handle = priv->source.source_handle;
    if (ret == ESP_OK)
        memcpy(codec, &priv->codec, sizeof(struct media_codec_info));

    if (free_url)
        audio_free(priv->source.url);
    audio_free(priv);
    return ret;
}

static int media_parser_get_codec_info2(struct media_parser_priv *priv)
{
    if (priv == NULL)
        return ESP_FAIL;

    if (strstr(priv->source.url, ".m3u") != NULL) {
        char temp[256];
        int ret = m3u_get_first_url(&priv->source, temp, sizeof(temp));
        if (ret == 0) {
            const char *media_url = audio_strdup(&temp[0]);
            if (media_url != NULL) {
                audio_free(priv->source.url);
                priv->source.url = media_url;
                OS_LOGV(TAG, "M3U first url: %s", media_url);
            }
        }
    }

    int ret = media_parser_main(priv);
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
    int ret = media_parser_get_codec_info2(priv);

    {
        os_mutex_lock(priv->lock);

        if (!priv->stop) {
            if (priv->listener != NULL) {
                enum media_parser_state state =
                    (ret == ESP_OK) ? MEDIA_PARSER_SUCCEED : MEDIA_PARSER_FAILED;
                priv->listener(state, &priv->codec, priv->listener_priv);
            }
            // update source handle for media source, we will reuse this handle
            priv->listener_source->source_handle = priv->source.source_handle;
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

media_parser_handle_t media_parser_start_async(struct media_source_info *source,
                                               media_parser_state_cb listener,
                                               void *listener_priv)
{
    struct media_parser_priv *priv = audio_calloc(1, sizeof(struct media_parser_priv));
    if (priv == NULL || source == NULL || source->url == NULL || source->out_ringbuf == NULL)
        goto start_failed;

    memcpy(&priv->source, source, sizeof(struct media_source_info));
    priv->ringbuf_size = rb_get_size(source->out_ringbuf);
    priv->listener = listener;
    priv->listener_priv = listener_priv;
    priv->listener_source = source;
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
