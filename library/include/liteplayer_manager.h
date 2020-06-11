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

#ifndef _LITEPLAYER_MANAGER_H_
#define _LITEPLAYER_MANAGER_H_

#include "liteplayer_adapter.h"
#include "liteplayer_main.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct liteplayer_mngr *liteplayer_mngr_handle_t;

liteplayer_mngr_handle_t liteplayer_mngr_create();

int liteplayer_mngr_register_file_wrapper(liteplayer_mngr_handle_t mngr, struct file_wrapper *file_ops);

int liteplayer_mngr_register_http_wrapper(liteplayer_mngr_handle_t mngr, struct http_wrapper *http_ops);

int liteplayer_mngr_register_sink_wrapper(liteplayer_mngr_handle_t mngr, struct sink_wrapper *sink_ops);

int liteplayer_mngr_register_state_listener(liteplayer_mngr_handle_t mngr, liteplayer_state_cb listener, void *listener_priv);

int liteplayer_mngr_set_data_source(liteplayer_mngr_handle_t mngr, const char *url, int threshold_ms);

int liteplayer_mngr_prepare_async(liteplayer_mngr_handle_t mngr);

int liteplayer_mngr_write(liteplayer_mngr_handle_t mngr, char *data, int size, bool final);

int liteplayer_mngr_start(liteplayer_mngr_handle_t mngr);

int liteplayer_mngr_pause(liteplayer_mngr_handle_t mngr);

int liteplayer_mngr_resume(liteplayer_mngr_handle_t mngr);

int liteplayer_mngr_seek(liteplayer_mngr_handle_t mngr, int msec);

int liteplayer_mngr_next(liteplayer_mngr_handle_t mngr);

int liteplayer_mngr_prev(liteplayer_mngr_handle_t mngr);

int liteplayer_mngr_set_single_looping(liteplayer_mngr_handle_t mngr, bool enable);

int liteplayer_mngr_stop(liteplayer_mngr_handle_t mngr);

int liteplayer_mngr_reset(liteplayer_mngr_handle_t mngr);

int liteplayer_mngr_get_available_size(liteplayer_mngr_handle_t mngr);

int liteplayer_mngr_get_position(liteplayer_mngr_handle_t mngr, int *msec);

int liteplayer_mngr_get_duration(liteplayer_mngr_handle_t mngr, int *msec);

void liteplayer_mngr_destroy(liteplayer_mngr_handle_t mngr);

#ifdef __cplusplus
}
#endif

#endif
