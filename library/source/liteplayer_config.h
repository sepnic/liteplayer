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

#ifndef _LITEPLAYER_CONFIG_H_
#define _LITEPLAYER_CONFIG_H_

#include "osal/os_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

// media parser definations, core feature
#define DEFAULT_MEDIA_PARSER_TASK_PRIO           ( OS_THREAD_PRIO_HIGH )
#define DEFAULT_MEDIA_PARSER_TASK_STACKSIZE      ( 1024*8 )

// media decoder definations, core feature
#define DEFAULT_MEDIA_DECODER_TASK_PRIO          ( OS_THREAD_PRIO_REALTIME )
#define DEFAULT_MEDIA_DECODER_TASK_STACKSIZE     ( 1024*16 )

// media source definations, core feature
#define DEFAULT_MEDIA_SOURCE_TASK_PRIO           ( OS_THREAD_PRIO_HIGH )
#define DEFAULT_MEDIA_SOURCE_TASK_STACKSIZE      ( 1024*6 )

// playlist player definations, for playlist support
#define DEFAULT_LISTPLAYER_TASK_PRIO             ( OS_THREAD_PRIO_HIGH )
#define DEFAULT_LISTPLAYER_TASK_STACKSIZE        ( 1024*4 )

// socket upload definations, for data dump, see tools/README.md
#define DEFAULT_SOCKET_UPLOAD_START              "GENIE_SOCKET_UPLOAD_START"
#define DEFAULT_SOCKET_UPLOAD_END                "GENIE_SOCKET_UPLOAD_END"
#define DEFAULT_SOCKET_UPLOAD_TASK_PRIO          ( OS_THREAD_PRIO_NORMAL )
#define DEFAULT_SOCKET_UPLOAD_TASK_STACKSIZE     ( 1024*2 )

#ifdef __cplusplus
}
#endif

#endif // _LITEPLAYER_CONFIG_H_
