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

#ifndef _LITEPLAYER_MEDIAPARSER_H_
#define _LITEPLAYER_MEDIAPARSER_H_

#include "esp_adf/audio_common.h"
#include "audio_extractor/mp3_extractor.h"
#include "audio_extractor/aac_extractor.h"
#include "audio_extractor/m4a_extractor.h"
#include "audio_extractor/wav_extractor.h"
#include "liteplayer_source.h"

#ifdef __cplusplus
extern "C" {
#endif

enum media_parser_state {
    MEDIA_PARSER_FAILED = -1,
    MEDIA_PARSER_SUCCEED = 0,
};

struct media_codec_info {
    audio_codec_t       codec_type;
    int                 codec_samplerate;
    int                 codec_channels;
    int                 codec_bits;
    long                content_pos;
    long                content_len;
    int                 bytes_per_sec;
    int                 duration_ms;
    union {
        struct wav_info wav_info;
        struct mp3_info mp3_info;
        struct aac_info aac_info;
        struct m4a_info m4a_info;
        //struct opus_info opus_info;
        //struct flac_info flac_info;
    } detail;
};

typedef void (*media_parser_state_cb)(enum media_parser_state state, struct media_codec_info *info, void *priv);

typedef void *media_parser_handle_t;

int media_info_parse(struct media_source_info *source, struct media_codec_info *codec);

media_parser_handle_t media_parser_start_async(struct media_source_info *source,
                                               media_parser_state_cb listener,
                                               void *listener_priv);

void media_parser_stop(media_parser_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // _LITEPLAYER_MEDIAPARSER_H_
