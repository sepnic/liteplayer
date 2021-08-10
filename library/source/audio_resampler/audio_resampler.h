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

#ifndef _AUDIO_RESAMPLER_H_
#define _AUDIO_RESAMPLER_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_RESAMPLE_CFG_DEFAULT()    { \
    .in_rate      = 16000,                \
    .in_channels  = 2,                    \
    .out_rate     = 44100,                \
    .out_channels = 2,                    \
    .bits         = 16,                   \
    .quality      = 8,                    \
}

struct resample_cfg {
    int  in_channels;
    int  in_rate;
    int  out_channels;
    int  out_rate;
    int  bits;
    int  quality;
};

struct resample_converter;
typedef struct resample_converter *resample_converter_handle_t;

struct resample_converter {
    int (*open)(resample_converter_handle_t self, struct resample_cfg *config);
    int (*process)(resample_converter_handle_t self, const short *in, int in_bytes);
    int (*close)(resample_converter_handle_t self);
    void (*destroy)(resample_converter_handle_t self);
    int out_bytes;          /*!< Output length for one converter */
    short  *out_buf;         /*!< Output pointer for rate converter */
};

resample_converter_handle_t audio_resampler_init();

#ifdef __cplusplus
}
#endif

#endif
