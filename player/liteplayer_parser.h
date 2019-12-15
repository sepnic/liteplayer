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

typedef enum media_parser_state {
    MEDIA_PARSER_FAILED = -1,
    MEDIA_PARSER_SUCCEED = 0,
} media_parser_state_t;

typedef struct media_codec_info {
    audio_codec_t       codec_type;
    int                 codec_samplerate;
    int                 codec_channels;
    long                content_pos;
    long                content_len;
    long long           duration_ms;
    m4a_info_t          m4a_info;
} media_codec_info_t;

typedef void (*media_parser_state_cb)(media_parser_state_t state, media_codec_info_t *media_info, void *priv);

typedef void *media_parser_handle_t;

int media_info_parse(media_source_info_t *source_info, media_codec_info_t *media_info);

media_parser_handle_t media_parser_start_async(media_source_info_t *source_info,
                                               media_parser_state_cb listener,
                                               void *listener_priv);
void media_parser_stop(media_parser_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif
