/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

 /** Copyright (c) 2019-2021 Qinglong <sysu.zqlong@gmail.com> */

#include <string.h>
#include "osal/os_thread.h"
#include "cutils/log_helper.h"
#include "cutils/memory_helper.h"

#include "driver/gpio.h"
#include "audio_hal.h"

static const char *TAG = "AUDIO_HAL";

#define AUDIO_HAL_VOL_DEFAULT 50

#define AUDIO_HAL_CHECK_NULL(a, format, b, ...) \
    if ((a) == 0) { \
        OS_LOGE(TAG, format, ##__VA_ARGS__); \
        return b;\
    }

audio_hal_handle_t audio_hal_init(audio_hal_codec_config_t *audio_hal_conf, audio_hal_func_t *audio_hal_func)
{
    int ret = 0;
    audio_hal_handle_t audio_hal = (audio_hal_handle_t) OS_CALLOC(1, sizeof(audio_hal_func_t));
    AUDIO_MEM_CHECK(TAG, audio_hal, return NULL);
    memcpy(audio_hal, audio_hal_func, sizeof(audio_hal_func_t));
    audio_hal->audio_hal_lock = os_mutex_create();

    AUDIO_MEM_CHECK(TAG, audio_hal->audio_hal_lock, {
        OS_FREE(audio_hal);
        return NULL;
    });
    os_mutex_lock(audio_hal->audio_hal_lock);
    ret  = audio_hal->audio_codec_initialize(audio_hal_conf);
    if (ret != 0) {
        OS_FREE(audio_hal);
        if (audio_hal_func->handle) {
            return audio_hal_func->handle;
        } else {
            OS_LOGE(TAG, "codec init failed!");
            return NULL;
        }
    }
    ret |= audio_hal->audio_codec_config_iface(audio_hal_conf->codec_mode, &audio_hal_conf->i2s_iface);
    ret |= audio_hal->audio_codec_set_volume(AUDIO_HAL_VOL_DEFAULT);
    audio_hal->handle = audio_hal;
    audio_hal_func->handle = audio_hal;
    os_mutex_unlock(audio_hal->audio_hal_lock);
    return audio_hal;
}

int audio_hal_deinit(audio_hal_handle_t audio_hal)
{
    int ret;
    AUDIO_HAL_CHECK_NULL(audio_hal, "audio_hal handle is null", -1);
    os_mutex_destroy(audio_hal->audio_hal_lock);
    ret = audio_hal->audio_codec_deinitialize();
    audio_hal->audio_hal_lock = NULL;
    audio_hal->handle = NULL;
    OS_FREE(audio_hal);
    audio_hal = NULL;
    return ret;
}

int audio_hal_ctrl_codec(audio_hal_handle_t audio_hal, audio_hal_codec_mode_t mode, audio_hal_ctrl_t audio_hal_state)
{
    int ret;
    AUDIO_HAL_CHECK_NULL(audio_hal, "audio_hal handle is null", -1);
    os_mutex_lock(audio_hal->audio_hal_lock);
    OS_LOGI(TAG, "Codec mode is %d, Ctrl:%d", mode, audio_hal_state);
    ret = audio_hal->audio_codec_ctrl(mode, audio_hal_state);
    os_mutex_unlock(audio_hal->audio_hal_lock);
    return ret;
}

int audio_hal_codec_iface_config(audio_hal_handle_t audio_hal, audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface)
{
    int ret = 0;
    AUDIO_HAL_CHECK_NULL(audio_hal, "audio_hal handle is null", -1);
    AUDIO_HAL_CHECK_NULL(iface, "Get volume para is null", -1);
    os_mutex_lock(audio_hal->audio_hal_lock);
    ret = audio_hal->audio_codec_config_iface(mode, iface);
    os_mutex_unlock(audio_hal->audio_hal_lock);
    return ret;
}

int audio_hal_set_mute(audio_hal_handle_t audio_hal, bool mute)
{
    int ret;
    AUDIO_HAL_CHECK_NULL(audio_hal, "audio_hal handle is null", -1);
    os_mutex_lock(audio_hal->audio_hal_lock);
    ret = audio_hal->audio_codec_set_mute(mute);
    os_mutex_unlock(audio_hal->audio_hal_lock);
    return ret;
}

int audio_hal_set_volume(audio_hal_handle_t audio_hal, int volume)
{
    int ret;
    AUDIO_HAL_CHECK_NULL(audio_hal, "audio_hal handle is null", -1);
    os_mutex_lock(audio_hal->audio_hal_lock);
    ret = audio_hal->audio_codec_set_volume(volume);
    os_mutex_unlock(audio_hal->audio_hal_lock);
    return ret;
}

int audio_hal_get_volume(audio_hal_handle_t audio_hal, int *volume)
{
    int ret;
    AUDIO_HAL_CHECK_NULL(audio_hal, "audio_hal handle is null", -1);
    AUDIO_HAL_CHECK_NULL(volume, "Get volume para is null", -1);
    os_mutex_lock(audio_hal->audio_hal_lock);
    ret = audio_hal->audio_codec_get_volume(volume);
    os_mutex_unlock(audio_hal->audio_hal_lock);
    return ret;
}
