// Copyright (c) 2019-2021 Qinglong<sysu.zqlong@gmail.com>
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

#ifndef _LITEPLAYER_DEBUG_H_
#define _LITEPLAYER_DEBUG_H_

#ifdef __cplusplus
extern "C" {
#endif

struct socketupload;
typedef struct socketupload *socketupload_handle_t;

struct socketupload {
    int (*start)(socketupload_handle_t self, const char *server_addr, int server_port);
    int (*fill_data)(socketupload_handle_t self, char *data, int size);
    void (*stop)(socketupload_handle_t self);
    void (*destroy)(socketupload_handle_t self);
};

socketupload_handle_t socketupload_init(int buffer_size);

#ifdef __cplusplus
}
#endif

#endif // _LITEPLAYER_DEBUG_H_
