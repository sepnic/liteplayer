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

#ifndef _MP3_DECODER_H_
#define _MP3_DECODER_H_

#include "msgutils/os_thread.h"
#include "esp_adf/audio_element.h"

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
typedef struct {
    int   out_rb_size;    /*!< Size of output ringbuffer */
    int   task_stack;     /*!< Task stack size */
    int   task_prio;      /*!< Task priority (based on freeRTOS priority) */
} mp3_decoder_cfg_t;

#define MP3_DECODER_TASK_STACK          (4 * 1024)
#define MP3_DECODER_TASK_PRIO           (OS_THREAD_PRIO_NORMAL)
#define MP3_DECODER_RINGBUFFER_SIZE     (8 * 1024)
#define MP3_DECODER_BUFFER_SIZE         (512)

#define DEFAULT_MP3_DECODER_CONFIG() {\
    .out_rb_size    = MP3_DECODER_RINGBUFFER_SIZE,\
    .task_stack     = MP3_DECODER_TASK_STACK,\
    .task_prio      = MP3_DECODER_TASK_PRIO,\
}

typedef struct mp3_buf_in {
    char *data;
    int length;
    bool eof;
} mp3_buf_in_t;

typedef struct mp3_buf_out {
    char *data;          //data for output
    int length;          //total lenght of current decoded frame
    unsigned int offset; //total lenght of current decoded frame
} mp3_buf_out_t;

struct mp3_decoder {
    void                    *handle;
    audio_element_handle_t  el;
    mp3_buf_in_t            buf_in;
    mp3_buf_out_t           buf_out;
    bool                    parsed_header;
    int                     frame_size;
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
audio_element_handle_t mp3_decoder_init(mp3_decoder_cfg_t *config);

#ifdef __cplusplus
}
#endif

#endif