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

#ifndef _MP3_EXTRACTOR_H_
#define _MP3_EXTRACTOR_H_

#ifdef __cplusplus
extern "C" {
#endif

// Return the data size obtained
typedef int (*mp3_fetch_cb)(char *buf, int wanted_size, long offset, void *fetch_priv);

typedef struct mp3_info {
    int channels;
    int sample_rate;
    int bit_rate;
    int frame_size;
    int frame_start_offset;
    int id3v2_length;
} mp3_info_t;

int mp3_find_syncword(char *buf, int size);

int mp3_parse_header(char *buf, int buf_size, mp3_info_t *info);

int mp3_extractor(mp3_fetch_cb fetch_cb, void *fetch_priv, mp3_info_t *info);

#ifdef __cplusplus
}
#endif

#endif
