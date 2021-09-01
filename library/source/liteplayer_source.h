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

#ifndef _LITEPLAYER_MEDIASOURCE_H_
#define _LITEPLAYER_MEDIASOURCE_H_

#include "cutils/ringbuf.h"
#include "liteplayer_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

enum media_source_state {
    MEDIA_SOURCE_READ_SUCCEED,
    MEDIA_SOURCE_READ_FAILED,
    MEDIA_SOURCE_READ_DONE,
    MEDIA_SOURCE_WRITE_SUCCEED,
    MEDIA_SOURCE_WRITE_FAILED,
    MEDIA_SOURCE_WRITE_DONE,
    MEDIA_SOURCE_REACH_THRESHOLD,
};

typedef void (*media_source_state_cb)(enum media_source_state state, void *priv);

struct media_source_info {
    const char *url;
    struct source_wrapper *source_ops;
    long long content_pos;
    int threshold_size;
};

typedef void *media_source_handle_t;

media_source_handle_t media_source_start(struct media_source_info *info,
                                         ringbuf_handle rb,
                                         media_source_state_cb listener,
                                         void *listener_priv);

void media_source_stop(media_source_handle_t handle);

int m3u_get_first_url(struct media_source_info *info, char *buf, int buf_size);

#ifdef __cplusplus
}
#endif

#endif
