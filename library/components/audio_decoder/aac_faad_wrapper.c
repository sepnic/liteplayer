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
#include "aac-faad/neaacdec.h"

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

#define AAC_ADTS_FRAME_HEADER_SIZE 9

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
    NeAACDecFrameInfo frameInfo = {0};
    void *sample_buffer = NULL;
    int decode_fail_cnt = 0;
    int ret = 0;

fill_data:
    ret = aac_adts_read(decoder);
    if (ret != AAC_ERR_NONE) {
        if (ret == AAC_ERR_EOF) {
            OS_LOGV(TAG, "AAC frame end");
            ret = AEL_IO_DONE;
        }
        return ret;
    }

    unsigned char *in = (unsigned char *)(decoder->buf_in.data);
    void *out = (void *)(decoder->buf_out.data);
    int size = decoder->buf_in.size_read;

    if (!decoder->parsed_header) {
        if (size < AAC_ADTS_FRAME_HEADER_SIZE) {
            return AEL_PROCESS_FAIL;
        }
        unsigned long samplerate = 0;
        unsigned char channels = 0;
        unsigned char frame_header[AAC_ADTS_FRAME_HEADER_SIZE] = {0};
        memcpy(frame_header, in, sizeof(frame_header));
        if (NeAACDecInit(decoder->handle, frame_header, sizeof(frame_header), &samplerate, &channels) < 0) {
            OS_LOGE(TAG, "Failed to NeAACDecInit");
            return AEL_PROCESS_FAIL;
        }

        audio_element_info_t info = {0};
        info.out_samplerate = samplerate;
        info.out_channels   = channels;
        info.bits           = 16;
        audio_element_setinfo(decoder->el, &info);
        audio_element_report_info(decoder->el);
        OS_LOGV(TAG,"Found aac header: SR=%d, CH=%d", info.out_samplerate, info.out_channels);
        decoder->parsed_header = true;
    }

    sample_buffer = NeAACDecDecode2(decoder->handle, &frameInfo, in, size, &out, AAC_DECODER_OUTPUT_BUFFER_SIZE);
    if (sample_buffer == NULL || frameInfo.samples == 0) {
        OS_LOGW(TAG, "NeAACDecDecode2 error, frame samples: %d", frameInfo.samples);
        if (decode_fail_cnt++ >= 4)
            return AEL_PROCESS_FAIL;
        goto fill_data;
    }

    decoder->buf_in.size_read -= frameInfo.bytesconsumed;

    if (frameInfo.error == 0) {
        decoder->buf_out.length = frameInfo.samples * 16/8;
        return 0;
    }
    else {
        OS_LOGE(TAG, "NeAACDecDecode2 error: %s", NeAACDecGetErrorMessage(frameInfo.error));
        return AEL_PROCESS_FAIL;
    }
}

int aac_wrapper_init(aac_decoder_handle_t decoder)
{
    NeAACDecConfigurationPtr config;
    NeAACDecHandle hDecoder = NeAACDecOpen();
    decoder->handle = hDecoder;
    if (hDecoder == NULL)
        return -1;

    /* Set configuration */
    config = NeAACDecGetCurrentConfiguration(hDecoder);
    config->outputFormat = FAAD_FMT_16BIT;
    config->downMatrix = 0;
    NeAACDecSetConfiguration(hDecoder, config);
    return 0;
}

void aac_wrapper_deinit(aac_decoder_handle_t decoder)
{
    if (decoder->handle != NULL) {
        NeAACDecClose((NeAACDecHandle)(decoder->handle));
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
                goto read_done;
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
            goto read_done;
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

read_done:
    handle->stsz_current += 1;
    handle->size_proced  += frame_size;
    return AAC_ERR_NONE;
}

int m4a_wrapper_run(m4a_decoder_handle_t decoder)
{
    NeAACDecFrameInfo frameInfo = {0};
    void *sample_buffer = NULL;
    int decode_fail_cnt = 0;
    int ret = 0;

fill_data:
    ret = m4a_mdat_read(decoder);
    if (ret != AAC_ERR_NONE) {
        if (ret == AAC_ERR_EOF) {
            OS_LOGV(TAG, "M4A frame end");
            ret = AEL_IO_DONE;
        }
        return ret;
    }

    unsigned char *in = (unsigned char *)(decoder->buf_in.data);
    void *out = (void *)(decoder->buf_out.data);
    int size = decoder->buf_in.size_want;

    sample_buffer = NeAACDecDecode2(decoder->handle, &frameInfo, in, size, &out, AAC_DECODER_OUTPUT_BUFFER_SIZE);
    if (sample_buffer == NULL || frameInfo.samples == 0) {
        OS_LOGW(TAG, "NeAACDecDecode2 error, frame samples: %d", frameInfo.samples);
        if (decode_fail_cnt++ >= 4)
            return AEL_PROCESS_FAIL;
        goto fill_data;
    }

    if (frameInfo.error == 0) {
        decoder->buf_out.length = frameInfo.samples * decoder->m4a_info.bits/8;
        return 0;
    }
    else {
        OS_LOGE(TAG, "NeAACDecDecode2 error: %s", NeAACDecGetErrorMessage(frameInfo.error));
        return AEL_PROCESS_FAIL;
    }
}

int m4a_wrapper_init(m4a_decoder_handle_t decoder)
{
    NeAACDecConfigurationPtr config;
    mp4AudioSpecificConfig mp4asc;
    unsigned long samplerate = 0;
    unsigned char channels = 0;

    NeAACDecHandle hDecoder = NeAACDecOpen();
    decoder->handle = hDecoder;
    if (hDecoder == NULL)
        return -1;

    /* Set configuration */
    config = NeAACDecGetCurrentConfiguration(hDecoder);
    config->outputFormat = FAAD_FMT_16BIT;
    config->downMatrix = 0;
    NeAACDecSetConfiguration(hDecoder, config);

    if (NeAACDecInit2(hDecoder, decoder->m4a_info.asc.buf, decoder->m4a_info.asc.size, &samplerate, &channels) < 0) {
        OS_LOGE(TAG, "Failed to NeAACDecInit2");
        goto init_failed;
    }

    if (NeAACDecAudioSpecificConfig(decoder->m4a_info.asc.buf, decoder->m4a_info.asc.size, &mp4asc) < 0) {
        OS_LOGE(TAG, "Failed to NeAACDecAudioSpecificConfig");
        goto init_failed;
    }

    if (decoder->parsed_header == false) {
        audio_element_info_t info = {0};
        info.out_samplerate = mp4asc.samplingFrequency;
        info.out_channels   = mp4asc.channelsConfiguration;
        info.bits           = decoder->m4a_info.bits;
        audio_element_setinfo(decoder->el, &info);
        audio_element_report_info(decoder->el);
        decoder->parsed_header = true;
        OS_LOGV(TAG,"Found aac header: SR=%d, CH=%d", info.out_samplerate, info.out_channels);
    }
    return 0;

init_failed:
    NeAACDecClose((NeAACDecHandle)(decoder->handle));
    decoder->handle = NULL;
    return -1;
}

void m4a_wrapper_deinit(m4a_decoder_handle_t decoder)
{
    if (decoder->handle != NULL) {
        NeAACDecClose((NeAACDecHandle)(decoder->handle));
        decoder->handle = NULL;
    }
}
