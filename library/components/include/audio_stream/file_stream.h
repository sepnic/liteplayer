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

#ifndef _FILE_STREAM_H_
#define _FILE_STREAM_H_

#include "msgutils/os_thread.h"
#include "esp_adf/audio_element.h"
#include "esp_adf/audio_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum file_mode {
    FILE_WRITE = 0,
    FILE_READ = 1,
} file_mode_t;

typedef void *file_handle_t;

/**
 * @brief   File Stream configurations, if any entry is zero then the configuration will be set to default values
 */
typedef struct {
    audio_stream_type_t type;        /*!< Stream type */
    int              task_prio;      /*!< Task priority (based on freeRTOS priority) */
    int              task_stack;     /*!< Task stack size */
    int              out_rb_size;    /*!< Size of output ringbuffer */
    int              buf_sz;         /*!< Audio Element Buffer size */

    const char      *url;
    void            *file_priv;
    file_handle_t  (*file_open)(const char *url, file_mode_t mode, long long content_pos, void *file_priv);
    // File read callback return:
    //  >0: bytes read; <0: error occur; =0: read done
    int            (*file_read)(file_handle_t handle, char *buffer, int size);
    int            (*file_write)(file_handle_t handle, char *buffer, int size);
    int            (*file_seek)(file_handle_t handle, long offset);
    void           (*file_close)(file_handle_t handle);
} file_stream_cfg_t;

#define FILE_STREAM_TASK_PRIO           (OS_THREAD_PRIO_NORMAL)
#define FILE_STREAM_TASK_STACK          (4096)
#define FILE_STREAM_RINGBUFFER_SIZE     (64 * 1024)
#define FILE_STREAM_BUF_SIZE            (2048)

#define FILE_STREAM_CFG_DEFAULT() {             \
    .type = AUDIO_STREAM_READER,                 \
    .task_prio = FILE_STREAM_TASK_PRIO,         \
    .task_stack = FILE_STREAM_TASK_STACK,       \
    .out_rb_size = FILE_STREAM_RINGBUFFER_SIZE, \
    .buf_sz = FILE_STREAM_BUF_SIZE,             \
    .url = NULL,                                 \
    .file_priv = NULL,                          \
    .file_open = NULL,                          \
    .file_read = NULL,                          \
    .file_write = NULL,                         \
    .file_seek = NULL,                          \
    .file_close = NULL,                         \
}

/**
 * @brief      Create a handle to an Audio Element to stream data from file to another Element
 *             or get data from other elements written to file, depending on the configuration
 *             the stream type, either AUDIO_STREAM_READER or AUDIO_STREAM_WRITER.
 *
 * @param      config  The configuration
 *
 * @return     The Audio Element handle
 */
audio_element_handle_t file_stream_init(file_stream_cfg_t *config);

#ifdef __cplusplus
}
#endif

#endif
