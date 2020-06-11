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

#ifndef _LITEPLAYER_DEBUG_H_
#define _LITEPLAYER_DEBUG_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef void *socket_upload_handle_t;

socket_upload_handle_t socket_upload_start(const char *server_addr, int server_port);

int socket_upload_fill_data(socket_upload_handle_t handle, char *data, int size);

void socket_upload_stop(socket_upload_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif
