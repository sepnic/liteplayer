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
#include <string.h>
#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "sink_esp32_i2s_wrapper.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "driver/i2s.h"

#define TAG "[liteplayer]esp32_i2s_out"

#define I2S_PORT  ( I2S_NUM_0 )

const char *esp32_i2s_out_wrapper_name()
{
    return "esp32_i2s_out";
}

sink_handle_t esp32_i2s_out_wrapper_open(int samplerate, int channels, int bits, void *priv_data)
{
    OS_LOGD(TAG, "Opening esp32_i2s_out: samplerate=%d, channels=%d, bits=%d", samplerate, channels, bits);
    if (i2s_set_clk(I2S_PORT, samplerate, bits, channels) != 0) {
        OS_LOGE(TAG, "i2s_set_clk failed");
        return NULL;
    }
    return (sink_handle_t)0xffff;
}

int esp32_i2s_out_wrapper_write(sink_handle_t handle, char *buffer, int size)
{
    //OS_LOGD(TAG, "Writing esp32_i2s_out: size=%d", size);
    size_t bytes_written = 0;
    if (i2s_write(I2S_PORT, buffer, size, &bytes_written, 0x7fffffff) != 0) {
        OS_LOGE(TAG, "i2s_write failed");
        return -1;
    }
    return bytes_written;
}

void esp32_i2s_out_wrapper_close(sink_handle_t handle)
{
    OS_LOGD(TAG, "closing esp32_i2s_out");
    i2s_stop(I2S_PORT);
}
