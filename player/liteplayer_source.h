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

#ifndef _LITEPLAYER_MEDIASOURCE_H_
#define _LITEPLAYER_MEDIASOURCE_H_

#include "msgutils/ringbuf.h"
#include "liteplayer_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum media_source_type {
    MEDIA_SOURCE_UNKNOWN,
    MEDIA_SOURCE_STREAM,
    MEDIA_SOURCE_HTTP,
    MEDIA_SOURCE_FILE,
} media_source_type_t;

typedef enum media_source_state {
    MEDIA_SOURCE_READ_SUCCEED,
    MEDIA_SOURCE_READ_FAILED,
    MEDIA_SOURCE_READ_DONE,
    MEDIA_SOURCE_WRITE_SUCCEED,
    MEDIA_SOURCE_WRITE_FAILED,
    MEDIA_SOURCE_WRITE_DONE,
} media_source_state_t;

typedef void (*media_source_state_cb)(media_source_state_t state, void *priv);

typedef struct media_source_info {
    const char *url;
    media_source_type_t source_type;
    http_wrapper_t http_wrapper;
    file_wrapper_t file_wrapper;
    long long content_pos;
} media_source_info_t;

typedef void *media_source_handle_t;

media_source_handle_t media_source_start(media_source_info_t *info,
                                         ringbuf_handle_t rb,
                                         media_source_state_cb listener,
                                         void *listener_priv);

void media_source_stop(media_source_handle_t handle);

int m3u_get_first_url(media_source_info_t *info, char *buf, int buf_size);

#ifdef __cplusplus
}
#endif

#endif
