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

#ifndef _SINK_STREAM_H_
#define _SINK_STREAM_H_

#include "osal/os_thread.h"
#include "esp_adf/audio_element.h"
#include "esp_adf/audio_common.h"
#include "liteplayer_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

//typedef void *sink_handle_t;

/**
 * @brief      Sink stream configurations
 *             Default value will be used if any entry is zero
 */
struct sink_stream_cfg {
    int             task_prio;      /*!< Task priority (based on freeRTOS priority) */
    int             task_stack;     /*!< Task stack size */
    int             out_rb_size;    /*!< Size of output ringbuffer */
    int             buf_sz;         /*!< Audio Element Buffer size */

    int             in_samplerate;
    int             in_channels;
    int             out_samplerate;
    int             out_channels;
    void           *sink_priv;
    sink_handle_t (*sink_open)(int samplerate, int channels, void *sink_priv);
    int           (*sink_write)(sink_handle_t handle, char *buffer, int size);
    void          (*sink_close)(sink_handle_t handle);
};

#define SINK_STREAM_TASK_PRIO            (OS_THREAD_PRIO_REALTIME)
#define SINK_STREAM_TASK_STACK           (4 * 1024)
#define SINK_STREAM_RINGBUFFER_SIZE      (8 * 1024)
#define SINK_STREAM_BUF_SIZE             (4 * 1024)
#define SINK_STREAM_SAMPLE_RATE          (44100)
#define SINK_STREAM_CHANNELS             (2)

#define SINK_STREAM_CFG_DEFAULT() {                 \
    .task_prio      = SINK_STREAM_TASK_PRIO,        \
    .task_stack     = SINK_STREAM_TASK_STACK,       \
    .out_rb_size    = SINK_STREAM_RINGBUFFER_SIZE,  \
    .buf_sz         = SINK_STREAM_BUF_SIZE,         \
    .sink_priv      = NULL,                         \
    .out_samplerate = SINK_STREAM_SAMPLE_RATE,      \
    .out_channels   = SINK_STREAM_CHANNELS,         \
}

/**
 * @brief      Create a handle to an Audio Element to stream data from sink to another Element
 *             or get data from other elements sent to sink
 * @param      config  The configuration
 *
 * @return     The Audio Element handle
 */
audio_element_handle_t sink_stream_init(struct sink_stream_cfg *config);

#ifdef __cplusplus
}
#endif

#endif
