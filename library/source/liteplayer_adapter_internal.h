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

#ifndef _LITEPLAYER_ADAPTER_INTERNAL_H_
#define _LITEPLAYER_ADAPTER_INTERNAL_H_

#include "liteplayer_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

struct liteplayer_adapter;
typedef struct liteplayer_adapter *liteplayer_adapter_handle_t;

struct liteplayer_adapter {
    int                    (*add_source_wrapper)(liteplayer_adapter_handle_t self, struct source_wrapper *wrapper);
    struct source_wrapper *(*find_source_wrapper)(liteplayer_adapter_handle_t self, const char *url);
    int                    (*add_sink_wrapper)(liteplayer_adapter_handle_t self, struct sink_wrapper *wrapper);
    struct sink_wrapper   *(*find_sink_wrapper)(liteplayer_adapter_handle_t self, const char *name);
    void                   (*destory)(liteplayer_adapter_handle_t self);
};

liteplayer_adapter_handle_t liteplayer_adapter_init();

#ifdef __cplusplus
}
#endif

#endif // _LITEPLAYER_ADAPTER_INTERNAL_H_
