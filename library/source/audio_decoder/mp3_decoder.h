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

#ifndef _MP3_DECODER_H_
#define _MP3_DECODER_H_

#include "osal/os_thread.h"
#include "esp_adf/audio_element.h"
#include "audio_extractor/mp3_extractor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MP3_MAX_NCHANS                  (2)
// MAX_NSAMP*MAX_NGRAN*MAX_NCHAN*sizeof(short)
// MAX_NSAMP refer to http://wiki.hydrogenaud.io/index.php?title=MP3#Polyphase_Filterbank_Formula
#define MP3_DECODER_OUTPUT_BUFFER_SIZE  (1152 * MP3_MAX_NCHANS * sizeof(short))
#define MP3_DECODER_INPUT_BUFFER_SIZE   (1940)  // MAINBUF_SIZE

/**
 * @brief      Mp3 Decoder configurations
 */
struct mp3_decoder_cfg {
    int   task_stack;     /*!< Task stack size */
    int   task_prio;      /*!< Task priority (based on freeRTOS priority) */
    struct mp3_info *mp3_info;
};

#define MP3_DECODER_TASK_STACK          (4 * 1024)
#define MP3_DECODER_TASK_PRIO           (OS_THREAD_PRIO_NORMAL)

#define DEFAULT_MP3_DECODER_CONFIG() {\
    .task_stack     = MP3_DECODER_TASK_STACK,\
    .task_prio      = MP3_DECODER_TASK_PRIO,\
}

struct mp3_buf_in {
    char data[MP3_DECODER_INPUT_BUFFER_SIZE];
    int  bytes_want;     // bytes that want to read
    int  bytes_read;     // bytes that have read
    bool eof;            // if end of stream
};

struct mp3_buf_out {
    char data[MP3_DECODER_OUTPUT_BUFFER_SIZE];
    int  bytes_remain;   // bytes that remained to write
    int  bytes_written;  // bytes that have written
};

struct mp3_decoder {
    void                   *handle;
    audio_element_handle_t  el;
    struct mp3_buf_in       buf_in;
    struct mp3_buf_out      buf_out;
    struct mp3_info        *mp3_info;
    bool                    parsed_header;
    bool                    seek_mode;
};

typedef struct mp3_decoder *mp3_decoder_handle_t;

int mp3_wrapper_init(mp3_decoder_handle_t decoder);
void mp3_wrapper_deinit(mp3_decoder_handle_t decoder);
int mp3_wrapper_run(mp3_decoder_handle_t decoder);

/**
 * @brief      Create an Audio Element handle to decode incoming MP3 data
 *
 * @param      config  The configuration
 *
 * @return     The audio element handle
 */
audio_element_handle_t mp3_decoder_init(struct mp3_decoder_cfg *config);

#ifdef __cplusplus
}
#endif

#endif