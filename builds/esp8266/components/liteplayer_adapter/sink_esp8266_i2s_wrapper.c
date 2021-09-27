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
#include "sink_esp8266_i2s_wrapper.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "esp8266/pin_mux_register.h"

#define TAG "[liteplayer]esp8266-i2s"

#define I2S_DMA_BUF_LEN 256
#define I2S_DMA_BUF_CNT 4

const char *esp8266_i2s_wrapper_name()
{
    return "esp8266_i2s";
}

sink_handle_t esp8266_i2s_wrapper_open(int samplerate, int channels, int bits, void *priv_data)
{
    OS_LOGD(TAG, "Opening esp8266_i2s: samplerate=%d, channels=%d, bits=%d", samplerate, channels, bits);

    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = samplerate,
        .bits_per_sample = bits,
        .channel_format = (channels == 1) ? I2S_CHANNEL_FMT_ONLY_LEFT : I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB,
        .dma_buf_count = I2S_DMA_BUF_CNT,
        .dma_buf_len = I2S_DMA_BUF_LEN,
        .tx_desc_auto_clear = true,
    };
    i2s_pin_config_t pin_config = {
        .bck_o_en = 1,
        .ws_o_en = 1,
        .data_out_en = 1,
        .data_in_en = 0,
    };
    if (i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL) != 0)
        return NULL;
    if (i2s_set_pin(I2S_NUM_0, &pin_config) != 0)
        goto open_fail;
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U, FUNC_CLK_XTAL); // mclk

    if (i2s_set_clk(I2S_NUM_0, samplerate, bits, channels) != 0)
        goto open_fail;

    return (sink_handle_t)0xffff;

open_fail:
    i2s_driver_uninstall(I2S_NUM_0);
    return NULL;
}

int esp8266_i2s_wrapper_write(sink_handle_t handle, char *buffer, int size)
{
    //OS_LOGD(TAG, "Writing esp8266_i2s: size=%d", size);
    size_t i2s_bytes_write = 0;
    if (i2s_write(I2S_NUM_0, buffer, size, &i2s_bytes_write, 0x7FFFFFFF) != 0)
        return -1;
    //OS_LOGD(TAG, "Written esp8266_i2s: size=%d", (int)i2s_bytes_write);
    return i2s_bytes_write;
}

void esp8266_i2s_wrapper_close(sink_handle_t handle)
{
    OS_LOGD(TAG, "closing esp8266_i2s");
    i2s_driver_uninstall(I2S_NUM_0);
}
