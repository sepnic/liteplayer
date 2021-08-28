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

#ifndef _LITEPLAYER_RESAMPLER_H_
#define _LITEPLAYER_RESAMPLER_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RESAMPLER_CFG_DEFAULT()    {      \
    .in_rate      = 44100,                \
    .in_channels  = 2,                    \
    .out_rate     = 48000,                \
    .out_channels = 2,                    \
    .bits         = 16,                   \
    .quality      = 8,                    \
}

struct resampler_cfg {
    int  in_rate;
    int  in_channels;
    int  out_rate;
    int  out_channels;
    int  bits;
    int  quality;
};

struct resampler;
typedef struct resampler *resampler_handle_t;

struct resampler {
    int (*open)(resampler_handle_t self, struct resampler_cfg *config);
    int (*process)(resampler_handle_t self, const short *in, int in_bytes);
    void (*close)(resampler_handle_t self);
    void (*destroy)(resampler_handle_t self);
    int out_bytes;   /*!< will update output length after process() */
    short *out_buf;  /*!< will update output pointer after process() */
};

resampler_handle_t resampler_init();

#ifdef __cplusplus
}
#endif

#endif // _LITEPLAYER_RESAMPLER_H_
