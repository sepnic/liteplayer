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

#define TAG "[liteplayer]esp8266-i2s"

const char *esp8266_i2s_wrapper_name()
{
    return "esp8266_i2s";
}

sink_handle_t esp8266_i2s_wrapper_open(int samplerate, int channels, int bits, void *priv_data)
{
    OS_LOGD(TAG, "Opening esp8266_i2s: samplerate=%d, channels=%d, bits=%d", samplerate, channels, bits);
    return 1;
}

int esp8266_i2s_wrapper_write(sink_handle_t handle, char *buffer, int size)
{
    OS_LOGD(TAG, "Writing esp8266_i2s: size=%d", size);
    return size;
}

void esp8266_i2s_wrapper_close(sink_handle_t handle)
{
    OS_LOGD(TAG, "closing esp8266_i2s");
}
