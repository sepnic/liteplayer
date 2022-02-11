// Copyright (c) 2019-2022 Qinglong<sysu.zqlong@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
