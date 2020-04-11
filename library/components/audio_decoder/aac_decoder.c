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
#include <stdint.h>

#include "msgutils/os_logger.h"
#include "esp_adf/audio_common.h"
#include "esp_adf/audio_element.h"
#include "audio_decoder/aac_decoder.h"

#define TAG "AAC_DECODER"

#define AAC_DECODER_INPUT_TIMEOUT_MAX  200

static esp_err_t aac_decoder_destroy(audio_element_handle_t self)
{
    aac_decoder_handle_t decoder = (aac_decoder_handle_t)audio_element_getdata(self);
    OS_LOGV(TAG, "Destroy aac decoder");

    if (decoder->handle != NULL)
        aac_wrapper_deinit(decoder);
    if (decoder->buf_in.data != NULL)
        audio_free(decoder->buf_in.data);
    if (decoder->buf_out.data != NULL)
        audio_free(decoder->buf_out.data);
    audio_free(decoder);
    return ESP_OK;
}

static esp_err_t aac_decoder_open(audio_element_handle_t self)
{
    esp_err_t err = ESP_OK;
    aac_decoder_handle_t decoder = (aac_decoder_handle_t)audio_element_getdata(self);

    if (decoder->handle != NULL) {
        OS_LOGD(TAG, "AAC decoder already opened");
        return ESP_OK;
    }

    OS_LOGV(TAG, "Open aac decoder");

    decoder->buf_in.data = audio_calloc(AAC_DECODER_INPUT_BUFFER_SIZE, sizeof(char));
    if (decoder->buf_in.data == NULL) {
        OS_LOGE(TAG, "Failed to allocate input buffer");
        return ESP_ERR_NO_MEM;
    }

    decoder->buf_out.data = audio_calloc(AAC_DECODER_OUTPUT_BUFFER_SIZE, sizeof(char));
    if (decoder->buf_out.data == NULL) {
        OS_LOGE(TAG, "Failed to allocate output buffer");
        return ESP_ERR_NO_MEM;
    }

    err = aac_wrapper_init(decoder);
    return err;
}

static esp_err_t aac_decoder_close(audio_element_handle_t self)
{
    aac_decoder_handle_t decoder = (aac_decoder_handle_t)audio_element_getdata(self);

    if (audio_element_get_state(self) != AEL_STATE_PAUSED) {
        OS_LOGV(TAG, "Close aac decoder");
        aac_wrapper_deinit(decoder);
        if (decoder->buf_in.data != NULL)
            audio_free(decoder->buf_in.data);
        memset(&decoder->buf_in, 0x0, sizeof(aac_buf_in_t));
        if (decoder->buf_out.data != NULL)
            audio_free(decoder->buf_out.data);
        memset(&decoder->buf_out, 0x0, sizeof(aac_buf_out_t));

        decoder->handle = NULL;
        decoder->parsed_header = false;

        audio_element_info_t info = {0};
        audio_element_getinfo(self, &info);
        info.byte_pos = 0;
        info.total_bytes = 0;
        audio_element_setinfo(self, &info);
    }

    return ESP_OK;
}

static int aac_decoder_process(audio_element_handle_t self, char *in_buffer, int in_len)
{
    int byte_write = 0;
    int ret = AEL_IO_FAIL;
    aac_decoder_handle_t decoder = (aac_decoder_handle_t)audio_element_getdata(self);

    if (decoder->buf_out.length > 0) {
        /* Output buffer have remain data */
        byte_write = audio_element_output(self,
                        (char*)(decoder->buf_out.data+decoder->buf_out.offset),
                        decoder->buf_out.length);
    } else {
        /* More data need to be wrote */
        ret = aac_wrapper_run(decoder);
        if (ret < 0) {
            if (ret == AEL_IO_TIMEOUT) {
                OS_LOGW(TAG, "aac_wrapper_run AEL_IO_TIMEOUT");
            }
            else if (ret != AEL_IO_DONE) {
                OS_LOGE(TAG, "aac_wrapper_run failed:%d", ret);
            }
            return ret;
        }

        //OS_LOGV(TAG, "ret=%d, length=%d", ret, decoder->buf_out.length);
        decoder->buf_out.offset = 0;
        byte_write = audio_element_output(self, (char*)decoder->buf_out.data, decoder->buf_out.length);
    }

    if (byte_write > 0) {
        decoder->buf_out.length -= byte_write;
        decoder->buf_out.offset += byte_write;

        audio_element_info_t audio_info = {0};
        audio_element_getinfo(self, &audio_info);
        audio_info.byte_pos += byte_write;
        audio_element_setinfo(self, &audio_info);
    }

    return byte_write;
}

audio_element_handle_t aac_decoder_init(aac_decoder_cfg_t *config)
{
    OS_LOGV(TAG, "Init aac decoder");

    aac_decoder_handle_t decoder = audio_calloc(1, sizeof(struct aac_decoder));
    AUDIO_MEM_CHECK(TAG, decoder, return NULL);

    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.destroy = aac_decoder_destroy;
    cfg.process = aac_decoder_process;
    cfg.open    = aac_decoder_open;
    cfg.close   = aac_decoder_close;
    cfg.buffer_len = AAC_DECODER_BUFFER_SIZE;
   
    cfg.task_stack  = config->task_stack;
    cfg.task_prio   = config->task_prio;
    cfg.out_rb_size = config->out_rb_size;
    if (cfg.task_stack == 0)
        cfg.task_stack = AAC_DECODER_TASK_STACK;

    cfg.tag = "aac";

    audio_element_handle_t el = audio_element_init(&cfg);
    AUDIO_MEM_CHECK(TAG, el, goto aac_init_error);

    decoder->el = el;
    audio_element_setdata(el, decoder);

    audio_element_set_input_timeout(el, AAC_DECODER_INPUT_TIMEOUT_MAX);
    return el;

aac_init_error:
    audio_free(decoder);
    return NULL;
}
