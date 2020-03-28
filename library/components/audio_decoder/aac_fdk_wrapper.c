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

#include "msgutils/os_logger.h"
#include "esp_adf/audio_common.h"
#include "esp_adf/audio_element.h"
#include "audio_decoder/aac_decoder.h"
#include "audio_decoder/m4a_decoder.h"
#include "aacdecoder_lib.h"

#define TAG "AAC_WRAPPER"

typedef enum aac_error {
    AAC_ERR_NONE          = -0x00,    /* no error */
    AAC_ERR_FAIL          = -0x01,    /* input buffer too small */
    AAC_ERR_UNSUPPORTED   = -0x02,    /* invalid (null) buffer pointer */
    AAC_ERR_NOMEM         = -0x03,    /* not enough memory */
    AAC_ERR_OPCODE        = -0x04,    /* opcode error */
    AAC_ERR_STARVE_0      = -0x05,    /* no data remaining, need more data */
    AAC_ERR_STARVE_1      = -0x06,    /* still have data left but not enough for continue handling */
    AAC_ERR_STARVE_2      = -0x07,    /* ATOM_DATA finish, no data remaining, need more data to process ATOM_NAME. */
    AAC_ERR_LOSTSYNC      = -0x08,    /* lost synchronization */
    AAC_ERR_EOF           = -0x09,    /* EOF */
} aac_error_t;

static int aac_adts_read(aac_decoder_handle_t handle)
{
    int ret = AAC_ERR_NONE;
    char *data = handle->buf_in.data;
    int remain = handle->buf_in.size_read;
    int want = AAC_DECODER_INPUT_BUFFER_SIZE - remain;

    if (remain != 0)
        memmove(data, &data[want], remain);

    int byte_read = audio_element_input(handle->el, (char*)&data[remain], want);
    if (byte_read >= 0) {
        handle->buf_in.size_read += byte_read;
    } else {
        if (byte_read == RB_DONE)
            handle->buf_in.eof = true;
        ret = byte_read;
    }

    return ret;
}

int aac_wrapper_run(aac_decoder_handle_t decoder)
{
    int ret = 0;
    int decode_fail_cnt = 0;

fill_data:
    ret = aac_adts_read(decoder);
    if (ret != AAC_ERR_NONE) {
        if (ret == AAC_ERR_EOF) {
            OS_LOGV(TAG, "AAC frame end");
            ret = AEL_IO_DONE;
        }
        return ret;
    }

    UCHAR *in = (UCHAR *)(decoder->buf_in.data);
    INT_PCM *out = (INT_PCM *)(decoder->buf_out.data);
    UINT size = decoder->buf_in.size_read;
    UINT left = decoder->buf_in.size_read;

    AAC_DECODER_ERROR err = aacDecoder_Fill(decoder->handle, &in, &size, &left);
    if (err != AAC_DEC_OK) {
        OS_LOGE(TAG, "Failed to fill data:0x%x", err);
        return AEL_PROCESS_FAIL;
    }

    err = aacDecoder_DecodeFrame(decoder->handle, out, AAC_DECODER_OUTPUT_BUFFER_SIZE / sizeof(INT_PCM), 0);
    if (err != AAC_DEC_OK) {
        OS_LOGE(TAG, "Failed to decode data:0x%x", err);
        if (decode_fail_cnt++ >= 4)
            return AEL_PROCESS_FAIL;
        goto fill_data;
    }
    decoder->buf_in.size_read = left;

    CStreamInfo *frame_info = aacDecoder_GetStreamInfo(decoder->handle);
    if (frame_info == NULL || frame_info->sampleRate <= 0 || frame_info->numChannels <= 0) {
        OS_LOGE(TAG, "Failed to get stream info");
        return AEL_PROCESS_FAIL;
    }
    decoder->buf_out.length = sizeof(INT_PCM)*frame_info->frameSize*frame_info->numChannels;

    if (!decoder->parsed_header) {
        audio_element_info_t info = {0};
        info.out_samplerate = frame_info->sampleRate;
        info.out_channels   = frame_info->numChannels;
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
    HANDLE_AACDECODER hDecoder = aacDecoder_Open(TT_MP4_ADTS, 1);
    if (hDecoder == NULL) {
        OS_LOGE(TAG, "Failed to open aac decoder");
        return -1;
    }
    decoder->handle = hDecoder;
    return 0;
}

void aac_wrapper_deinit(aac_decoder_handle_t decoder)
{
    if (decoder->handle != NULL) {
        aacDecoder_Close((HANDLE_AACDECODER)(decoder->handle));
        decoder->handle = NULL;
    }
}

static int m4a_mdat_read(m4a_decoder_handle_t handle)
{
    unsigned int stsz_entries = handle->m4a_info.stszsize;
    unsigned int stsz_current = handle->stsz_current;
    char *data = handle->buf_in.data;
    int size_read = handle->buf_in.size_read;
    int size_want = handle->buf_in.size_want;
    int bytes_read = 0;
    unsigned short frame_size = 0;

    if (size_read < size_want) {
        bytes_read = audio_element_input(handle->el, &data[size_read], size_want-size_read);

        if (bytes_read >= 0) {
            handle->buf_in.size_read += bytes_read;
            if (bytes_read < size_want - size_read) {
                OS_LOGW(TAG, "Remain size_read timeout with less data");
                return AEL_IO_TIMEOUT;
            }
            else if (bytes_read == size_want - size_read) {
                goto finish;
            }
            else {
                OS_LOGD(TAG, "Remain size_read error, stop decode");
                return AAC_ERR_EOF;
            }
        }
        else {
            OS_LOGE(TAG, "Remain size_read fail, ret=%d", bytes_read);
            return bytes_read;
        }
    }

    /* Last size_read success */
    handle->buf_in.size_read = 0;
    handle->buf_in.size_want = 0;

    if (stsz_current >= stsz_entries)
        return AAC_ERR_EOF;

    frame_size = handle->m4a_info.stszdata[stsz_current];
    handle->buf_in.size_want = frame_size;

    /* Newly wanted size */
    bytes_read = audio_element_input(handle->el, data, frame_size);
    if (bytes_read >= 0) {
        handle->buf_in.size_read += bytes_read;
        if (bytes_read < frame_size) {
            OS_LOGW(TAG, "Newly size_read timeout with less data[%d]", bytes_read);
            return AEL_IO_TIMEOUT;
        }
        else if (bytes_read == frame_size) {
            goto finish;
        }
        else {
            OS_LOGD(TAG, "Newly size_read error, stop decode");
            return AAC_ERR_EOF;
        }
    }
    else {
        OS_LOGE(TAG, "Newly size_read fail, ret=%d", bytes_read);
        return bytes_read;
    }

finish:
    handle->stsz_current += 1;
    handle->size_proced  += frame_size;
    return AAC_ERR_NONE;
}

int m4a_wrapper_run(m4a_decoder_handle_t decoder)
{
    int ret = m4a_mdat_read(decoder);
    if (ret != AAC_ERR_NONE) {
        if (ret == AAC_ERR_EOF) {
            OS_LOGV(TAG, "M4A frame end");
            ret = AEL_IO_DONE;
        }
        return ret;
    }

    UCHAR *in = (UCHAR *)(decoder->buf_in.data);
    INT_PCM *out = (INT_PCM *)(decoder->buf_out.data);
    UINT size = decoder->buf_in.size_want;
    UINT left = decoder->buf_in.size_want;

    AAC_DECODER_ERROR err = aacDecoder_Fill(decoder->handle, &in, &size, &left);
    if (err != AAC_DEC_OK) {
        OS_LOGE(TAG, "Failed to fill data:0x%x", err);
        return AEL_PROCESS_FAIL;
    }

    err = aacDecoder_DecodeFrame(decoder->handle, out, AAC_DECODER_OUTPUT_BUFFER_SIZE / sizeof(INT_PCM), 0);
    if (err != AAC_DEC_OK) {
        OS_LOGE(TAG, "Failed to decode data:0x%x", err);
        return AEL_PROCESS_FAIL;
    }

    if (left != 0) {
        OS_LOGW(TAG, "Left %d bytes not decoded", left);
    }

    CStreamInfo *frame_info = aacDecoder_GetStreamInfo(decoder->handle);
    if (frame_info == NULL || frame_info->sampleRate <= 0 || frame_info->numChannels <= 0) {
        OS_LOGE(TAG, "Failed to get stream info");
        return AEL_PROCESS_FAIL;
    }
    decoder->buf_out.length = sizeof(INT_PCM)*frame_info->frameSize*frame_info->numChannels;

    if (!decoder->parsed_header) {
        audio_element_info_t info = {0};
        info.out_samplerate = frame_info->sampleRate;
        info.out_channels   = frame_info->numChannels;
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
    HANDLE_AACDECODER hDecoder = aacDecoder_Open(TT_MP4_RAW, 1);
    if (hDecoder == NULL) {
        OS_LOGE(TAG, "Failed to open aac decoder");
        return -1;
    }

    UCHAR *asc_buf = (UCHAR *)(decoder->m4a_info.asc.buf);
    UINT asc_size = (UINT)(decoder->m4a_info.asc.size);
    AAC_DECODER_ERROR err = aacDecoder_ConfigRaw(hDecoder, &asc_buf, &asc_size);
    if (err != AAC_DEC_OK) {
        OS_LOGE(TAG, "Failed to decode the ASC");
        return -1;
    }

    decoder->handle = hDecoder;
    return 0;
}

void m4a_wrapper_deinit(m4a_decoder_handle_t decoder)
{
    if (decoder->handle != NULL) {
        aacDecoder_Close((HANDLE_AACDECODER)(decoder->handle));
        decoder->handle = NULL;
    }
}
