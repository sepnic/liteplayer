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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mp3-mad/mad.h"
#include "mp3-mad/global.h"
#include "msgutils/os_logger.h"
#include "esp_adf/audio_common.h"
#include "esp_adf/audio_element.h"
#include "audio_decoder/mp3_decoder.h"

#define TAG "MP3_WRAPPER"

static inline short scale(mad_fixed_t sample)
{
    /* round */
    sample += (1L << (MAD_F_FRACBITS - 16));

    /* clip */
    if (sample >= MAD_F_ONE)
        sample = MAD_F_ONE - 1;
    else if (sample < -MAD_F_ONE)
        sample = -MAD_F_ONE;

    /* quantize */
    return sample >> (MAD_F_FRACBITS + 1 - 16);
}

/* 
* This is the error callback function. It is called whenever a decoding
* error occurs. The error is indicated by stream->error; the list of
* possible MAD_ERROR_* errors can be found in the mad.h (or stream.h)
* header file.
*/
static enum mad_flow mad_wrapper_error(void *data, struct mad_stream *stream, struct mad_frame *frame)
{
    mp3_decoder_handle_t decoder = (mp3_decoder_handle_t)data;

    OS_LOGV(TAG, "Decoding error 0x%04x (%s) at byte offset %d",
             stream->error, mad_stream_errorstr(stream),
             stream->this_frame - (unsigned char*)decoder->buf_in.data);

    /* return MAD_FLOW_BREAK here to stop decoding (and propagate an error) */
    return MAD_FLOW_CONTINUE;
}

/*
 * NAME: decode->input_read()
 * DESCRIPTION: (re)fill decoder input buffer by reading a file descriptor
 */
static enum mad_flow mad_wrapper_input(void *data, struct mad_stream *stream)
{
    mp3_decoder_handle_t decoder = (mp3_decoder_handle_t)data;
    mp3_buf_in_t *input = &decoder->buf_in;
    int size_read = 0;
    int remain = stream->bufend - stream->next_frame;

    if (stream->next_frame && remain > 0)
        memmove(input->data, stream->next_frame, remain);

    if (input->eof) {
        OS_LOGV(TAG, "length=%d, frame_size=%d, error=0x%X",
                 input->length, decoder->frame_size, stream->error);

        if (stream->error == MAD_ERROR_BUFLEN)
            return MAD_FLOW_STOP;

        if (remain >= decoder->frame_size) {
            mad_stream_buffer(stream, (const unsigned char *)input->data, remain);
            return MAD_FLOW_CONTINUE;
        }
        else if (stream->error != MAD_ERROR_NONE) {
            return MAD_FLOW_STOP;
        }
    }

    size_read = audio_element_input(decoder->el,
                                    (char *)(input->data + remain),
                                    MP3_DECODER_INPUT_BUFFER_SIZE - remain);
    if (size_read > 0) {
       while (size_read < MAD_BUFFER_GUARD)
            input->data[remain + size_read++] = 0;
    }
    else if (size_read == AEL_IO_OK || size_read == AEL_IO_DONE || size_read == AEL_IO_ABORT) {
        input->eof = true;
        size_read = 0;
        while (size_read < MAD_BUFFER_GUARD)
            input->data[remain + size_read++] = 0;
    }
    else if (size_read == AEL_IO_TIMEOUT) {
        OS_LOGV(TAG, "Timeout to fetch data");
        return MAD_FLOW_TIMEOUT;
    }
    else if (size_read < 0) {
        OS_LOGE(TAG, "Unexpected bytes read: %d", size_read);
        return MAD_FLOW_BREAK;
    }

    mad_stream_buffer(stream, (const unsigned char *)input->data, remain + size_read);
    return MAD_FLOW_CONTINUE;
}

/* 
* NAME: decode->input_read()
* This is the output callback function. It is called after each frame of
* MPEG audio data has been completely decoded.
*/
static enum mad_flow mad_wrapper_output(void *data,
                                        struct mad_header const *header,
                                        struct mad_pcm *pcm)
{
    mp3_decoder_handle_t decoder = (mp3_decoder_handle_t)data;
    int nchannels = pcm->channels;
    int nsamples  = pcm->length;
    mad_fixed_t const * left  = pcm->samples[0];
    mad_fixed_t const * right = pcm->samples[1];
    int k;

    decoder->buf_out.length = nsamples * nchannels * sizeof(short);

    /* output sample(s) in 16-bit signed little-endian PCM */
    short *out = (short *)(decoder->buf_out.data);
    if (nchannels == 2) {
        for (k = 0; k < nsamples; k++) {
            out[k*2+0] = scale(*left++);
            out[k*2+1] = scale(*right++);
        }
    }
    else if (nchannels == 1) {
        for (k = 0; k < nsamples; k++)
            out[k] = scale(*left++);
    }

    return MAD_FLOW_CONTINUE;
}

static enum mad_flow mad_wrapper_header(void *context, struct mad_header const *header)
{
    mp3_decoder_handle_t decoder = (mp3_decoder_handle_t)context;

    if (!decoder->parsed_header && (header->flags & MAD_FLAG_INCOMPLETE)) {
        audio_element_info_t info = {0};
        info.out_samplerate = header->samplerate;
        info.out_channels   = MAD_NCHANNELS (header);
        info.bits           = 16;

        OS_LOGV(TAG,"Found mp3 header: SR=%d, CH=%d", info.out_samplerate, info.out_channels);

        decoder->parsed_header = true;

        audio_element_setinfo(decoder->el, &info);
        audio_element_report_info(decoder->el);
    }

    if (decoder->buf_in.eof) {
        int frame_size  = 0;
        /* calculate beginning of next frame */
        int pad_slot = (header->flags & MAD_FLAG_PADDING) ? 1 : 0;

        if (header->layer == MAD_LAYER_I) {
            frame_size = ((12*header->bitrate/header->samplerate)+pad_slot)*4;
        }
        else {
            int slots_per_frame;
            slots_per_frame = (header->layer == MAD_LAYER_III &&
                              (header->flags & MAD_FLAG_LSF_EXT)) ? 72 : 144;
            frame_size = (slots_per_frame*header->bitrate/header->samplerate)+pad_slot;
        }

        decoder->frame_size = frame_size;
    }

    return MAD_FLOW_CONTINUE;
}

int mp3_wrapper_run(mp3_decoder_handle_t decoder) 
{
    int result = 0;
    struct mad_decoder *mad = (struct mad_decoder *)(decoder->handle);
    struct mad_stream *stream = &(mad->sync->stream);
    struct mad_frame *frame = &(mad->sync->frame);
    struct mad_synth *synth = &(mad->sync->synth);

    do {
        switch (mad->input_func(mad->cb_data, stream)) {
        case MAD_FLOW_STOP:
            goto done;
        case MAD_FLOW_BREAK:
            result = AEL_IO_FAIL;
            goto done;
        case MAD_FLOW_TIMEOUT:
            result = AEL_IO_TIMEOUT;
            goto done;
        case MAD_FLOW_IGNORE:
            continue;
        case MAD_FLOW_CONTINUE:
            break;
        }

        while (1) {
            if (mad->header_func) {
                if (mad_header_decode(&frame->header, stream) != 0) {
                    if (!MAD_RECOVERABLE(stream->error))
                        break;

                    if (mad->error_func) {
                        switch (mad->error_func(mad->cb_data, stream, frame)) {
                        case MAD_FLOW_STOP:
                            goto done;
                        case MAD_FLOW_BREAK:
                        case MAD_FLOW_TIMEOUT:
                            result = AEL_IO_FAIL;
                            goto done;
                        case MAD_FLOW_IGNORE:
                        case MAD_FLOW_CONTINUE:
                            continue;
                        }
                    }
                }

                switch (mad->header_func(mad->cb_data, &frame->header)) {
                case MAD_FLOW_STOP:
                    goto done;
                case MAD_FLOW_BREAK:
                case MAD_FLOW_TIMEOUT:
                    result = AEL_IO_FAIL;
                    goto done;
                case MAD_FLOW_IGNORE:
                    continue;
                case MAD_FLOW_CONTINUE:
                    break; // TODO: continue is better?
                }
            }

            if (mad_frame_decode(frame, stream) != 0) {
                if (!MAD_RECOVERABLE(stream->error))
                    break;

                if (mad->error_func) {
                    switch (mad->error_func(mad->cb_data, stream, frame)) {
                    case MAD_FLOW_STOP:
                        goto done;
                    case MAD_FLOW_BREAK:
                    case MAD_FLOW_TIMEOUT:
                        result = AEL_IO_FAIL;
                        goto done;
                    case MAD_FLOW_IGNORE:
                        break;
                    case MAD_FLOW_CONTINUE:
                        continue;
                    }
                }
            }

            mad_synth_frame(synth, frame);

            if (mad->output_func) {
                mad->output_func(mad->cb_data, &frame->header, &synth->pcm);
                return 0;
            }
        }
    } while (stream->error == MAD_ERROR_BUFLEN);

done:
    mad_synth_finish(synth);
    mad_frame_finish(frame);
    mad_stream_finish(stream);
    return result;
}

/* 
* This is the function called by main() above to perform all the decoding.
* It instantiates a decoder object and configures it with the input,
* output, and error callback functions above. A single call to
* mad_decoder_run() continues until a callback function returns
* MAD_FLOW_STOP (to stop decoding) or MAD_FLOW_BREAK (to stop decoding and
* signal an error).
*/

int mp3_wrapper_init(mp3_decoder_handle_t decoder) 
{
    struct mad_decoder *mad = audio_calloc(1, sizeof(struct mad_decoder));
    if (mad == NULL) {
        OS_LOGE(TAG, "Failed to allocate memory for mad decoder");
        return -1;
    }

    decoder->handle = (void *)mad;
    mad->mode = MAD_DECODER_MODE_SYNC;

    /* configure input, output, and error functions */
    mad_decoder_init(mad, (void *)decoder,
                     mad_wrapper_input,
                     mad_wrapper_header,
                     0 /* filter */,
                     mad_wrapper_output,
                     mad_wrapper_error,
                     0 /* message */);

    mad->sync = audio_calloc(1, sizeof(*mad->sync));
    if (mad->sync == NULL) {
        OS_LOGE(TAG, "Failed to allocate memory for decoder sync");
        audio_free(mad);
        return -1;
    }

    mad_stream_init(&(mad->sync->stream));
    mad_frame_init(&(mad->sync->frame));
    mad_synth_init(&(mad->sync->synth));
    mad_stream_options(&(mad->sync->stream), mad->options);

    return 0;
} 

void mp3_wrapper_deinit(mp3_decoder_handle_t decoder)
{
    struct mad_decoder *mad = (struct mad_decoder *)decoder->handle;
    if (mad == NULL) return;

    mad_synth_finish(&(mad->sync->synth));
    mad_frame_finish(&(mad->sync->frame));
    mad_stream_finish(&(mad->sync->stream));

    audio_free(mad->sync);
    audio_free(mad);
}
