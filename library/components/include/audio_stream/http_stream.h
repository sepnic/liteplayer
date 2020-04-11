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

#ifndef _HTTP_STREAM_WRITER_H_
#define _HTTP_STREAM_WRITER_H_

#include <stdint.h>
#include "msgutils/os_thread.h"
#include "esp_adf/audio_element.h"
#include "esp_adf/audio_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *http_handle_t;

/**
 * @brief      HTTP Stream configurations
 *             Default value will be used if any entry is zero
 */
typedef struct {
    int             task_prio;      /*!< Task priority (based on freeRTOS priority) */
    int             task_stack;     /*!< Task stack size */
    int             out_rb_size;    /*!< Size of output ringbuffer */
    int             buf_sz;         /*!< Audio Element Buffer size */

    const char     *url;
    void           *http_priv;
    http_handle_t (*http_open)(const char *url, long long content_pos, void *http_priv);
    // Http read callback return:
    //  >0: bytes read; <0: error occur; =0: read done
    int           (*http_read)(http_handle_t handle, char *buffer, int size);
    void          (*http_close)(http_handle_t handle);
} http_stream_cfg_t;

#define HTTP_STREAM_TASK_PRIO           (OS_THREAD_PRIO_SOFT_REALTIME)
#define HTTP_STREAM_TASK_STACK          (4 * 1024)
#define HTTP_STREAM_RINGBUFFER_SIZE     (128 * 1024)
#define HTTP_STREAM_BUFFER_SIZE         (1024)

#define HTTP_STREAM_CFG_DEFAULT() {              \
    .task_prio = HTTP_STREAM_TASK_PRIO,          \
    .task_stack = HTTP_STREAM_TASK_STACK,        \
    .out_rb_size = HTTP_STREAM_RINGBUFFER_SIZE,  \
    .buf_sz = HTTP_STREAM_BUFFER_SIZE,           \
    .url = NULL,                                 \
    .http_priv = NULL,                           \
    .http_open = NULL,                           \
    .http_read = NULL,                           \
    .http_close = NULL,                          \
}

/**
 * @brief      Create a handle to an Audio Element to stream data from HTTP to another Element
 *             or get data from other elements sent to HTTP
 * @param      config  The configuration
 *
 * @return     The Audio Element handle
 */
audio_element_handle_t http_stream_init(http_stream_cfg_t *config);

#ifdef __cplusplus
}
#endif

#endif
