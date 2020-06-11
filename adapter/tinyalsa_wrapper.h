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

#ifndef _TINYALSA_WRAPPER_H_
#define _TINYALSA_WRAPPER_H_

#include "liteplayer_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

sink_handle_t tinyalsa_wrapper_open(int samplerate, int channels, void *sink_priv);

int tinyalsa_wrapper_write(sink_handle_t handle, char *buffer, int size);

void tinyalsa_wrapper_close(sink_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif
