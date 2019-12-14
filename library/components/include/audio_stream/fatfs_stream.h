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

#ifndef _FATFS_STREAM_H_
#define _FATFS_STREAM_H_

#include "msgutils/os_thread.h"
#include "esp_adf/audio_element.h"
#include "esp_adf/audio_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum fatfs_mode {
    FATFS_WRITE = 0,
    FATFS_READ = 1,
} fatfs_mode_t;

typedef void *fatfs_handle_t;

/**
 * @brief   FATFS Stream configurations, if any entry is zero then the configuration will be set to default values
 */
typedef struct {
    audio_stream_type_t type;           /*!< Stream type */
    int                 task_prio;      /*!< Task priority (based on freeRTOS priority) */
    int                 task_stack;     /*!< Task stack size */
    int                 out_rb_size;    /*!< Size of output ringbuffer */
    int                 buf_sz;         /*!< Audio Element Buffer size */

    const char      *url;
    void            *fatfs_priv;
    fatfs_handle_t (*fatfs_open)(const char *url, fatfs_mode_t mode, long long content_pos, void *fatfs_priv);
    // File read callback return:
    //  >0: bytes read; <0: error occur; =0: read done
    int            (*fatfs_read)(fatfs_handle_t handle, char *buffer, int size);
    int            (*fatfs_write)(fatfs_handle_t handle, char *buffer, int size);
    int            (*fatfs_seek)(fatfs_handle_t handle, long offset);
    void           (*fatfs_close)(fatfs_handle_t handle);
} fatfs_stream_cfg_t;

#define FATFS_STREAM_TASK_PRIO           (OS_THREAD_PRIO_NORMAL)
#define FATFS_STREAM_TASK_STACK          (4096)
#define FATFS_STREAM_RINGBUFFER_SIZE     (64 * 1024)
#define FATFS_STREAM_BUF_SIZE            (2048)

#define FATFS_STREAM_CFG_DEFAULT() {             \
    .type = AUDIO_STREAM_READER,                 \
    .task_prio = FATFS_STREAM_TASK_PRIO,         \
    .task_stack = FATFS_STREAM_TASK_STACK,       \
    .out_rb_size = FATFS_STREAM_RINGBUFFER_SIZE, \
    .buf_sz = FATFS_STREAM_BUF_SIZE,             \
    .url = NULL,                                 \
    .fatfs_priv = NULL,                          \
    .fatfs_open = NULL,                          \
    .fatfs_read = NULL,                          \
    .fatfs_write = NULL,                         \
    .fatfs_seek = NULL,                          \
    .fatfs_close = NULL,                         \
}

/**
 * @brief      Create a handle to an Audio Element to stream data from FatFs to another Element
 *             or get data from other elements written to FatFs, depending on the configuration
 *             the stream type, either AUDIO_STREAM_READER or AUDIO_STREAM_WRITER.
 *
 * @param      config  The configuration
 *
 * @return     The Audio Element handle
 */
audio_element_handle_t fatfs_stream_init(fatfs_stream_cfg_t *config);

#ifdef __cplusplus
}
#endif

#endif
