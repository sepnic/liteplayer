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

#include "mp3-helix/mp3common.h"
#include "msgutils/os_logger.h"
#include "esp_adf/audio_common.h"
#include "esp_adf/audio_element.h"
#include "audio_decoder/mp3_decoder.h"

#define TAG "MP3_WRAPPER"

static const char *helix_stream_errorstr(enum helix_error error)
{
    switch (error) {
    case ERR_MP3_NONE:                  return "no error";
    case ERR_MP3_INDATA_UNDERFLOW:      return "input buffer too small (or EOF)";
    case ERR_MP3_MAINDATA_UNDERFLOW:    return "output buffer too small (or EOF)";
    case ERR_MP3_NULL_POINTER:          return "invalid (null) buffer pointer";
    case ERR_MP3_OUT_OF_MEMORY:         return "not enough memory";
    case ERR_MP3_FREE_BITRATE_SYNC:     return "lost synchronization";
    case ERR_MP3_INVALID_FRAMEHEADER:   return "invalid frame header value";
    case ERR_MP3_INVALID_SIDEINFO:      return "invalid side information";
    case ERR_MP3_INVALID_SCALEFACT:     return "bad scalefactor index";
    case ERR_MP3_INVALID_HUFFCODES:     return "bad Huffman table select";
    case ERR_MP3_INVALID_DEQUANTIZE:    return "bad dequantize";
    case ERR_MP3_INVALID_IMDCT:         return "bad imdct";
    case ERR_MP3_INVALID_SUBBAND:       return "bad subband";
    case ERR_UNKNOWN:
    default:                            return "unknown error";
    }
}

static int mp3_data_read(mp3_decoder_handle_t decoder)
{
    unsigned char *in = (unsigned char *)decoder->buf_in.data;
    int offset = 0, length = 0;

next_offset:
    offset = decoder->buf_in.offset;
    length = decoder->buf_in.bytes_read;

    do {
        int sync = MP3FindSyncWord(&in[offset], length-offset);
        if (sync >= 0) {
            offset += sync;
            break;
        }

        offset = 0;
        // Could be 1st half of syncword, preserve it...
        if (length > 0 && in[length-1] == SYNCWORDH) {
            length = audio_element_input(decoder->el, (char *)&in[1], MP3_DECODER_INPUT_BUFFER_SIZE-1);
            if (length > 0) {
                in[0] = SYNCWORDH;
                length += 1;
            }
            else if (length == AEL_IO_OK || length == AEL_IO_DONE || length == AEL_IO_ABORT) {
                decoder->buf_in.eof = true;
                return AEL_IO_DONE;
            }
        }
        else {
            // Try a whole new buffer
            length = audio_element_input(decoder->el, (char *)in, MP3_DECODER_INPUT_BUFFER_SIZE);
            if (length == AEL_IO_OK || length == AEL_IO_DONE || length == AEL_IO_ABORT) {
                decoder->buf_in.eof = true;
                return AEL_IO_DONE;
            }
        }
    } while (true);

    // Move the frame to start at offset 0 in the buffer
    if (offset > 0) {
        length -= offset;
        memmove(in, &in[offset], length);
    }
    decoder->buf_in.offset = 0;
    decoder->buf_in.bytes_read = length;

    // We have a sync word at 0 now, try and fill remainder of buffer
    if (!decoder->buf_in.eof) {
        if (length < MP3_DECODER_INPUT_BUFFER_SIZE) {
            int byte_read = audio_element_input(decoder->el, (char*)&in[length], MP3_DECODER_INPUT_BUFFER_SIZE-length);
            if (byte_read > 0)
                decoder->buf_in.bytes_read += byte_read;
            else if (byte_read == AEL_IO_OK || byte_read == AEL_IO_DONE || byte_read == AEL_IO_ABORT)
                decoder->buf_in.eof = true;
        }
    }

    MP3FrameInfo fi;
    int ret = MP3GetNextFrameInfo(decoder->handle, &fi, in);
    if (ret == ERR_MP3_INVALID_FRAMEHEADER) {
        OS_LOGV(TAG, "Fake sync word, find next sync frame");
        // Need to update pointer if fake sync word
        decoder->buf_in.offset += sizeof(char);
        decoder->buf_in.bytes_read -= sizeof(char);
        goto next_offset;
    }
    else if (ret != ERR_MP3_NONE) {
        return AEL_IO_FAIL;
    }

    return 0;
}

int mp3_wrapper_run(mp3_decoder_handle_t decoder)
{
    HMP3Decoder hMP3Decoder = decoder->handle;
    int decode_fail_cnt = 0;
    int ret = 0;

fill_data:
    ret = mp3_data_read(decoder);
    if (ret != ERR_MP3_NONE) {
        if (decoder->buf_in.eof) {
            OS_LOGV(TAG, "MP3 frame end");
            ret = AEL_IO_DONE;
        }
        return ret;
    }

    unsigned char *in = (unsigned char *)decoder->buf_in.data;
    short *out        = (short *)(decoder->buf_out.data);
    int bytes_left    = decoder->buf_in.bytes_read;

    ret = MP3Decode(hMP3Decoder, &in, &bytes_left, out, 0);
    if (ret == ERR_MP3_INDATA_UNDERFLOW) {
        if (decoder->buf_in.eof)
            return AEL_IO_DONE;
        else
            goto fill_data;
    }
    else if (ret < ERR_MP3_NONE) {
        OS_LOGE(TAG, "MP3Decode error[%d],[%s]", ret, helix_stream_errorstr(ret));
        if(decode_fail_cnt++ >= 4)
            return AEL_PROCESS_FAIL;
        goto fill_data;
    }

    decoder->buf_in.offset = decoder->buf_in.bytes_read - bytes_left;

    MP3FrameInfo fi;
    MP3GetLastFrameInfo(hMP3Decoder, &fi);
    decoder->buf_out.length = fi.outputSamps * fi.bitsPerSample / 8;

    if (decoder->parsed_header == false) {
        OS_LOGV(TAG,"Found mp3 header: SR=%d, CH=%d", fi.samprate, fi.nChans);
        audio_element_info_t info = {0};
        info.out_samplerate = fi.samprate;
        info.out_channels   = fi.nChans;
        info.bits           = fi.bitsPerSample;
        audio_element_setinfo(decoder->el, &info);
        audio_element_report_info(decoder->el);
        decoder->parsed_header = true;
    }
    return 0;
}

int mp3_wrapper_init(mp3_decoder_handle_t decoder)
{
    decoder->handle = MP3InitDecoder();
    if (decoder->handle == NULL) {
        OS_LOGE(TAG, "Failed to init mp3 decoder");
        return -1;
    }
    return 0;
}

void mp3_wrapper_deinit(mp3_decoder_handle_t decoder)
{
    HMP3Decoder hdecoder = (HMP3Decoder)decoder->handle;
    if (hdecoder == NULL)
        return;
    MP3FreeDecoder(hdecoder);
}
