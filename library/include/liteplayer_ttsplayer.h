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

#ifndef _LITEPLAYER_TTSPLAYER_H_
#define _LITEPLAYER_TTSPLAYER_H_

#include <stdbool.h>
#include "liteplayer_adapter.h"
#include "liteplayer_main.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_TTSPLAYER_RINGBUF_SIZE (1024*32)

#define DEFAULT_TTSPLAYER_CFG() {\
    .ringbuf_size = DEFAULT_TTSPLAYER_RINGBUF_SIZE,\
}

struct ttsplayer_cfg {
    int ringbuf_size;
};

typedef struct ttsplayer *ttsplayer_handle_t;

ttsplayer_handle_t ttsplayer_create(struct ttsplayer_cfg *cfg);

int ttsplayer_register_sink_wrapper(ttsplayer_handle_t handle, struct sink_wrapper *wrapper);

int ttsplayer_register_state_listener(ttsplayer_handle_t handle, liteplayer_state_cb listener, void *listener_priv);

int ttsplayer_prepare_async(ttsplayer_handle_t handle);

int ttsplayer_write(ttsplayer_handle_t handle, char *buffer, int size, bool final);

int ttsplayer_start(ttsplayer_handle_t handle);

int ttsplayer_stop(ttsplayer_handle_t handle);

int ttsplayer_reset(ttsplayer_handle_t handle);

void ttsplayer_destroy(ttsplayer_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // _LITEPLAYER_TTSPLAYER_H_
