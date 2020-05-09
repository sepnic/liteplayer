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

#ifndef _WAV_DECODER_H_
#define _WAV_DECODER_H_

#include "msgutils/os_thread.h"
#include "esp_adf/audio_element.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * brief      WAV Decoder configurations
 */
struct wav_decoder_cfg {
    int out_rb_size;    /*!< Size of output ringbuffer */
    int task_stack;     /*!< Task stack size */
    int task_prio;      /*!< Task priority (based on freeRTOS priority) */ 
};

#define WAV_DECODER_TASK_PRIO           (OS_THREAD_PRIO_NORMAL)
#define WAV_DECODER_TASK_STACK          (4 * 1024)
#define WAV_DECODER_RINGBUFFER_SIZE     (8 * 1024)
#define WAV_DECODER_BUFFER_SIZE         (512)

#define DEFAULT_WAV_DECODER_CONFIG() {\
    .task_prio          = WAV_DECODER_TASK_PRIO,\
    .task_stack         = WAV_DECODER_TASK_STACK,\
    .out_rb_size        = WAV_DECODER_RINGBUFFER_SIZE,\
}

/**
 * @brief      Create an Audio Element handle to decode incoming WAV data
 *
 * @param      config  The configuration
 *
 * @return     The audio element handle
 */
audio_element_handle_t wav_decoder_init(struct wav_decoder_cfg *config);


#ifdef __cplusplus
}
#endif

#endif
