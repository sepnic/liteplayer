// Copyright (c) 2019-2022 Qinglong<sysu.zqlong@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
