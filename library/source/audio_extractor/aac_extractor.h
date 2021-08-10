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

#ifndef _AAC_EXTRACTOR_H_
#define _AAC_EXTRACTOR_H_

#ifdef __cplusplus
extern "C" {
#endif

// Return the data size obtained
typedef int (*aac_fetch_cb)(char *buf, int wanted_size, long offset, void *fetch_priv);

struct aac_info {
    int channels;
    int sample_rate;
    int frame_size;
    int frame_start_offset;
};

int aac_parse_adts_frame(char *buf, int buf_size, struct aac_info *info);

int aac_extractor(aac_fetch_cb fetch_cb, void *fetch_priv, struct aac_info *info);

#ifdef __cplusplus
}
#endif

#endif
