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
#include <string.h>

#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "source_esp8266_builtin_wrapper.h"

#define TAG "[liteplayer]esp8266_builtin"

const char *esp8266_builtin_wrapper_url_protocol()
{
    return "flash";
}

source_handle_t esp8266_builtin_wrapper_open(const char *url, long long content_pos, void *priv_data)
{
    return NULL;
}

int esp8266_builtin_wrapper_read(source_handle_t handle, char *buffer, int size)
{
    return -1;
}

long long esp8266_builtin_wrapper_content_pos(source_handle_t handle)
{
    return 0;
}

long long esp8266_builtin_wrapper_content_len(source_handle_t handle)
{
    return 0;
}

int esp8266_builtin_wrapper_seek(source_handle_t handle, long offset)
{
    return -1;
}

void esp8266_builtin_wrapper_close(source_handle_t handle)
{

}
