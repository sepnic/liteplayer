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

#ifndef _LITEPLAYER_DEBUG_H_
#define _LITEPLAYER_DEBUG_H_

#ifdef __cplusplus
extern "C" {
#endif

struct socketupload;
typedef struct socketupload *socketupload_handle_t;

struct socketupload {
    int (*start)(socketupload_handle_t self, const char *server_addr, int server_port);
    int (*fill_data)(socketupload_handle_t self, char *data, int size);
    void (*stop)(socketupload_handle_t self);
    void (*destroy)(socketupload_handle_t self);
};

socketupload_handle_t socketupload_init(int ringbuf_size);

#ifdef __cplusplus
}
#endif

#endif
