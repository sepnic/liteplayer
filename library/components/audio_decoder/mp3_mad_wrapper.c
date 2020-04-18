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
#include "audio_extractor/mp3_extractor.h"
#include "audio_decoder/mp3_decoder.h"

#define TAG "MP3_WRAPPER"

struct mad_wrapper {
    struct mad_stream stream;
    struct mad_frame frame;
    struct mad_synth synth;
    char data[MP3_DECODER_INPUT_BUFFER_SIZE*2];
    bool new_frame;    // if reading new frame
    int frame_size;
};

static int mp3_frame_size(char *buf)
{
    unsigned char ver, layer, brIdx, srIdx, padding;
    int sample_rate = 0, bit_rate = 0, frame_size = 0;

    if ((buf[0] & 0xFF) != 0xFF || (buf[1] & 0xE0) != 0xE0) {
        OS_LOGE(TAG, "Invalid mp3 sync word");
        return -1;
    }

    // read header fields - use bitmasks instead of GetBits() for speed, since format never varies
    ver     = (buf[1] >> 3) & 0x03;
    layer   = (buf[1] >> 1) & 0x03;
    brIdx   = (buf[2] >> 4) & 0x0f;
    srIdx   = (buf[2] >> 2) & 0x03;
    padding = (buf[2] >> 1) & 0x01;
    //sMode   = (buf[3] >> 6) & 0x03;

    // check parameters to avoid indexing tables with bad values
    if (ver == 1 ||  srIdx >= 3 || layer == 0 || brIdx == 15 || brIdx == 0) {
        OS_LOGE(TAG, "Invalid mp3 frame header");
        return -1;
    }

    static const int kSamplingRateV1[] = {44100, 48000, 32000};
    sample_rate = kSamplingRateV1[srIdx];
    if (ver == 2 /* V2 */) {
        sample_rate /= 2;
    } else if (ver == 0 /* V2.5 */) {
        sample_rate /= 4;
    }

    if (layer == 3) {
        // layer I
        static const int kBitrateV1[] = {
            32, 64, 96, 128, 160, 192, 224, 256,  288, 320, 352, 384, 416, 448
        };
        static const int kBitrateV2[] = {
            32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256
        };
        bit_rate = (ver == 3) ? kBitrateV1[brIdx - 1] : kBitrateV2[brIdx - 1];
        frame_size = (12000 * bit_rate / sample_rate + padding) * 4;
    } else {
        // layer II or III
        static const int kBitrateV1L2[] = {
            32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384
        };
        static const int kBitrateV1L3[] = {
            32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320
        };
        static const int kBitrateV2[] = {
            8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160
        };
        if (ver == 3 /* V1 */) {
            bit_rate = (layer == 2) ? kBitrateV1L2[brIdx - 1] : kBitrateV1L3[brIdx - 1];
        } else {
            // V2 (or 2.5)
            bit_rate = kBitrateV2[brIdx - 1];
        }

        if (ver == 3 /* V1 */) {
            frame_size = 144000 * bit_rate / sample_rate + padding;
        } else {
            // V2 or V2.5
            int tmp = (layer == 1 /* L3 */) ? 72000 : 144000;
            frame_size = tmp * bit_rate / sample_rate + padding;
        }
    }

    return frame_size;
}

static int mp3_data_read(mp3_decoder_handle_t decoder)
{
    struct mad_wrapper *mad = (struct mad_wrapper *)(decoder->handle);
    mp3_buf_in_t *in = &decoder->buf_in;
    int ret = 0;

    if (in->eof)
        return AEL_IO_DONE;

    if (in->bytes_want > 0) {
        if (mad->new_frame) {
            OS_LOGD(TAG, "Remain %d/4 bytes header needed to read", in->bytes_want);
            goto fill_header;
        }
        else {
            OS_LOGD(TAG, "Remain %d/%d bytes frame needed to read", in->bytes_want, mad->frame_size);
            goto fill_frame;
        }
    }

    in->bytes_want = 4;
    in->bytes_read = 0;
    mad->new_frame = true;
fill_header:
    ret = audio_element_input(decoder->el, in->data + in->bytes_read, in->bytes_want);
    if (ret < in->bytes_want) {
        if (ret >= 0) {
            in->bytes_read += ret;
            in->bytes_want -= ret;
            return AEL_IO_TIMEOUT;
        }
        else if (ret == AEL_IO_TIMEOUT) {
            return AEL_IO_TIMEOUT;
        }
        else if (ret == AEL_IO_OK || ret == AEL_IO_DONE || ret == AEL_IO_ABORT) {
            in->eof = true;
            return AEL_IO_DONE;
        }
        else {
            return AEL_IO_FAIL;
        }
    }

    mad->frame_size = mp3_frame_size(in->data);
    if (mad->frame_size <= 0 || mad->frame_size > MP3_DECODER_INPUT_BUFFER_SIZE) {
        OS_LOGW(TAG, "MP3 demux dummy data, AEL_IO_DONE");
        //in->eof = true;
        return AEL_IO_DONE;
    }

    in->bytes_read = 4;
    in->bytes_want = mad->frame_size - in->bytes_read;
    mad->new_frame = false;
fill_frame:
    ret = audio_element_input(decoder->el, in->data + in->bytes_read, in->bytes_want);
    if (ret < in->bytes_want) {
        if (ret >= 0) {
            in->bytes_read += ret;
            in->bytes_want -= ret;
            return AEL_IO_TIMEOUT;
        }
        else if (ret == AEL_IO_TIMEOUT) {
            return AEL_IO_TIMEOUT;
        }
        else if (ret == AEL_IO_OK || ret == AEL_IO_DONE || ret == AEL_IO_ABORT) {
            in->eof = true;
            return AEL_IO_DONE;
        }
        else {
            return AEL_IO_FAIL;
        }
    }

    in->bytes_read = mad->frame_size;
    in->bytes_want = 0;
    return AEL_IO_OK;
}

static inline short scale(mad_fixed_t sample)
{
    sample += (1L << (MAD_F_FRACBITS - 16));
    if (sample >= MAD_F_ONE)
        sample = MAD_F_ONE - 1;
    else if (sample < -MAD_F_ONE)
        sample = -MAD_F_ONE;
    return sample >> (MAD_F_FRACBITS + 1 - 16);
}

int mp3_wrapper_run(mp3_decoder_handle_t decoder) 
{
    struct mad_wrapper *mad = (struct mad_wrapper *)(decoder->handle);
    struct mad_stream *stream = &mad->stream;
    struct mad_frame *frame = &mad->frame;
    struct mad_synth *synth = &mad->synth;
    int remain = 0;
    int ret = 0;

fill_data:
    ret = mp3_data_read(decoder);
    if (ret != AEL_IO_OK) {
        if (decoder->buf_in.eof) {
            remain = stream->bufend - stream->next_frame;
            if (stream->next_frame != NULL && remain >= mad->frame_size) {
                OS_LOGV(TAG, "Remain last frame %d bytes needed to decode", remain);
                goto decode_data;
            }
            OS_LOGV(TAG, "MP3 frame end, remain bytes: %d", remain);
            ret = AEL_IO_DONE;
        }
        return ret;
    }

decode_data:
    remain = stream->bufend - stream->next_frame;
    if (stream->next_frame != NULL && remain > 0) {
        if (remain >= MP3_DECODER_INPUT_BUFFER_SIZE) {
            OS_LOGW(TAG, "Unexpected bytes remain: %d, dummy data?", remain);
            return AEL_IO_DONE;
        }
        memmove(mad->data, stream->next_frame, remain);
    }
    if (decoder->buf_in.bytes_read > 0)
        memcpy(mad->data+remain, decoder->buf_in.data, decoder->buf_in.bytes_read);

    int bytes_read = decoder->buf_in.bytes_read;
    if (decoder->buf_in.eof) {
        while (bytes_read < MAD_BUFFER_GUARD && (bytes_read+remain) < MP3_DECODER_INPUT_BUFFER_SIZE) {
            mad->data[remain+bytes_read] = 0;
            bytes_read++;
        }
    }

    mad_stream_buffer(stream, (unsigned char *)mad->data, bytes_read+remain);
    stream->error = MAD_ERROR_NONE;

    ret = mad_frame_decode(frame, stream);
    if (ret != 0) {
        if (decoder->buf_in.eof) {
            return AEL_IO_DONE;
        }
        if (stream->error == MAD_ERROR_BUFLEN) {
            OS_LOGV(TAG, "Input buffer too small (or EOF)");
            goto fill_data;
        }
        else if (MAD_RECOVERABLE(stream->error)) {
            OS_LOGV(TAG, "Recoverable error: %s", mad_stream_errorstr(stream));
            goto fill_data;
        }
        else {
            OS_LOGE(TAG, "Unrecoverable error: %s", mad_stream_errorstr(stream));
            return AEL_PROCESS_FAIL;
        }
    }

    mad_synth_frame(synth, frame);

    mad_fixed_t const *left  = synth->pcm.samples[0];
    mad_fixed_t const *right = synth->pcm.samples[1];
    short *out = (short *)(decoder->buf_out.data);
    int k;
    decoder->buf_out.bytes_remain = synth->pcm.length * synth->pcm.channels * sizeof(short);
    if (synth->pcm.channels == 2) {
        for (k = 0; k < synth->pcm.length; k++) {
            out[k*2+0] = scale(*left++);
            out[k*2+1] = scale(*right++);
        }
    }
    else if (synth->pcm.channels == 1) {
        for (k = 0; k < synth->pcm.length; k++)
            out[k] = scale(*left++);
    }

    if (!decoder->parsed_header) {
        audio_element_info_t info = {0};
        info.out_samplerate = frame->header.samplerate;
        info.out_channels   = MAD_NCHANNELS(&(frame->header));
        info.bits           = 16;
        OS_LOGV(TAG,"Found mp3 header: SR=%d, CH=%d", info.out_samplerate, info.out_channels);
        audio_element_setinfo(decoder->el, &info);
        audio_element_report_info(decoder->el);
        decoder->parsed_header = true;
    }

    return AEL_IO_OK;
}

int mp3_wrapper_init(mp3_decoder_handle_t decoder) 
{
    struct mad_wrapper *mad = audio_calloc(1, sizeof(struct mad_wrapper));
    if (mad == NULL) {
        OS_LOGE(TAG, "Failed to allocate memory for mad decoder");
        return -1;
    }

    mad_stream_init(&mad->stream);
    mad->stream.options |= MAD_OPTION_IGNORECRC;
    mad_frame_init(&mad->frame);
    mad_synth_init(&mad->synth);

    decoder->handle = (void *)mad;
    return 0;
} 

void mp3_wrapper_deinit(mp3_decoder_handle_t decoder)
{
    struct mad_wrapper *mad = (struct mad_wrapper *)decoder->handle;
    if (mad == NULL) return;

    mad_synth_finish(&mad->synth);
    mad_frame_finish(&mad->frame);
    mad_stream_finish(&mad->stream);
    audio_free(mad);
}
