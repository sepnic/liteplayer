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

#ifndef _M4A_DECODER_H_
#define _M4A_DECODER_H_

#include "audio_extractor/m4a_extractor.h"
#include "aac_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief      M4A Decoder configurations
 */
struct m4a_decoder_cfg {
    int   out_rb_size;    /*!< Size of output ringbuffer */
    int   task_stack;     /*!< Task stack size */
    int   task_prio;      /*!< Task priority (based on freeRTOS priority) */
    struct m4a_info *m4a_info;
};

#define DEFAULT_M4A_DECODER_CONFIG() {\
    .out_rb_size    = AAC_DECODER_RINGBUFFER_SIZE,\
    .task_stack     = AAC_DECODER_TASK_STACK,\
    .task_prio      = AAC_DECODER_TASK_PRIO,\
}

struct m4a_decoder {
    void                   *handle;
    audio_element_handle_t  el;
    struct aac_buf_in       buf_in;
    struct aac_buf_out      buf_out;
    struct m4a_info        *m4a_info;
    bool                    parsed_header;
};

typedef struct m4a_decoder *m4a_decoder_handle_t;

int m4a_wrapper_run(m4a_decoder_handle_t decoder);
void m4a_wrapper_deinit(m4a_decoder_handle_t decoder);
int m4a_wrapper_init(m4a_decoder_handle_t decoder);

/**
 * @brief      Create an Audio Element handle to decode incoming M4A data
 *
 * @param      config  The configuration
 *
 * @return     The audio element handle
 */
audio_element_handle_t m4a_decoder_init(struct m4a_decoder_cfg *config);

#ifdef __cplusplus
}
#endif

#endif
