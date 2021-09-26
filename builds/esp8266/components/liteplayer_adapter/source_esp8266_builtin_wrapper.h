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

#ifndef _LITEPLAYER_ADAPTER_ESP8266_BUILTIN_WRAPPER_H_
#define _LITEPLAYER_ADAPTER_ESP8266_BUILTIN_WRAPPER_H_

#include "liteplayer_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *esp8266_builtin_wrapper_url_protocol();

source_handle_t esp8266_builtin_wrapper_open(const char *url, long long content_pos, void *priv_data);

int esp8266_builtin_wrapper_read(source_handle_t handle, char *buffer, int size);

long long esp8266_builtin_wrapper_content_pos(source_handle_t handle);

long long esp8266_builtin_wrapper_content_len(source_handle_t handle);

int esp8266_builtin_wrapper_seek(source_handle_t handle, long offset);

void esp8266_builtin_wrapper_close(source_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // _LITEPLAYER_ADAPTER_ESP8266_BUILTIN_WRAPPER_H_
