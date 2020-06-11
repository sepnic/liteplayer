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

#ifndef _LITEPLAYER_ADAPTER_H_
#define _LITEPLAYER_ADAPTER_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef void *file_handle_t;
typedef void *http_handle_t;
typedef void *sink_handle_t;

struct file_wrapper {
    void            *file_priv;
    file_handle_t  (*open)(const char *url, long long content_pos, void *file_priv);
    int            (*read)(file_handle_t handle, char *buffer, int size);
    long long      (*filesize)(file_handle_t handle);
    int            (*seek)(file_handle_t handle, long offset);
    void           (*close)(file_handle_t handle);
};

struct http_wrapper {
    void            *http_priv;
    http_handle_t  (*open)(const char *url, long long content_pos, void *http_priv);
    int            (*read)(http_handle_t handle, char *buffer, int size);
    long long      (*filesize)(http_handle_t handle);
    int            (*seek)(http_handle_t handle, long offset);
    void           (*close)(http_handle_t handle);
};

struct sink_wrapper {
    void            *sink_priv;
    sink_handle_t  (*open)(int samplerate, int channels, void *sink_priv);
    int            (*write)(sink_handle_t handle, char *buffer, int size);
    void           (*close)(sink_handle_t handle);
};

#ifdef __cplusplus
}
#endif

#endif
