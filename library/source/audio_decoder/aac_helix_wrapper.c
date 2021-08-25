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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cutils/log_helper.h"
#include "esp_adf/audio_common.h"
#include "esp_adf/audio_element.h"
#include "audio_decoder/aac_decoder.h"
#include "audio_decoder/m4a_decoder.h"
#include "aac-helix/aacdec.h"

#define TAG "[liteplayer]AAC_DECODER"

static int aac_adts_read(aac_decoder_handle_t decoder)
{
    char *data = decoder->buf_in.data;
    int remain = decoder->buf_in.bytes_read;
    int want = AAC_DECODER_INPUT_BUFFER_SIZE - remain;

    if (remain != 0)
        memmove(data, &data[want], remain);

    int ret = audio_element_input(decoder->el, &data[remain], want);
    if (ret >= 0) {
        decoder->buf_in.bytes_read += ret;
        return AEL_IO_OK;
    } else if (ret == AEL_IO_TIMEOUT) {
        return AEL_IO_TIMEOUT;
    } else if (ret == AEL_IO_OK || ret == AEL_IO_DONE || ret == AEL_IO_ABORT) {
        decoder->buf_in.eof = true;
        return AEL_IO_DONE;
    } else {
        OS_LOGE(TAG, "AAC read fail, ret=%d", ret);
        return AEL_IO_FAIL;
    }
    return ret;
}

int aac_wrapper_run(aac_decoder_handle_t decoder)
{
    int ret = 0;
    int decode_fail_cnt = 0;

fill_data:
    ret = aac_adts_read(decoder);
    if (ret != AEL_IO_OK) {
        if (decoder->buf_in.eof) {
            OS_LOGV(TAG, "AAC frame end");
            ret = AEL_IO_DONE;
        }
        return ret;
    }

    unsigned char *in = (unsigned char *)(decoder->buf_in.data);
    short *out = (short *)(decoder->buf_out.data);
    int size = decoder->buf_in.bytes_read;

    ret = AACDecode(decoder->handle, &in, &size, out);
    if (ret == ERR_AAC_INDATA_UNDERFLOW) {
        if (decoder->buf_in.eof)
            return AEL_IO_DONE;
        else
            goto fill_data;
    } else if (ret < ERR_AAC_NONE) {
        OS_LOGE(TAG, "AACDecode error[%d]", ret);
        if(decode_fail_cnt++ >= 4)
            return AEL_PROCESS_FAIL;
        goto fill_data;
    }

    decoder->buf_in.bytes_read = size;

    AACFrameInfo frame_info;
    AACGetLastFrameInfo(decoder->handle, &frame_info);
    decoder->buf_out.bytes_remain = frame_info.outputSamps*frame_info.bitsPerSample/8;

    if (!decoder->parsed_header && frame_info.sampRateOut != 0 && frame_info.nChans != 0) {
        audio_element_info_t info = {0};
        info.out_samplerate = frame_info.sampRateOut;
        info.out_channels   = frame_info.nChans;
        info.bits           = 16;
        audio_element_setinfo(decoder->el, &info);
        audio_element_report_info(decoder->el);

        OS_LOGV(TAG,"Found aac header: SR=%d, CH=%d", info.out_samplerate, info.out_channels);
        decoder->parsed_header = true;
    }
    return 0;
}

int aac_wrapper_init(aac_decoder_handle_t decoder)
{
    HAACDecoder hDecoder = AACInitDecoder();
    if (hDecoder == NULL) {
        OS_LOGE(TAG, "Failed to init aac decoder");
        return -1;
    }

    decoder->handle = hDecoder;
    return AACFlushCodec(hDecoder);
}

void aac_wrapper_deinit(aac_decoder_handle_t decoder)
{
    if (decoder->handle != NULL) {
        AACFreeDecoder((HAACDecoder)(decoder->handle));
        decoder->handle = NULL;
    }
}

static int m4a_mdat_read(m4a_decoder_handle_t decoder)
{
    unsigned int stsz_entries = decoder->m4a_info->stsz_samplesize_entries;
    unsigned int stsz_current = decoder->m4a_info->stsz_samplesize_index;
    struct aac_buf_in *in = &decoder->buf_in;
    int ret = AEL_IO_OK;

    if (stsz_current >= stsz_entries) {
        in->eof = true;
        return AEL_IO_DONE;
    }
    in->bytes_want = decoder->m4a_info->stsz_samplesize[stsz_current];
    in->bytes_read = 0;

    ret = audio_element_input_chunk(decoder->el, in->data, in->bytes_want);
    if (ret == in->bytes_want) {
        in->bytes_read += ret;
        goto read_done;
    } else if (ret == AEL_IO_OK || ret == AEL_IO_DONE || ret == AEL_IO_ABORT) {
        in->eof = true;
        return AEL_IO_DONE;
    } else if (ret < 0) {
        OS_LOGW(TAG, "Read chunk error: %d/%d", ret, in->bytes_want);
        return ret;
    } else {
        OS_LOGW(TAG, "Read chunk insufficient: %d/%d, AEL_IO_DONE", ret, in->bytes_want);
        in->eof = true;
        return AEL_IO_DONE;
    }

read_done:
    decoder->m4a_info->stsz_samplesize_index++;
    return AEL_IO_OK;
}

int m4a_wrapper_run(m4a_decoder_handle_t decoder)
{
    int ret = 0;
    int decode_fail_cnt = 0;

fill_data:
    ret = m4a_mdat_read(decoder);
    if (ret != AEL_IO_OK) {
        if (decoder->buf_in.eof) {
            OS_LOGV(TAG, "M4A frame end");
            ret = AEL_IO_DONE;
        }
        return ret;
    }

    unsigned char *in = (unsigned char *)(decoder->buf_in.data);
    short *out = (short *)(decoder->buf_out.data);
    int size = decoder->buf_in.bytes_read;

    ret = AACDecode(decoder->handle, &in, &size, out);
    if (ret == ERR_AAC_INDATA_UNDERFLOW) {
        if (decoder->buf_in.eof == true)
            return AEL_IO_DONE;
        else
            goto fill_data;
    } else if (ret < ERR_AAC_NONE) {
        OS_LOGE(TAG, "AACDecode error[%d]", ret);
        if(decode_fail_cnt++ >= 4)
            return AEL_PROCESS_FAIL;
        goto fill_data;
    }

    AACFrameInfo frame_info;
    AACGetLastFrameInfo(decoder->handle, &frame_info);
    decoder->buf_out.bytes_remain = frame_info.outputSamps*frame_info.bitsPerSample/8;

    if (!decoder->parsed_header && frame_info.sampRateOut != 0 && frame_info.nChans != 0) {
        audio_element_info_t info = {0};
        info.out_samplerate = frame_info.sampRateOut;
        info.out_channels   = frame_info.nChans;
        info.bits           = 16;
        audio_element_setinfo(decoder->el, &info);
        audio_element_report_info(decoder->el);

        OS_LOGV(TAG,"Found aac header: SR=%d, CH=%d", info.out_samplerate, info.out_channels);
        decoder->parsed_header = true;
    }
    return 0;
}

int m4a_wrapper_init(m4a_decoder_handle_t decoder)
{
    int ret = 0;
    HAACDecoder hDecoder = AACInitDecoder();
    if (hDecoder == NULL) {
        OS_LOGE(TAG, "Failed to init aac decoder");
        return -1;
    }

    {
        AACFrameInfo aacFrameInfo   = {0};
        aacFrameInfo.nChans         = decoder->m4a_info->asc.channels;
        aacFrameInfo.sampRateCore   = decoder->m4a_info->asc.samplerate;
        aacFrameInfo.profile        = 1; /* 0 = main, 1 = LC, 2 = SSR, 3 = reserved */
        ret = AACSetRawBlockParams(hDecoder, 0, &aacFrameInfo);
    }

    ret |= AACFlushCodec(hDecoder);

    decoder->handle = hDecoder;
    return ret;
}

void m4a_wrapper_deinit(m4a_decoder_handle_t decoder)
{
    if (decoder->handle != NULL) {
        AACFreeDecoder((HAACDecoder)(decoder->handle));
        decoder->handle = NULL;
    }
}
