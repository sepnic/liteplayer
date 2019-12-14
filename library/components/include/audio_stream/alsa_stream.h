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

#ifndef _ALSA_STREAM_WRITER_H_
#define _ALSA_STREAM_WRITER_H_

#include "msgutils/os_thread.h"
#include "esp_adf/audio_element.h"
#include "esp_adf/audio_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *alsa_handle_t;

/**
 * @brief      ALSA Stream configurations
 *             Default value will be used if any entry is zero
 */
typedef struct {
    audio_stream_type_t type;           /*!< Type of stream */
    int                 task_prio;      /*!< Task priority (based on freeRTOS priority) */
    int                 task_stack;     /*!< Task stack size */
    int                 out_rb_size;    /*!< Size of output ringbuffer */
    int                 buf_sz;         /*!< Audio Element Buffer size */

    void           *alsa_priv;
    int             out_samplerate;
    int             out_channels;
    alsa_handle_t (*alsa_open)(int samplerate, int channels, void *alsa_priv);
    int           (*alsa_write)(alsa_handle_t handle, char *buffer, int size);
    void          (*alsa_close)(alsa_handle_t handle);
} alsa_stream_cfg_t;

#define ALSA_STREAM_TASK_PRIO            (OS_THREAD_PRIO_HARD_REALTIME)
#define ALSA_STREAM_TASK_STACK           (4 * 1024)
#define ALSA_STREAM_RINGBUFFER_SIZE      (8 * 1024)
#define ALSA_STREAM_BUF_SIZE             (4 * 1024)
#define ALSA_STREAM_SAMPLE_RATE          (44100)
#define ALSA_STREAM_CHANNELS             (2)

#define ALSA_STREAM_CFG_DEFAULT() {                 \
    .type           = AUDIO_STREAM_WRITER,          \
    .task_prio      = ALSA_STREAM_TASK_PRIO,        \
    .task_stack     = ALSA_STREAM_TASK_STACK,       \
    .out_rb_size    = ALSA_STREAM_RINGBUFFER_SIZE,  \
    .buf_sz         = ALSA_STREAM_BUF_SIZE,         \
    .alsa_priv      = NULL,                         \
    .out_samplerate = ALSA_STREAM_SAMPLE_RATE,      \
    .out_channels   = ALSA_STREAM_CHANNELS,         \
}

/**
 * @brief      Create a handle to an Audio Element to stream data from ALSA to another Element
 *             or get data from other elements sent to ALSA, depending on the configuration of stream type
 *             is AUDIO_STREAM_READER or AUDIO_STREAM_WRITER.
 * @param      config  The configuration
 *
 * @return     The Audio Element handle
 */
audio_element_handle_t alsa_stream_init(alsa_stream_cfg_t *config);

#ifdef __cplusplus
}
#endif

#endif
