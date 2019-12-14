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
#include <stdint.h>

#include "esp_adf/esp_log.h"
#include "esp_adf/audio_common.h"
#include "audio_extractor/wav_extractor.h"

#define TAG "WAV_EXTRACTOR"

#define BSWAP_16(x) \
    (uint_16)( \
              (((uint_16)(x) & 0x00ff) << 8) | \
              (((uint_16)(x) & 0xff00) >> 8)   \
             )

#define BSWAP_32(x) \
    (uint_32)( \
              (((uint_32)(x) & 0xff000000) >> 24) | \
              (((uint_32)(x) & 0x00ff0000) >> 8)  | \
              (((uint_32)(x) & 0x0000ff00) << 8)  | \
              (((uint_32)(x) & 0x000000ff) << 24)   \
             )

#if !defined(BYTE_ORDER_BIG_ENDIAN)
#define COMPOSE_ID(a,b,c,d) ((a) | ((b)<<8) | ((c)<<16) | ((d)<<24))
#define LE_SHORT(v)         (v)
#define LE_INT(v)           (v)
#define BE_SHORT(v)         BSWAP_16(v)
#define BE_INT(v)           BSWAP_32(v)
#else
#define COMPOSE_ID(a,b,c,d) ((d) | ((c)<<8) | ((b)<<16) | ((a)<<24))
#define LE_SHORT(v)         BSWAP_16(v)
#define LE_INT(v)           BSWAP_32(v)
#define BE_SHORT(v)         (v)
#define BE_INT(v)           (v)
#endif

#define WAV_RIFF        COMPOSE_ID('R','I','F','F')
#define WAV_WAVE        COMPOSE_ID('W','A','V','E')
#define WAV_FMT         COMPOSE_ID('f','m','t',' ')
#define WAV_DATA        COMPOSE_ID('d','a','t','a')

/* WAVE fmt block constants from Microsoft mmreg.h header */
#define WAV_FMT_PCM             0x0001
#define WAV_FMT_IEEE_FLOAT      0x0003
#define WAV_FMT_DOLBY_AC3_SPDIF 0x0092
#define WAV_FMT_EXTENSIBLE      0xfffe

/* WAV_FMT_EXTENSIBLE format */
#define WAV_GUID_TAG "/x00/x00/x00/x00/x10/x00/x80/x00/x00/xAA/x00/x38/x9B/x71"

int wav_parse_header(char *buf, int buf_size, wav_info_t *info)
{
    if (buf_size < sizeof(wav_header_t))
        return -1;

    wav_header_t *h = (wav_header_t *)buf;

    if (h->riff.ChunkID != WAV_RIFF ||
        h->riff.Format != WAV_WAVE ||
        h->fmt.ChunkID != WAV_FMT ||
        h->fmt.ChunkSize != LE_INT(16) ||
        h->fmt.AudioFormat != LE_SHORT(WAV_FMT_PCM) ||
        (h->fmt.NumOfChannels != LE_SHORT(1) && h->fmt.NumOfChannels != LE_SHORT(2)) ||
        h->data.ChunkID != WAV_DATA) {
        return -1;
    }

    info->channels   = h->fmt.NumOfChannels;
    info->sampleRate = h->fmt.SampleRate;
    info->bits       = h->fmt.BitsPerSample;
    info->dataSize   = h->data.ChunkSize;
    info->dataShift  = sizeof(wav_header_t);
    info->blockAlign = h->fmt.BlockAlign;
    info->byteRate   = h->fmt.ByteRate;
    return 0;
}

void wav_build_header(wav_header_t *header, int samplerate, int bits, int channels, int datasize)
{
    header->riff.ChunkID = WAV_RIFF;
    header->riff.Format = WAV_WAVE;
    header->riff.ChunkSize = LE_INT(datasize+sizeof(wav_header_t)-8);
    header->fmt.ChunkID = WAV_FMT;
    header->fmt.ChunkSize = LE_INT(16);
    header->fmt.AudioFormat = LE_SHORT(WAV_FMT_PCM);
    header->fmt.NumOfChannels = LE_SHORT(channels);
    header->fmt.SampleRate = LE_INT(samplerate);
    header->fmt.BitsPerSample = LE_SHORT(bits);
    header->fmt.BlockAlign = LE_SHORT(bits*channels/8);
    header->fmt.ByteRate = LE_INT(header->fmt.BlockAlign*samplerate);
    header->data.ChunkID = WAV_DATA;
    header->data.ChunkSize = LE_INT(datasize);
}

int wav_extractor(wav_fetch_cb fetch_cb, void *fetch_priv, wav_info_t *info)
{
    int buf_size = sizeof(wav_header_t);
    char *buf = (char *)audio_calloc(1, buf_size);
    int ret = -1;

    AUDIO_MEM_CHECK(TAG, buf, return -1);

    buf_size = fetch_cb(buf, buf_size, 0, fetch_priv);
    if (buf_size != sizeof(wav_header_t)) {
        ESP_LOGE(TAG, "Not enough data[%d] to parse", buf_size);
        goto finish;
    }

    ret = wav_parse_header(buf, buf_size, info);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to parse wav header");
    }

finish:
    audio_free(buf);
    return ret;
}
