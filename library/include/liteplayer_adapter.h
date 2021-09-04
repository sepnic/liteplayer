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

#ifndef _LITEPLAYER_ADAPTER_H_
#define _LITEPLAYER_ADAPTER_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *source_handle_t;
typedef void *sink_handle_t;

struct source_wrapper {
    bool            async_mode; // for network stream, it's better to set async mode
    int             ringbuf_size; // only use for async mode
    void            *priv_data;
    const char *    (*procotol)(); // "http", "tts", "rtsp", "rtmp", "file"
    source_handle_t (*open)(const char *url, long long content_pos, void *priv_data);
    int             (*read)(source_handle_t handle, char *buffer, int size);//note: 0<=ret<size means eof
    long long       (*filesize)(source_handle_t handle);
    int             (*seek)(source_handle_t handle, long offset);
    void            (*close)(source_handle_t handle);
};

struct sink_wrapper {
    void           *priv_data;
    const char *   (*name)(); // "alsa", "wave", "opensles", "audiotrack"
    sink_handle_t  (*open)(int samplerate, int channels, int bits, void *priv_data);
    int            (*write)(sink_handle_t handle, char *buffer, int size);//return actual written size
    void           (*close)(sink_handle_t handle);
};

#ifdef __cplusplus
}
#endif

#endif // _LITEPLAYER_ADAPTER_H_
