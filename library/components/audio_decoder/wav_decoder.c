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
#include <string.h>

#include "msgutils/os_logger.h"
#include "esp_adf/audio_element.h"
#include "esp_adf/audio_common.h"
#include "audio_extractor/wav_extractor.h"
#include "audio_decoder/wav_decoder.h"

#define TAG "WAV_DECODER"

#define WAV_DECODER_INPUT_TIMEOUT_MAX  200

typedef struct wav_decoder {
    bool parsed_header;
} wav_decoder_t;

static esp_err_t wav_decoder_destroy(audio_element_handle_t self)
{
    wav_decoder_t *wav = (wav_decoder_t *)audio_element_getdata(self);
    OS_LOGV(TAG, "Destroy wav decoder");
    audio_free(wav);
    return ESP_OK;
}

static esp_err_t wav_decoder_open(audio_element_handle_t self)
{
    OS_LOGV(TAG, "Open wav decoder");
    return ESP_OK;
}

static esp_err_t wav_decoder_close(audio_element_handle_t self)
{
    OS_LOGV(TAG, "Close wav decoder");
    if (AEL_STATE_PAUSED != audio_element_get_state(self)) {
        audio_element_info_t info = {0};
        audio_element_getinfo(self, &info);
        info.byte_pos = 0;
        info.total_bytes = 0;
        audio_element_setinfo(self, &info);
        wav_decoder_t *wav = (wav_decoder_t *)audio_element_getdata(self);
        wav->parsed_header = false;
    }
    return ESP_OK;
}

static int wav_decoder_process(audio_element_handle_t self, char *in_buffer, int in_len)
{
    wav_decoder_t *wav = (wav_decoder_t *)audio_element_getdata(self);
    int r_size = audio_element_input(self, in_buffer, in_len);
    int out_len = r_size;

    if (r_size > 0) {
        audio_element_info_t info = { 0 };
        audio_element_getinfo(self, &info);

        if (!wav->parsed_header) {
            wav->parsed_header = true;
            wav_info_t wav_info;
            if (wav_parse_header(in_buffer, r_size, &wav_info) == 0) {
                int remain_data = r_size - wav_info.dataOffset;

                info.out_samplerate = wav_info.sampleRate;
                info.out_channels = wav_info.channels;
                info.bits = wav_info.bits;
                info.total_bytes = wav_info.dataSize;
                info.byte_pos = remain_data;

                audio_element_setinfo(self, &info);
                audio_element_report_info(self);
                if (remain_data > 0) {
                    audio_element_output(self, in_buffer + wav_info.dataOffset, remain_data);
                    return remain_data;
                }
                return r_size;
            }
        }

        if (info.total_bytes > 0 && info.byte_pos + r_size >= info.total_bytes)
            out_len = info.total_bytes - info.byte_pos;
        out_len = audio_element_output(self, in_buffer, out_len);

        info.byte_pos += out_len;
        audio_element_setinfo(self, &info);
    }

    if (out_len != r_size)
        return ESP_OK;
    return out_len;
}

static esp_err_t wav_decoder_seek(audio_element_handle_t self, long long offset)
{
    wav_decoder_t *wav = (wav_decoder_t *)audio_element_getdata(self);
    wav->parsed_header = true;
    return ESP_OK;
}

audio_element_handle_t wav_decoder_init(wav_decoder_cfg_t *config)
{
    OS_LOGV(TAG, "Init wav decoder");

    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.destroy     = wav_decoder_destroy;
    cfg.open        = wav_decoder_open;
    cfg.close       = wav_decoder_close;
    cfg.process     = wav_decoder_process;
    cfg.seek        = wav_decoder_seek;
    cfg.buffer_len  = WAV_DECODER_BUFFER_SIZE;

    cfg.task_stack  = config->task_stack;
    cfg.task_prio   = config->task_prio;
    cfg.out_rb_size = config->out_rb_size;
    if (cfg.task_stack == 0)
        cfg.task_stack = WAV_DECODER_TASK_STACK;
    cfg.tag = "wav_dec";

    wav_decoder_t *wav = audio_calloc(1, sizeof(wav_decoder_t));
    if (wav == NULL)
        return NULL;

    audio_element_handle_t el = audio_element_init(&cfg);
    AUDIO_MEM_CHECK(TAG, el, goto wav_init_error);

    audio_element_setdata(el, wav);

    audio_element_info_t info = { 0 };
    memset(&info, 0x0, sizeof(info));
    audio_element_setinfo(el, &info);

    audio_element_set_input_timeout(el, WAV_DECODER_INPUT_TIMEOUT_MAX);
    return el;

wav_init_error:
    audio_free(wav);
    return NULL;
}
