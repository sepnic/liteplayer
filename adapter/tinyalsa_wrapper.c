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
#include <string.h>

#include "cutils/os_logger.h"
#include "tinyalsa/asoundlib.h"
#include "tinyalsa_wrapper.h"

#define TAG "[liteplayer]tinyalsa"

#define DEFAULT_SND_CARD   0
#define DEFAULT_SND_DEVICE 0

static int tinyalsa_check_param(struct pcm_params *params, unsigned int param, unsigned int value,
        char *param_name, char *param_unit)
{
    unsigned int min;
    unsigned int max;
    int is_within_bounds = 1;

    min = pcm_params_get_min(params, param);
    if (value < min) {
        OS_LOGE(TAG, "%s is %u%s, device only supports >= %u%s", param_name, value,
                param_unit, min, param_unit);
        is_within_bounds = 0;
    }

    max = pcm_params_get_max(params, param);
    if (value > max) {
        OS_LOGE(TAG, "%s is %u%s, device only supports <= %u%s", param_name, value,
                param_unit, max, param_unit);
        is_within_bounds = 0;
    }

    return is_within_bounds;
}

static bool tinyalsa_can_play(unsigned int card, unsigned int device,
        unsigned int channels, unsigned int rate, unsigned int bits,
        unsigned int period_size, unsigned int period_count)
{
    struct pcm_params *params;
    int can_play;

    params = pcm_params_get(card, device, PCM_OUT);
    if (params == NULL) {
        OS_LOGE(TAG, "Unable to open PCM device %u", device);
        return false;
    }

    can_play  = tinyalsa_check_param(params, PCM_PARAM_RATE, rate, "Sample rate", "Hz");
    can_play &= tinyalsa_check_param(params, PCM_PARAM_CHANNELS, channels, "Sample", " channels");
    can_play &= tinyalsa_check_param(params, PCM_PARAM_SAMPLE_BITS, bits, "Bitrate", " bits");
    can_play &= tinyalsa_check_param(params, PCM_PARAM_PERIOD_SIZE, period_size, "Period size", " frames");
    can_play &= tinyalsa_check_param(params, PCM_PARAM_PERIODS, period_count, "Period count", " periods");

    pcm_params_free(params);
    return !!can_play;
}

sink_handle_t tinyalsa_wrapper_open(int samplerate, int channels, void *sink_priv)
{
    struct pcm *pcm = NULL;
    struct pcm_config config;
    unsigned int card = DEFAULT_SND_CARD;
    unsigned int device = DEFAULT_SND_CARD;
    unsigned int bits = 16;
    unsigned int period_size = 1024;
    unsigned int period_count = 4;

    if (!tinyalsa_can_play(card, device, channels, samplerate, bits, period_size, period_count)) {
        OS_LOGE(TAG, "Invalid pcm params");
        return NULL;
    }

    memset(&config, 0, sizeof(config));
    config.channels = channels;
    config.rate = samplerate;
    config.period_size = period_size;
    config.period_count = period_count;
    config.format = PCM_FORMAT_S16_LE;
    config.start_threshold = 0;
    config.stop_threshold = 0;
    config.silence_threshold = 0;

    pcm = pcm_open(DEFAULT_SND_CARD, DEFAULT_SND_DEVICE, PCM_OUT, &config);
    if (pcm == NULL || !pcm_is_ready(pcm)) {
        OS_LOGE(TAG, "Unable to open PCM (%s)", pcm_get_error(pcm));
        return NULL;
    }
    return pcm;
}

int tinyalsa_wrapper_write(sink_handle_t handle, char *buffer, int size)
{
    struct pcm *pcm = (struct pcm *)handle;
    if (pcm_write(pcm, buffer, size)) {
        OS_LOGE(TAG, "Error writing pcm\n");
        return -1;
    }
    return size;
}

void tinyalsa_wrapper_close(sink_handle_t handle)
{
    struct pcm *pcm = (struct pcm *)handle;
    pcm_close(pcm);
}
