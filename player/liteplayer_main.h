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

#ifndef _LITEPLAYER_MAIN_H_
#define _LITEPLAYER_MAIN_H_

#include "liteplayer_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum liteplayer_state {
    LITEPLAYER_IDLE            = 0x00,
    LITEPLAYER_INITED          = 0x01,
    LITEPLAYER_PREPARED        = 0x02,
    LITEPLAYER_STARTED         = 0x03,
    LITEPLAYER_PAUSED          = 0x04,
    LITEPLAYER_NEARLYCOMPLETED = 0x05,
    LITEPLAYER_COMPLETED       = 0x06,
    LITEPLAYER_STOPPED         = 0x07,
    LITEPLAYER_ERROR           = 0x09,
} liteplayer_state_t;

typedef int (*liteplayer_state_cb)(liteplayer_state_t state, int errcode, void *priv);

typedef struct liteplayer *liteplayer_handle_t;

liteplayer_handle_t liteplayer_create();

int liteplayer_register_fatfs_wrapper(liteplayer_handle_t handle, fatfs_wrapper_t *fatfs_wrapper);

int liteplayer_register_http_wrapper(liteplayer_handle_t handle, http_wrapper_t *http_wrapper);

int liteplayer_register_alsa_wrapper(liteplayer_handle_t handle, alsa_wrapper_t *alsa_wrapper);

int liteplayer_register_state_listener(liteplayer_handle_t handle, liteplayer_state_cb listener, void *listener_priv);

int liteplayer_set_data_source(liteplayer_handle_t handle, const char *url);

int liteplayer_prepare(liteplayer_handle_t handle);

int liteplayer_prepare_async(liteplayer_handle_t handle);

int liteplayer_write(liteplayer_handle_t handle, char *data, int size, bool final);

int liteplayer_start(liteplayer_handle_t handle);

int liteplayer_pause(liteplayer_handle_t handle);

int liteplayer_resume(liteplayer_handle_t handle);

int liteplayer_stop(liteplayer_handle_t handle);

int liteplayer_reset(liteplayer_handle_t handle);

int liteplayer_get_available_size(liteplayer_handle_t handle);

int liteplayer_get_position(liteplayer_handle_t handle, long long *msec);

int liteplayer_get_duration(liteplayer_handle_t handle, long long *msec);

void liteplayer_destroy(liteplayer_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif
