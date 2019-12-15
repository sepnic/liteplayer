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

#include "esp_adf/esp_log.h"
#include "alsastub_wrapper.h"

#define TAG "alsawrapper"

#define ALSA_OUT_FILE "alsa_out.pcm"

alsa_handle_t alsastub_wrapper_open(int samplerate, int channels, void *priv)
{
    ESP_LOGI(TAG, "Open alsa device: samplerate=%d, channels=%d", samplerate, channels);
    FILE *file = fopen(ALSA_OUT_FILE, "wb+");
    return file;
}

int alsastub_wrapper_write(alsa_handle_t handle, char *buffer, int size)
{
    FILE *file = (FILE *)handle;
    return fwrite(buffer, 1, size, file);
}

void alsastub_wrapper_close(alsa_handle_t handle)
{
    FILE *file = (FILE *)handle;
    fflush(file);
    fclose(file);
}
