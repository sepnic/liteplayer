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

#ifndef _LITEPLAYER_MANAGER_H_
#define _LITEPLAYER_MANAGER_H_

#include "liteplayer_adapter.h"
#include "liteplayer_main.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct liteplayer_manager *liteplayermanager_handle_t;

liteplayermanager_handle_t liteplayermanager_create();

int liteplayermanager_register_source_wrapper(liteplayermanager_handle_t mngr, struct source_wrapper *wrapper);

int liteplayermanager_set_prefered_source_wrapper(liteplayermanager_handle_t handle, struct source_wrapper *wrapper);

int liteplayermanager_register_sink_wrapper(liteplayermanager_handle_t mngr, struct sink_wrapper *wrapper);

int liteplayermanager_set_prefered_sink_wrapper(liteplayermanager_handle_t handle, struct sink_wrapper *wrapper);

int liteplayermanager_register_state_listener(liteplayermanager_handle_t mngr, liteplayer_state_cb listener, void *listener_priv);

int liteplayermanager_set_data_source(liteplayermanager_handle_t mngr, const char *url, int threshold_ms);

int liteplayermanager_prepare_async(liteplayermanager_handle_t mngr);

int liteplayermanager_start(liteplayermanager_handle_t mngr);

int liteplayermanager_pause(liteplayermanager_handle_t mngr);

int liteplayermanager_resume(liteplayermanager_handle_t mngr);

int liteplayermanager_seek(liteplayermanager_handle_t mngr, int msec);

int liteplayermanager_next(liteplayermanager_handle_t mngr);

int liteplayermanager_prev(liteplayermanager_handle_t mngr);

int liteplayermanager_set_single_looping(liteplayermanager_handle_t mngr, bool enable);

int liteplayermanager_stop(liteplayermanager_handle_t mngr);

int liteplayermanager_reset(liteplayermanager_handle_t mngr);

int liteplayermanager_get_position(liteplayermanager_handle_t mngr, int *msec);

int liteplayermanager_get_duration(liteplayermanager_handle_t mngr, int *msec);

void liteplayermanager_destroy(liteplayermanager_handle_t mngr);

#ifdef __cplusplus
}
#endif

#endif
