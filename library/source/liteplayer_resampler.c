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
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "cutils/log_helper.h"
#include "esp_adf/audio_common.h"
#include "speex/speex_resampler.h"
#include "liteplayer_resampler.h"

#define TAG "[liteplayer]RESAMPLER"

struct resample_priv {
    struct resampler     converter;
    struct resampler_cfg cfg;
    SpeexResamplerState *src_state;
    int                  in_bytes;
    int                  out_bytes;
    bool                 enable_rate_convert;
    bool                 enable_channels_convert;
    bool                 opened;
};
typedef struct resample_priv *resample_priv_handle_t;

static int mono_to_stereo(short *buf, int nbytes)
{
    int nsamples = nbytes/2;
    for (int i = nsamples-1; i >= 0; i--) {
        buf[2*i+0] = buf[i];
        buf[2*i+1] = buf[i];
    }
    return 0;
}

static int stereo_to_mono(short *buf, int nbytes)
{
    int nsamples = nbytes/4;
    for (int i = 0; i < nsamples; i++)
        buf[i] = buf[i*2];
    return 0;
}

static int resampler_open(resampler_handle_t self, struct resampler_cfg *config)
{
    resample_priv_handle_t priv = (resample_priv_handle_t)self;
    int in_channels  = config->in_channels;
    int in_rate      = config->in_rate;
    int out_channels = config->out_channels;
    int out_rate     = config->out_rate;
    int bits         = config->bits;
    int quality      = config->quality;
    int ret = 0;

    OS_LOGD(TAG, "Open resampler: in_channels(%d), in_rate(%d), out_channels(%d), out_rate(%d), bits(%d), quality(%d)",
            in_channels, in_rate, out_channels, out_rate, bits, quality);

    if (bits != 16) {
        OS_LOGE(TAG, "Unsupported sample bits(%d) for rate_convert", bits);
        return -1;
    }

    memcpy(&priv->cfg, config, sizeof(struct resampler_cfg));
    priv->enable_rate_convert = (in_rate != out_rate) ? true : false;
    priv->enable_channels_convert = (in_channels != out_channels) ? true : false;
    if (priv->enable_channels_convert) {
        if (in_channels != 1 && in_channels != 2 && out_channels != 1 && out_channels != 2) {
            OS_LOGE(TAG, "Unsupported channels(%d>>%d) for channels_convert", in_channels, out_channels);
            return -1;
        }
    }

    if (priv->enable_rate_convert) {
        SpeexResamplerState *src_state = speex_resampler_init(in_channels, in_rate, out_rate, quality, &ret);
        if (ret == 0) {
            speex_resampler_skip_zeros(src_state);
            priv->src_state = src_state;
        }
    }

    priv->opened = ret == 0 ? true : false;
    return ret;
}

static int resampler_process(resampler_handle_t self, const short *in, int in_bytes)
{
    resample_priv_handle_t priv = (resample_priv_handle_t)self;
    resampler_handle_t converter = &(priv->converter);
    int in_channels  = priv->cfg.in_channels;
    int out_channels = priv->cfg.out_channels;
    int ret = 0;

    if (!priv->enable_channels_convert && !priv->enable_rate_convert) {
        converter->out_buf = (short *)in;
        converter->out_bytes = in_bytes;
        return 0;
    }

    if (!priv->opened) {
        OS_LOGE(TAG, "Resampler is NOT opened");
        return -1;
    }

    if (converter->out_buf == NULL) {
        int in_rate   = priv->cfg.in_rate;
        int out_rate  = priv->cfg.out_rate;
        int out_bytes = in_bytes * out_rate / in_rate + 4; /* 4 more bytes(one frame) for fraction ratio */

        if (in_channels == 1 && out_channels == 2)
            out_bytes *= 2;

        converter->out_buf = (short *)audio_calloc(1, out_bytes);
        AUDIO_MEM_CHECK(TAG, converter->out_buf, return -1);

        priv->in_bytes = in_bytes;
        priv->out_bytes = out_bytes;
    } else if (in_bytes > priv->in_bytes) {
        int in_rate   = priv->cfg.in_rate;
        int out_rate  = priv->cfg.out_rate;
        int out_bytes  = in_bytes * out_rate / in_rate + 4; /* 4 more bytes(one frame) for fraction ratio */

        if (in_channels == 1 && out_channels == 2)
            out_bytes *= 2;

        if (out_bytes > priv->out_bytes) {
            OS_LOGV(TAG, "Not enough buffer, realloc new size(%d)", out_bytes);
            converter->out_buf = (short *)audio_realloc(converter->out_buf, out_bytes);
            AUDIO_MEM_CHECK(TAG, converter->out_buf, return -1);
        }

        priv->in_bytes = in_bytes;
        priv->out_bytes = out_bytes;
    }

    if (in_channels == 2 && out_channels == 1) {
        stereo_to_mono((short *)in, in_bytes);
        in_channels = 1;
        in_bytes /= 2;
    }

    if (priv->enable_rate_convert) {
        spx_uint32_t bytes_per_sample = in_channels * priv->cfg.bits / 8;
        spx_uint32_t in_sample = in_bytes / bytes_per_sample;
        spx_uint32_t out_sample = priv->out_bytes / bytes_per_sample;
        if (in_channels == 1)
            ret = speex_resampler_process_int(priv->src_state, 0, in, &in_sample, converter->out_buf, &out_sample);
        else if (in_channels == 2)
            ret = speex_resampler_process_interleaved_int(priv->src_state, in, &in_sample, converter->out_buf, &out_sample);
        else
            return -1;
        converter->out_bytes = out_sample * bytes_per_sample;
    } else if (priv->enable_channels_convert) {
        memcpy(converter->out_buf, in, in_bytes);
        converter->out_bytes = in_bytes;
    }

    if (in_channels == 1 && out_channels == 2) {
        mono_to_stereo(converter->out_buf, converter->out_bytes);
        converter->out_bytes *= 2;
    }

    //OS_LOGV(TAG, "in_sample=%d, out_sample=%d, in_bytes=%d, out_bytes=%d",
    //         in_sample, out_sample, in_bytes, converter->out_bytes);
    return ret;
}

static void resampler_close(resampler_handle_t self)
{
    resample_priv_handle_t priv = (resample_priv_handle_t)self;
    resampler_handle_t converter = &(priv->converter);

    if (priv->enable_channels_convert) {
        if (priv->src_state != NULL) {
            speex_resampler_destroy(priv->src_state);
            priv->src_state = NULL;
        }
    }

    if (priv->enable_channels_convert || priv->enable_rate_convert) {
        if (converter->out_buf != NULL) {
            audio_free(converter->out_buf);
            converter->out_buf = NULL;
        }
    }

    priv->opened = false;
}

static void resampler_destroy(resampler_handle_t self)
{
    resample_priv_handle_t priv = (resample_priv_handle_t)self;
    if (priv->opened)
        resampler_close(self);
    audio_free(priv);
}

resampler_handle_t resampler_init()
{
    resample_priv_handle_t handle = audio_calloc(1, sizeof(struct resample_priv));
    AUDIO_MEM_CHECK(TAG, handle, return NULL);

    handle->converter.open    = resampler_open;
    handle->converter.process = resampler_process;
    handle->converter.close   = resampler_close;
    handle->converter.destroy = resampler_destroy;
    return (resampler_handle_t)handle;
}