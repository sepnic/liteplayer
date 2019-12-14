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

#ifndef _AAC_DECODER_H_
#define _AAC_DECODER_H_

#include <stdbool.h>

#include "msgutils/os_thread.h"
#include "esp_adf/audio_element.h"

#ifdef __cplusplus
extern "C" {
#endif

/* according to spec (13818-7 section 8.2.2, 14496-3 section 4.5.3)
 * max size of input buffer = 
 *    6144 bits =  768 bytes per SCE or CCE-I
 *   12288 bits = 1536 bytes per CPE
 *       0 bits =    0 bytes per CCE-D (uses bits from the SCE/CPE/CCE-I it is coupled to)
 */
#define AAC_DECODER_INPUT_BUFFER_SIZE    (1536)
#define AAC_MAX_NCHANS                   (2)
#define AAC_DECODER_OUTPUT_BUFFER_SIZE   (1024 * AAC_MAX_NCHANS * sizeof(short))

/**
 * @brief      AAC Decoder configurations
 */
typedef struct {
    int   out_rb_size;    /*!< Size of output ringbuffer */
    int   task_stack;     /*!< Task stack size */
    int   task_prio;      /*!< Task priority (based on freeRTOS priority) */
} aac_decoder_cfg_t;

#define AAC_DECODER_TASK_STACK          (4 * 1024)
#define AAC_DECODER_TASK_PRIO           (OS_THREAD_PRIO_NORMAL)
#define AAC_DECODER_RINGBUFFER_SIZE     (8 * 1024)
#define AAC_DECODER_BUFFER_SIZE         (512)

#define DEFAULT_AAC_DECODER_CONFIG() {\
    .out_rb_size    = AAC_DECODER_RINGBUFFER_SIZE,\
    .task_stack     = AAC_DECODER_TASK_STACK,\
    .task_prio      = AAC_DECODER_TASK_PRIO,\
}

typedef struct aac_buf_in {
    char         *data;
    unsigned int offset;
    int          size_want;
    int          size_read;
    bool         eof;
} aac_buf_in_t;

typedef struct aac_buf_out {
    char         *data;
    int          length;
    unsigned int offset;
} aac_buf_out_t;

struct aac_decoder {
    void                    *handle;
    audio_element_handle_t  el;
    aac_buf_in_t            buf_in;
    aac_buf_out_t           buf_out;
    bool                    parsed_header;
};

typedef struct aac_decoder *aac_decoder_handle_t;

int aac_wrapper_run(aac_decoder_handle_t decoder);
void aac_wrapper_deinit(aac_decoder_handle_t decoder);
int aac_wrapper_init(aac_decoder_handle_t decoder);

/**
 * @brief      Create an Audio Element handle to decode incoming AAC data
 *
 * @param      config  The configuration
 *
 * @return     The audio element handle
 */
audio_element_handle_t aac_decoder_init(aac_decoder_cfg_t *config);

#ifdef __cplusplus
}
#endif

#endif
