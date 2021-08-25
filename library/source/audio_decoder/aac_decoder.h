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

#ifndef _AAC_DECODER_H_
#define _AAC_DECODER_H_

#include <stdbool.h>

#include "osal/os_thread.h"
#include "esp_adf/audio_element.h"
#include "audio_extractor/aac_extractor.h"

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
/* The frame size of the decoded PCM audio signal, Typically this is:
 *   1024 or 960 for AAC-LC
 *   2048 or 1920 for HE-AAC (v2)
 *   512 or 480 for AAC-LD and AAC-ELD
 *   768, 1024, 2048 or 4096 for USAC
 * max size of output buffer = 4096 * AAC_MAX_NCHANS * sizeof(short))
 */
#define AAC_DECODER_OUTPUT_BUFFER_SIZE   (4096 * AAC_MAX_NCHANS * sizeof(short))

/**
 * @brief      AAC Decoder configurations
 */
struct aac_decoder_cfg {
    int   out_rb_size;    /*!< Size of output ringbuffer */
    int   task_stack;     /*!< Task stack size */
    int   task_prio;      /*!< Task priority (based on freeRTOS priority) */
    struct aac_info *aac_info;
};

#define AAC_DECODER_TASK_STACK          (4 * 1024)
#define AAC_DECODER_TASK_PRIO           (OS_THREAD_PRIO_NORMAL)
#define AAC_DECODER_RINGBUFFER_SIZE     (8 * 1024)
#define AAC_DECODER_BUFFER_SIZE         (512)

#define DEFAULT_AAC_DECODER_CONFIG() {\
    .out_rb_size    = AAC_DECODER_RINGBUFFER_SIZE,\
    .task_stack     = AAC_DECODER_TASK_STACK,\
    .task_prio      = AAC_DECODER_TASK_PRIO,\
}

struct aac_buf_in {
    char  data[AAC_DECODER_INPUT_BUFFER_SIZE];
    int   bytes_want;
    int   bytes_read;
    bool  eof;
};

struct aac_buf_out {
    char  data[AAC_DECODER_OUTPUT_BUFFER_SIZE];
    int   bytes_remain;
    int   bytes_written;
};

struct aac_decoder {
    void                   *handle;
    audio_element_handle_t  el;
    struct aac_buf_in       buf_in;
    struct aac_buf_out      buf_out;
    struct aac_info        *aac_info;
    bool                    parsed_header;
    bool                    seek_mode;
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
audio_element_handle_t aac_decoder_init(struct aac_decoder_cfg *config);

#ifdef __cplusplus
}
#endif

#endif
