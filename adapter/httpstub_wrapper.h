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

#ifndef _HTTPSTUB_WRAPPER_H_
#define _HTTPSTUB_WRAPPER_H_

#include "audio_stream/http_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

http_handle_t httpstub_wrapper_open(const char *url, long long content_pos, void *priv);

int httpstub_wrapper_read(http_handle_t handle, char *buffer, int size);

long long httpstub_wrapper_filesize(http_handle_t handle);

int httpstub_wrapper_seek(http_handle_t handle, long offset);

void httpstub_wrapper_close(http_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif
