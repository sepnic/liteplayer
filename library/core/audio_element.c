/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "esp_adf/queue.h"
#include "esp_adf/esp_log.h"
#include "esp_adf/audio_event_iface.h"
#include "esp_adf/audio_element.h"
#include "esp_adf/audio_common.h"

#define TAG  "AUDIO_ELEMENT"

#define DEFAULT_WAIT_TIMEOUT_MS 12000 // ms

/**
 *  I/O Element Abstract
 */
typedef struct io_callback {
    stream_func                 cb;
    void                        *ctx;
} io_callback_t;

/**
 *  Audio Callback Abstract
 */
typedef struct audio_callback {
    event_cb_func               cb;
    void                        *ctx;
} audio_callback_t;

typedef struct audio_multi_rb {
    ringbuf_handle_t            *rb;
    int                         max_rb_num;
} audio_multi_rb_t;

typedef enum {
    IO_TYPE_RB = 1, /* I/O through ringbuffer */
    IO_TYPE_CB,     /* I/O through callback */
} io_type_t;

typedef enum {
    EVENTS_TYPE_Q = 1,  /* Events through MessageQueue */
    EVENTS_TYPE_CB,     /* Events through Callback function */
} events_type_t;

struct audio_element {
    /* Functions/RingBuffers */
    io_func                     open;
    io_func                     seek;
    process_func                process;
    io_func                     close;
    io_func                     destroy;
    io_type_t                   read_type;
    union {
        ringbuf_handle_t        input_rb;
        io_callback_t           read_cb;
    } in;
    io_type_t                   write_type;
    union {
        ringbuf_handle_t        output_rb;
        io_callback_t           write_cb;
    } out;

    audio_multi_rb_t            multi_in;
    audio_multi_rb_t            multi_out;

    /* Properties */
    bool                        is_open;
    audio_element_state_t       state;

    events_type_t               events_type;
    audio_event_iface_handle_t  iface_event;
    audio_callback_t            callback_event;

    int                         buf_size;
    char                        *buf;

    char                        *tag;
    int                         task_stack;
    int                         task_prio;
    os_mutex_t                  info_lock;
    audio_element_info_t        info;
    audio_element_info_t        *report_info;

    /* PrivateData */
    void                        *data;
    os_cond_t                   state_cond;
    os_mutex_t                  state_lock;
    int                         state_event;
    bool                        buffer_reach_level;
    int                         input_timeout_ms;
    int                         output_timeout_ms;
    int                         out_buf_size_expect;
    int                         out_rb_size;
    bool                        is_running;
    bool                        task_run;
    bool                        stopping;
};

const static int STOPPED_BIT            = (1 << 0);
const static int STARTED_BIT            = (1 << 1);
const static int BUFFER_REACH_LEVEL_BIT = (1 << 2);
const static int TASK_CREATED_BIT       = (1 << 3);
const static int TASK_DESTROYED_BIT     = (1 << 4);
const static int PAUSED_BIT             = (1 << 5);
const static int RESUMED_BIT            = (1 << 6);

static void audio_element_set_state_event(audio_element_handle_t el, int state_bit)
{
    OS_THREAD_MUTEX_LOCK(el->state_lock);

    el->state_event |= state_bit;
    if (state_bit == BUFFER_REACH_LEVEL_BIT)
        el->buffer_reach_level = true;
    OS_THREAD_COND_SIGNAL(el->state_cond);

    OS_THREAD_MUTEX_UNLOCK(el->state_lock);
}

static void audio_element_clear_state_event(audio_element_handle_t el, int state_bit)
{
    OS_THREAD_MUTEX_LOCK(el->state_lock);

    el->state_event &= ~state_bit;
    if (state_bit == BUFFER_REACH_LEVEL_BIT)
        el->buffer_reach_level = false;

    OS_THREAD_MUTEX_UNLOCK(el->state_lock);
}

static esp_err_t audio_element_force_set_state(audio_element_handle_t el, audio_element_state_t new_state)
{
    el->state = new_state;
    return ESP_OK;
}

static esp_err_t audio_element_cmd_send(audio_element_handle_t el, audio_element_msg_cmd_t cmd)
{
    audio_event_iface_msg_t msg = {
        .source = el,
        .source_type = AUDIO_ELEMENT_TYPE_ELEMENT,
        .cmd = cmd,
    };
    ESP_LOGV(TAG, "[%s]evt internal cmd = %d", el->tag, msg.cmd);
    return audio_event_iface_cmd(el->iface_event, &msg);
}

static esp_err_t audio_element_msg_sendout(audio_element_handle_t el, audio_event_iface_msg_t *msg)
{
    msg->source = el;
    msg->source_type = AUDIO_ELEMENT_TYPE_ELEMENT;
    if (el->events_type == EVENTS_TYPE_CB && el->callback_event.cb) {
        return el->callback_event.cb(el, msg, el->callback_event.ctx);
    }
    ESP_LOGV(TAG, "[%s]evt external cmd = %d", el->tag, msg->cmd);
    return audio_event_iface_sendout(el->iface_event, msg);
}

esp_err_t audio_element_process_init(audio_element_handle_t el)
{
    if (el->open == NULL) {
        el->is_open = true;
        audio_element_set_state_event(el, STARTED_BIT);
        return ESP_OK;
    }
    if (el->open(el) == ESP_OK) {
        ESP_LOGD(TAG, "[%s] el opened", el->tag);
        el->is_open = true;
        audio_element_force_set_state(el, AEL_STATE_RUNNING);
        audio_element_report_status(el, AEL_STATUS_STATE_RUNNING);
        audio_element_set_state_event(el, STARTED_BIT);
        return ESP_OK;
    }
    ESP_LOGE(TAG, "[%s] AEL_STATUS_ERROR_OPEN", el->tag);
    audio_element_report_status(el, AEL_STATUS_ERROR_OPEN);
    audio_element_cmd_send(el, AEL_MSG_CMD_ERROR);
    return ESP_FAIL;
}

esp_err_t audio_element_process_deinit(audio_element_handle_t el)
{
    if (el->is_open) {
        if (el->close)
            el->close(el);
        el->is_open = false;
        ESP_LOGD(TAG, "[%s] closed", el->tag);
    }
    return ESP_OK;
}

static esp_err_t audio_element_on_cmd(audio_event_iface_msg_t *msg, void *context)
{
    audio_element_handle_t el = (audio_element_handle_t)context;

    if (msg->source_type != AUDIO_ELEMENT_TYPE_ELEMENT) {
        ESP_LOGE(TAG, "[%s] Invalid event type, this event should be ELEMENT type", el->tag);
        return ESP_FAIL;
    }
    //process an event
    switch (msg->cmd) {
        case AEL_MSG_CMD_ERROR:
            audio_element_process_deinit(el);
            el->state = AEL_STATE_ERROR;
            audio_event_iface_set_cmd_waiting_timeout(el->iface_event, AUDIO_MAX_DELAY);
            audio_element_abort_input_ringbuf(el);
            audio_element_abort_output_ringbuf(el);
            el->is_running = false;
            audio_element_set_state_event(el, STOPPED_BIT);
            ESP_LOGE(TAG, "[%s] AEL_MSG_CMD_ERROR", el->tag);
            break;
        case AEL_MSG_CMD_FINISH:
            if ((el->state == AEL_STATE_ERROR)
                || (el->state == AEL_STATE_STOPPED)) {
                ESP_LOGV(TAG, "[%s] AEL_MSG_CMD_FINISH, state:%d", el->tag, el->state);
                break;
            }
            audio_element_process_deinit(el);
            el->state = AEL_STATE_FINISHED;
            audio_event_iface_set_cmd_waiting_timeout(el->iface_event, AUDIO_MAX_DELAY);
            audio_element_report_status(el, AEL_STATUS_STATE_FINISHED);
            el->is_running = false;
            audio_element_set_state_event(el, STOPPED_BIT);
            ESP_LOGV(TAG, "[%s] AEL_MSG_CMD_FINISH", el->tag);
            break;
        case AEL_MSG_CMD_STOP:
            if ((el->state != AEL_STATE_FINISHED) && (el->state != AEL_STATE_STOPPED)) {
                audio_element_process_deinit(el);
                el->state = AEL_STATE_STOPPED;
                audio_event_iface_set_cmd_waiting_timeout(el->iface_event, AUDIO_MAX_DELAY);
                audio_element_report_status(el, AEL_STATUS_STATE_STOPPED);
                el->is_running = false;
                el->stopping = false;
                ESP_LOGV(TAG, "[%s] AEL_MSG_CMD_STOP", el->tag);
                audio_element_set_state_event(el, STOPPED_BIT);
            } else {
                // Change element state to AEL_STATE_STOPPED, even if AEL_STATE_ERROR or AEL_STATE_FINISHED.
                ESP_LOGV(TAG, "[%s] AEL_MSG_CMD_STOP, state:%d", el->tag, el->state);
                el->state = AEL_STATE_STOPPED;
                el->is_running = false;
                el->stopping = false;
                audio_element_report_status(el, AEL_STATUS_STATE_STOPPED);
                audio_element_set_state_event(el, STOPPED_BIT);
            }
            break;
        case AEL_MSG_CMD_PAUSE:
            el->state = AEL_STATE_PAUSED;
            audio_element_process_deinit(el);
            audio_event_iface_set_cmd_waiting_timeout(el->iface_event, AUDIO_MAX_DELAY);
            audio_element_report_status(el, AEL_STATUS_STATE_PAUSED);
            el->is_running = false;
            ESP_LOGV(TAG, "[%s] AEL_MSG_CMD_PAUSE", el->tag);
            audio_element_set_state_event(el, PAUSED_BIT);
            break;
        case AEL_MSG_CMD_RESUME:
            ESP_LOGV(TAG, "[%s] AEL_MSG_CMD_RESUME,state:%d", el->tag, el->state);
            if (el->state == AEL_STATE_RUNNING) {
                el->is_running = true;
                audio_element_set_state_event(el, RESUMED_BIT);
                break;
            }
            if (el->state != AEL_STATE_INIT && el->state != AEL_STATE_RUNNING && el->state != AEL_STATE_PAUSED) {
                audio_element_reset_output_ringbuf(el);
            }
            if (audio_element_process_init(el) != ESP_OK) {
                audio_element_abort_output_ringbuf(el);
                audio_element_abort_input_ringbuf(el);
                audio_element_set_state_event(el, RESUMED_BIT);
                return ESP_FAIL;
            }
            audio_event_iface_set_cmd_waiting_timeout(el->iface_event, 0);
            audio_element_clear_state_event(el, STOPPED_BIT);
            el->is_running = true;
            audio_element_set_state_event(el, RESUMED_BIT);
            break;
        case AEL_MSG_CMD_DESTROY:
            el->task_run = false;
            el->is_running = false;
            ESP_LOGV(TAG, "[%s] AEL_MSG_CMD_DESTROY", el->tag);
            return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t audio_element_process_running(audio_element_handle_t el)
{
    int process_len = -1;
    if (el->state < AEL_STATE_RUNNING || !el->is_running) {
        return ESP_ERR_INVALID_STATE;
    }
    process_len = el->process(el, el->buf, el->buf_size);
    if (process_len <= 0) {
        switch (process_len) {
            case AEL_IO_ABORT:
                ESP_LOGE(TAG, "[%s] ERROR_PROCESS, AEL_IO_ABORT", el->tag);
                break;
            case AEL_IO_DONE:
            case AEL_IO_OK:
                // Re-open if reset_state function called
                if (audio_element_get_state(el) == AEL_STATE_INIT) {
                    el->is_open = false;
                    el->is_running = false;
                    audio_element_resume(el, 0, 0);
                    return ESP_OK;
                }
                audio_element_set_ringbuf_done(el);
                audio_element_cmd_send(el, AEL_MSG_CMD_FINISH);
                break;
            case AEL_IO_FAIL:
                ESP_LOGE(TAG, "[%s] ERROR_PROCESS, AEL_IO_FAIL", el->tag);
                audio_element_report_status(el, AEL_STATUS_ERROR_PROCESS);
                audio_element_cmd_send(el, AEL_MSG_CMD_ERROR);
                break;
            case AEL_IO_TIMEOUT:
                ESP_LOGV(TAG, "[%s] ERROR_PROCESS, AEL_IO_TIMEOUT", el->tag);
                break;
            case AEL_PROCESS_FAIL:
                ESP_LOGE(TAG, "[%s] ERROR_PROCESS, AEL_PROCESS_FAIL", el->tag);
                audio_element_report_status(el, AEL_STATUS_ERROR_PROCESS);
                audio_element_cmd_send(el, AEL_MSG_CMD_ERROR);
                break;
            default:
                ESP_LOGW(TAG, "[%s] Process return error,ret:%d", el->tag, process_len);
                break;
        }
    }
    return ESP_OK;
}

int audio_element_input(audio_element_handle_t el, char *buffer, int wanted_size)
{
    int in_len = 0;
    if (el->read_type == IO_TYPE_CB) {
        if (el->in.read_cb.cb == NULL) {
            ESP_LOGE(TAG, "[%s] Read IO Type callback but callback not set", el->tag);
            return ESP_FAIL;
        }
        in_len = el->in.read_cb.cb(el, buffer, wanted_size, el->input_timeout_ms,
                                   el->in.read_cb.ctx);
    } else if (el->read_type == IO_TYPE_RB) {
        if (el->in.input_rb == NULL) {
            ESP_LOGE(TAG, "[%s] Read IO type ringbuf but ringbuf not set", el->tag);
            return ESP_FAIL;
        }
        in_len = rb_read(el->in.input_rb, buffer, wanted_size, el->input_timeout_ms);
    } else {
        ESP_LOGE(TAG, "[%s] Invalid read IO type", el->tag);
        return ESP_FAIL;
    }
    if (in_len <= 0) {
        switch (in_len) {
            case AEL_IO_ABORT:
                ESP_LOGW(TAG, "IN-[%s] AEL_IO_ABORT", el->tag);
                audio_element_set_ringbuf_done(el);
                //audio_element_abort_output_ringbuf(el);
                audio_element_stop(el);
                break;
            case AEL_IO_DONE:
            case AEL_IO_OK:
                ESP_LOGI(TAG, "IN-[%s] AEL_IO_DONE,%d", el->tag, in_len);
                break;
            case AEL_IO_FAIL:
                ESP_LOGE(TAG, "IN-[%s] AEL_STATUS_ERROR_INPUT", el->tag);
                audio_element_report_status(el, AEL_STATUS_ERROR_INPUT);
                audio_element_cmd_send(el, AEL_MSG_CMD_ERROR);
                break;
            case AEL_IO_TIMEOUT:
                ESP_LOGV(TAG, "IN-[%s] AEL_IO_TIMEOUT", el->tag);
                break;
            default:
                ESP_LOGE(TAG, "IN-[%s] Input return not support,ret:%d", el->tag, in_len);
                audio_element_cmd_send(el, AEL_MSG_CMD_PAUSE);
                break;
        }
    }
    return in_len;
}

int audio_element_output(audio_element_handle_t el, char *buffer, int write_size)
{
    int output_len = 0;
    if (el->write_type == IO_TYPE_CB) {
        if (el->out.write_cb.cb && write_size) {
            output_len = el->out.write_cb.cb(el, buffer, write_size, el->output_timeout_ms,
                                             el->out.write_cb.ctx);
        }
    } else if (el->write_type == IO_TYPE_RB) {
        if (el->out.output_rb && write_size) {
            output_len = rb_write(el->out.output_rb, buffer, write_size, el->output_timeout_ms);
            if (!el->buffer_reach_level && ((rb_bytes_filled(el->out.output_rb) > el->out_buf_size_expect) || (output_len < 0))) {
                ESP_LOGV(TAG, "OUT-[%s] BUFFER_REACH_LEVEL_BIT, rb_bytes_filled:%d, out_buf_size_expect:%d, output_len:%d",
                         el->tag, rb_bytes_filled(el->out.output_rb), el->out_buf_size_expect, output_len);
                audio_element_set_state_event(el, BUFFER_REACH_LEVEL_BIT);
            }
        }
    }
    if (output_len <= 0) {
        switch (output_len) {
            case AEL_IO_ABORT:
                ESP_LOGW(TAG, "OUT-[%s] AEL_IO_ABORT", el->tag);
                audio_element_set_ringbuf_done(el);
                //audio_element_abort_input_ringbuf(el);
                audio_element_stop(el);
                break;
            case AEL_IO_DONE:
            case AEL_IO_OK:
                ESP_LOGI(TAG, "OUT-[%s] AEL_IO_DONE,%d", el->tag, output_len);
                break;
            case AEL_IO_FAIL:
                ESP_LOGE(TAG, "OUT-[%s] AEL_STATUS_ERROR_OUTPUT", el->tag);
                audio_element_report_status(el, AEL_STATUS_ERROR_OUTPUT);
                audio_element_cmd_send(el, AEL_MSG_CMD_ERROR);
                break;
            case AEL_IO_TIMEOUT:
                ESP_LOGW(TAG, "OUT-[%s] AEL_IO_TIMEOUT", el->tag);
                audio_element_cmd_send(el, AEL_MSG_CMD_PAUSE);
                break;
            default:
                ESP_LOGE(TAG, "OUT-[%s] Output return not support,ret:%d", el->tag, output_len);
                audio_element_cmd_send(el, AEL_MSG_CMD_PAUSE);
                break;
        }
    }
    return output_len;
}
void *audio_element_task(void *pv)
{
    audio_element_handle_t el = (audio_element_handle_t)pv;
    el->task_run = true;
    audio_element_set_state_event(el, TASK_CREATED_BIT);
    audio_element_force_set_state(el, AEL_STATE_INIT);
    audio_event_iface_set_cmd_waiting_timeout(el->iface_event, AUDIO_MAX_DELAY);
    if (el->buf_size > 0) {
        el->buf = audio_malloc(el->buf_size);
        AUDIO_MEM_CHECK(TAG, el->buf, {
            el->task_run = false;
            ESP_LOGE(TAG, "[%s] Error malloc element buffer", el->tag);
        });
    }
    audio_element_clear_state_event(el, STOPPED_BIT);
    while (el->task_run) {
        if (audio_event_iface_waiting_cmd_msg(el->iface_event) != ESP_OK) {
            audio_element_set_state_event(el, STOPPED_BIT);
            break;
        }
        if (audio_element_process_running(el) != ESP_OK) {
            // continue;
        }
    }

    if (el->is_open) {
        if (el->close)
            el->close(el);
        el->is_open = false;
        ESP_LOGD(TAG, "[%s] el closed", el->tag);
    }
    audio_free(el->buf);
    ESP_LOGV(TAG, "[%s] el task deleted,%p", el->tag, OS_THREAD_SELF());
    el->task_run = false;
    audio_element_set_state_event(el, TASK_DESTROYED_BIT);
    return NULL;
}

esp_err_t audio_element_reset_state(audio_element_handle_t el)
{
    return audio_element_force_set_state(el, AEL_STATE_INIT);
}

audio_element_state_t audio_element_get_state(audio_element_handle_t el)
{
    return el->state;
}

mqueue_t audio_element_get_event_queue(audio_element_handle_t el)
{
    if (!el) {
        return NULL;
    }
    return audio_event_iface_get_queue_handle(el->iface_event);
}

esp_err_t audio_element_setdata(audio_element_handle_t el, void *data)
{
    el->data = data;
    return ESP_OK;
}

void *audio_element_getdata(audio_element_handle_t el)
{
    return el->data;
}

esp_err_t audio_element_set_tag(audio_element_handle_t el, const char *tag)
{
    if (el->tag) {
        audio_free(el->tag);
        el->tag = NULL;
    }

    if (tag) {
        el->tag = audio_strdup(tag);
        AUDIO_MEM_CHECK(TAG, el->tag, {
            return ESP_ERR_NO_MEM;
        });
    }
    return ESP_OK;
}

char *audio_element_get_tag(audio_element_handle_t el)
{
    return el->tag;
}

esp_err_t audio_element_set_uri(audio_element_handle_t el, const char *uri)
{
    if (el->info.uri) {
        audio_free(el->info.uri);
        el->info.uri = NULL;
    }

    if (uri) {
        el->info.uri = audio_strdup(uri);
        AUDIO_MEM_CHECK(TAG, el->info.uri, {
            return ESP_ERR_NO_MEM;
        });
    }
    return ESP_OK;
}

char *audio_element_get_uri(audio_element_handle_t el)
{
    return el->info.uri;
}

esp_err_t audio_element_set_event_callback(audio_element_handle_t el, event_cb_func cb_func, void *ctx)
{
    el->events_type = EVENTS_TYPE_CB;
    el->callback_event.cb = cb_func;
    el->callback_event.ctx = ctx;
    return ESP_OK;
}

esp_err_t audio_element_msg_set_listener(audio_element_handle_t el, audio_event_iface_handle_t listener)
{
    return audio_event_iface_set_listener(el->iface_event, listener);
}

esp_err_t audio_element_msg_remove_listener(audio_element_handle_t el, audio_event_iface_handle_t listener)
{
    return audio_event_iface_remove_listener(listener, el->iface_event);
}

esp_err_t audio_element_setinfo(audio_element_handle_t el, audio_element_info_t *info)
{
    if (info && el) {
        //FIXME: We will got reset if lock mutex here
        OS_THREAD_MUTEX_LOCK(el->info_lock);
        memcpy(&el->info, info, sizeof(audio_element_info_t));
        OS_THREAD_MUTEX_UNLOCK(el->info_lock);
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t audio_element_getinfo(audio_element_handle_t el, audio_element_info_t *info)
{
    if (info && el) {
        OS_THREAD_MUTEX_LOCK(el->info_lock);
        memcpy(info, &el->info, sizeof(audio_element_info_t));
        OS_THREAD_MUTEX_UNLOCK(el->info_lock);
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t audio_element_report_info(audio_element_handle_t el)
{
    if (el) {
        audio_event_iface_msg_t msg = { 0 };
        msg.cmd = AEL_MSG_CMD_REPORT_MUSIC_INFO;
        msg.data = NULL;
        ESP_LOGV(TAG, "REPORT_INFO,[%s]evt out cmd:%d,", el->tag, msg.cmd);
        audio_element_msg_sendout(el, &msg);
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t audio_element_report_codec_fmt(audio_element_handle_t el)
{
    if (el) {
        audio_event_iface_msg_t msg = { 0 };
        msg.cmd = AEL_MSG_CMD_REPORT_CODEC_FMT;
        msg.data = NULL;
        ESP_LOGV(TAG, "REPORT_FMT,[%s]evt out cmd:%d,", el->tag, msg.cmd);
        audio_element_msg_sendout(el, &msg);
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t audio_element_report_status(audio_element_handle_t el, audio_element_status_t status)
{
    audio_event_iface_msg_t msg = { 0 };
    msg.cmd = AEL_MSG_CMD_REPORT_STATUS;
    msg.data = (void *)status;
    msg.data_len = sizeof(status);
    ESP_LOGV(TAG, "REPORT_STATUS,[%s]evt out cmd = %d,status:%d", el->tag, msg.cmd, status);
    return audio_element_msg_sendout(el, &msg);
}

esp_err_t audio_element_report_pos(audio_element_handle_t el)
{
    if (el) {
        audio_event_iface_msg_t msg = { 0 };
        msg.cmd = AEL_MSG_CMD_REPORT_POSITION;
        msg.data = (void *)el->info.byte_pos;
        msg.data_len = sizeof(el->info.byte_pos);
        //ESP_LOGV(TAG, "REPORT_POS,[%s]evt out cmd:%d,pos:%ld", el->tag, msg.cmd, (long)el->info.byte_pos);
        audio_element_msg_sendout(el, &msg);
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t audio_element_change_cmd(audio_element_handle_t el, audio_element_msg_cmd_t cmd)
{
    AUDIO_NULL_CHECK(TAG, el, return ESP_ERR_INVALID_ARG);
    return audio_element_cmd_send(el, cmd);
}

esp_err_t audio_element_reset_input_ringbuf(audio_element_handle_t el)
{
    if (el->read_type != IO_TYPE_RB) {
        return ESP_FAIL;
    }
    if (el->in.input_rb) {
        rb_reset(el->in.input_rb);
        for (int i = 0; i < el->multi_in.max_rb_num; ++i) {
            if (el->multi_in.rb[i]) {
                rb_reset(el->multi_in.rb[i]);
            }
        }
    }
    return ESP_OK;
}

esp_err_t audio_element_reset_output_ringbuf(audio_element_handle_t el)
{
    if (el->write_type != IO_TYPE_RB) {
        return ESP_FAIL;
    }
    if (el->out.output_rb) {
        rb_reset(el->out.output_rb);
        for (int i = 0; i < el->multi_out.max_rb_num; ++i) {
            if (el->multi_out.rb[i]) {
                rb_reset(el->multi_out.rb[i]);
            }
        }
    }
    return ESP_OK;
}

esp_err_t audio_element_abort_input_ringbuf(audio_element_handle_t el)
{
    if (el->read_type != IO_TYPE_RB) {
        return ESP_FAIL;
    }
    if (el->in.input_rb) {
        rb_abort(el->in.input_rb);
        for (int i = 0; i < el->multi_in.max_rb_num; ++i) {
            if (el->multi_in.rb[i]) {
                rb_abort(el->multi_in.rb[i]);
            }
        }
    }
    return ESP_OK;
}

esp_err_t audio_element_abort_output_ringbuf(audio_element_handle_t el)
{
    if (el->write_type != IO_TYPE_RB) {
        return ESP_FAIL;
    }
    if (el->out.output_rb) {
        rb_abort(el->out.output_rb);
        for (int i = 0; i < el->multi_out.max_rb_num; ++i) {
            if (el->multi_out.rb[i]) {
                rb_abort(el->multi_out.rb[i]);
            }
        }
    }
    return ESP_OK;
}

esp_err_t audio_element_set_ringbuf_done(audio_element_handle_t el)
{
    if (NULL == el) {
        return ESP_FAIL;
    }
    if (el->out.output_rb && el->write_type == IO_TYPE_RB) {
        rb_done_write(el->out.output_rb);
        for (int i = 0; i < el->multi_out.max_rb_num; ++i) {
            if (el->multi_out.rb[i]) {
                rb_done_write(el->multi_out.rb[i]);
            }
        }
    }
    return ESP_OK;
}

esp_err_t audio_element_set_input_ringbuf(audio_element_handle_t el, ringbuf_handle_t rb)
{
    if (rb) {
        el->in.input_rb = rb;
        el->read_type = IO_TYPE_RB;
    } else if (el->read_type == IO_TYPE_RB) {
        el->in.input_rb = rb;
    }
    return ESP_OK;
}

ringbuf_handle_t audio_element_get_input_ringbuf(audio_element_handle_t el)
{
    if (el->read_type == IO_TYPE_RB) {
        return el->in.input_rb;
    } else {
        return NULL;
    }
}

esp_err_t audio_element_set_output_ringbuf(audio_element_handle_t el, ringbuf_handle_t rb)
{
    if (rb) {
        el->out.output_rb = rb;
        el->write_type = IO_TYPE_RB;
    } else if (el->write_type == IO_TYPE_RB) {
        el->out.output_rb = rb;
    }
    return ESP_OK;
}

ringbuf_handle_t audio_element_get_output_ringbuf(audio_element_handle_t el)
{
    if (el->write_type == IO_TYPE_RB) {
        return el->out.output_rb;
    } else {
        return NULL;
    }
}

esp_err_t audio_element_set_input_timeout(audio_element_handle_t el, int timeout_ms)
{
    if (el) {
        el->input_timeout_ms = timeout_ms;
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t audio_element_set_output_timeout(audio_element_handle_t el, int timeout_ms)
{
    if (el) {
        el->output_timeout_ms = timeout_ms;
        return ESP_OK;
    }
    return ESP_FAIL;
}

int audio_element_get_output_ringbuf_size(audio_element_handle_t el)
{
    if (el) {
        return el->out_rb_size;
    }
    return 0;
}

esp_err_t audio_element_set_output_ringbuf_size(audio_element_handle_t el, int rb_size)
{
    if (el) {
        el->out_rb_size = rb_size;
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t audio_element_set_read_cb(audio_element_handle_t el, stream_func fn, void *context)
{
    if (el) {
        el->in.read_cb.cb = fn;
        el->in.read_cb.ctx = context;
        el->read_type = IO_TYPE_CB;
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t audio_element_set_write_cb(audio_element_handle_t el, stream_func fn, void *context)
{
    if (el) {
        el->out.write_cb.cb = fn;
        el->out.write_cb.ctx = context;
        el->write_type = IO_TYPE_CB;
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t audio_element_wait_for_stop(audio_element_handle_t el)
{
    if (el->state == AEL_STATE_STOPPED
        || el->state == AEL_STATE_INIT) {
        ESP_LOGV(TAG, "[%s] Element already stopped, no need waiting", el->tag);
        return ESP_OK;
    }
    esp_err_t ret = ESP_FAIL;
    OS_THREAD_MUTEX_LOCK(el->state_lock);
    while ((el->state_event & STOPPED_BIT) == 0) {
        if (OS_THREAD_COND_TIMEDWAIT(el->state_cond, el->state_lock, DEFAULT_WAIT_TIMEOUT_MS*1000) != 0)
            break;
    }
    if ((el->state_event & STOPPED_BIT) != 0)
        ret = ESP_OK;
    OS_THREAD_MUTEX_UNLOCK(el->state_lock);
    return ret;
}

esp_err_t audio_element_wait_for_buffer(audio_element_handle_t el, int size_expect, int timeout_ms)
{
    esp_err_t ret = ESP_FAIL;
    el->out_buf_size_expect = size_expect;
    if (el->out.output_rb) {
        audio_element_clear_state_event(el, BUFFER_REACH_LEVEL_BIT);
        OS_THREAD_MUTEX_LOCK(el->state_lock);
        while ((el->state_event & BUFFER_REACH_LEVEL_BIT) == 0) {
            if (OS_THREAD_COND_TIMEDWAIT(el->state_cond, el->state_lock, timeout_ms*1000) != 0)
                break;
        }
        if ((el->state_event & BUFFER_REACH_LEVEL_BIT) != 0)
            ret = ESP_OK;
        OS_THREAD_MUTEX_UNLOCK(el->state_lock);
    }
    return ret;
}

audio_element_handle_t audio_element_init(audio_element_cfg_t *config)
{
    audio_element_handle_t el = audio_calloc(1, sizeof(struct audio_element));

    AUDIO_MEM_CHECK(TAG, el, {
        return NULL;
    });

    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    evt_cfg.on_cmd = audio_element_on_cmd;
    evt_cfg.context = el;
    evt_cfg.queue_set_size = 0; // Element have no queue_set by default.
    bool _success =
        (
            ((config->tag ? audio_element_set_tag(el, config->tag) : audio_element_set_tag(el, "unknown")) == ESP_OK) &&
            (el->info_lock      = OS_THREAD_MUTEX_CREATE())         &&
            (el->iface_event    = audio_event_iface_init(&evt_cfg)) &&
            (el->state_cond     = OS_THREAD_COND_CREATE())          &&
            (el->state_lock     = OS_THREAD_MUTEX_CREATE())
        );

    AUDIO_MEM_CHECK(TAG, _success, goto _element_init_failed);

    el->open = config->open;
    el->process = config->process;
    el->close = config->close;
    el->destroy = config->destroy;
    el->state = 0;
    el->buffer_reach_level = false;
    el->multi_in.max_rb_num = config->multi_in_rb_num;
    el->multi_out.max_rb_num = config->multi_out_rb_num;
    if (el->multi_in.max_rb_num > 0) {
        el->multi_in.rb = (ringbuf_handle_t *)audio_calloc(el->multi_in.max_rb_num, sizeof(ringbuf_handle_t));
        AUDIO_MEM_CHECK(TAG, el->multi_in.rb, goto _element_init_failed);
    }
    if (el->multi_out.max_rb_num > 0) {
        el->multi_out.rb = (ringbuf_handle_t *)audio_calloc(el->multi_out.max_rb_num, sizeof(ringbuf_handle_t));
        AUDIO_MEM_CHECK(TAG, el->multi_out.rb, goto _element_init_failed);
    }

    if (config->task_stack > 0) {
        el->task_stack = config->task_stack;
    }
    if (config->task_prio) {
        el->task_prio = config->task_prio;
    } else {
        el->task_prio = DEFAULT_ELEMENT_TASK_PRIO;
    }
    if (config->out_rb_size > 0) {
        el->out_rb_size = config->out_rb_size;
    } else {
        el->out_rb_size = DEFAULT_ELEMENT_RINGBUF_SIZE;
    }
    el->data = config ->data;

    el->state = AEL_STATE_INIT;
    el->buf_size = config->buffer_len;

    audio_element_info_t info = AUDIO_ELEMENT_INFO_DEFAULT();
    audio_element_setinfo(el, &info);
    audio_element_set_input_timeout(el, AUDIO_MAX_DELAY);
    audio_element_set_output_timeout(el, AUDIO_MAX_DELAY);

    if (config->read != NULL) {
        el->read_type = IO_TYPE_CB;
        el->in.read_cb.cb = config->read;
    } else {
        el->read_type = IO_TYPE_RB;
    }

    if (config->write != NULL) {
        el->write_type = IO_TYPE_CB;
        el->out.write_cb.cb = config->write;
    } else {
        el->write_type = IO_TYPE_RB;
    }

    el->events_type = EVENTS_TYPE_Q;
    return el;
_element_init_failed:
    if (el->info_lock) {
        OS_THREAD_MUTEX_DESTROY(el->info_lock);
    }
    if (el->state_cond) {
        OS_THREAD_COND_DESTROY(el->state_cond);
    }
    if (el->state_lock) {
        OS_THREAD_MUTEX_DESTROY(el->state_lock);
    }
    if (el->iface_event) {
        audio_event_iface_destroy(el->iface_event);
    }
    if (el->tag) {
        audio_element_set_tag(el, NULL);
    }
    if (el->multi_in.rb) {
        audio_free(el->multi_in.rb);
        el->multi_in.rb = NULL;
    }
    if (el->multi_out.rb) {
        audio_free(el->multi_out.rb);
        el->multi_out.rb = NULL;
    }
    audio_element_set_uri(el, NULL);
    audio_free(el);
    return NULL;
}

esp_err_t audio_element_deinit(audio_element_handle_t el)
{
    audio_element_stop(el);
    audio_element_wait_for_stop(el);
    audio_element_terminate(el);
    OS_THREAD_MUTEX_DESTROY(el->state_lock);
    OS_THREAD_COND_DESTROY(el->state_cond);
    OS_THREAD_MUTEX_DESTROY(el->info_lock);
    audio_event_iface_destroy(el->iface_event);
    if (el->destroy) {
        el->destroy(el);
    }
    audio_element_set_tag(el, NULL);
    audio_element_set_uri(el, NULL);
    if (el->multi_in.rb) {
        audio_free(el->multi_in.rb);
        el->multi_in.rb = NULL;
    }
    if (el->multi_out.rb) {
        audio_free(el->multi_out.rb);
        el->multi_out.rb = NULL;
    }
    if (el->report_info) {
        audio_free(el->report_info);
    }
    audio_free(el);
    return ESP_OK;
}

esp_err_t audio_element_run(audio_element_handle_t el)
{
    char task_name[32];
    struct os_threadattr threadattr;
    esp_err_t ret = ESP_FAIL;
    if (el->task_run) {
        ESP_LOGV(TAG, "[%s] Element already created", el->tag);
        return ESP_OK;
    }
    ESP_LOGV(TAG, "[%s] Element starting...", el->tag);
    snprintf(task_name, 32, "el-%s", el->tag);
    threadattr.name = task_name;
    threadattr.priority = el->task_prio;
    threadattr.stacksize = el->task_stack > 0 ? el->task_stack : DEFAULT_ELEMENT_STACK_SIZE;
    threadattr.joinable = false;
    audio_event_iface_discard(el->iface_event);
    audio_element_clear_state_event(el, TASK_CREATED_BIT);
    if (OS_THREAD_CREATE(&threadattr, audio_element_task, (void *)el) == NULL) {
        ESP_LOGE(TAG, "[%s] Error create element task", el->tag);
        return ESP_FAIL;
    }
    OS_THREAD_MUTEX_LOCK(el->state_lock);
    while ((el->state_event & TASK_CREATED_BIT) == 0) {
        if (OS_THREAD_COND_TIMEDWAIT(el->state_cond, el->state_lock, DEFAULT_WAIT_TIMEOUT_MS*1000) != 0)
            break;
    }
    if ((el->state_event & TASK_CREATED_BIT) != 0)
        ret = ESP_OK;
    OS_THREAD_MUTEX_UNLOCK(el->state_lock);
    ESP_LOGV(TAG, "[%s] Element task created", el->tag);
    return ret;
}

esp_err_t audio_element_terminate(audio_element_handle_t el)
{
    if (!el->task_run) {
        ESP_LOGV(TAG, "[%s] Element has not create when AUDIO_ELEMENT_TERMINATE", el->tag);
        return ESP_OK;
    }
    audio_element_clear_state_event(el, TASK_DESTROYED_BIT);
    if (audio_element_cmd_send(el, AEL_MSG_CMD_DESTROY) != ESP_OK) {
        ESP_LOGE(TAG, "[%s] Element destroy CMD failed", el->tag);
        return ESP_FAIL;
    }
    esp_err_t ret = ESP_FAIL;
    OS_THREAD_MUTEX_LOCK(el->state_lock);
    while ((el->state_event & TASK_DESTROYED_BIT) == 0) {
        if (OS_THREAD_COND_TIMEDWAIT(el->state_cond, el->state_lock, DEFAULT_WAIT_TIMEOUT_MS*1000) != 0)
            break;
    }
    if ((el->state_event & TASK_DESTROYED_BIT) != 0)
        ret = ESP_OK;
    OS_THREAD_MUTEX_UNLOCK(el->state_lock);
    ESP_LOGV(TAG, "[%s] Element task destroyed", el->tag);
    return ret;
}

esp_err_t audio_element_pause(audio_element_handle_t el)
{
    ESP_LOGV(TAG, "[%s] Element pausing", el->tag);
    if (!el->task_run) {
        ESP_LOGV(TAG, "[%s] Element has not create when AUDIO_ELEMENT_PAUSE", el->tag);
        return ESP_OK;
    }
    if ((el->state >= AEL_STATE_PAUSED)) {
        audio_element_force_set_state(el, AEL_STATE_PAUSED);
        ESP_LOGV(TAG, "[%s] Element already paused, state:%d", el->tag, el->state);
        return ESP_OK;
    }
    audio_element_clear_state_event(el, PAUSED_BIT);
    if (audio_element_cmd_send(el, AEL_MSG_CMD_PAUSE) != ESP_OK) {
        ESP_LOGE(TAG, "[%s] Element send cmd error when AUDIO_ELEMENT_PAUSE", el->tag);
        return ESP_FAIL;
    }
    esp_err_t ret = ESP_FAIL;
    OS_THREAD_MUTEX_LOCK(el->state_lock);
    while ((el->state_event & PAUSED_BIT) == 0) {
        if (OS_THREAD_COND_TIMEDWAIT(el->state_cond, el->state_lock, DEFAULT_WAIT_TIMEOUT_MS*1000) != 0)
            break;
    }
    if ((el->state_event & PAUSED_BIT) != 0) {
        ESP_LOGD(TAG, "[%s] Element paused", el->tag);
        ret = ESP_OK;
    }
    else {
        ESP_LOGE(TAG, "[%s] Element failed when AUDIO_ELEMENT_PAUSE", el->tag);
    }
    OS_THREAD_MUTEX_UNLOCK(el->state_lock);
    return ret;
}

esp_err_t audio_element_resume(audio_element_handle_t el, float wait_for_rb_threshold, int timeout_ms)
{
    ESP_LOGV(TAG, "[%s] Element resuming", el->tag);
    if (!el->task_run) {
        ESP_LOGV(TAG, "[%s] Element has not create when AUDIO_ELEMENT_RESUME", el->tag);
        return ESP_FAIL;
    }
    if (el->state == AEL_STATE_RUNNING) {
        ESP_LOGV(TAG, "[%s] RESUME: Element is already running, state:%d, task_run:%d, is_running:%d",
                 el->tag, el->state, el->task_run, el->is_running);
        return ESP_OK;
    }
    if (el->state == AEL_STATE_ERROR) {
        ESP_LOGE(TAG, "[%s] RESUME: Element error, state:%d", el->tag, el->state);
        return ESP_FAIL;
    }
    if (el->state == AEL_STATE_FINISHED) {
        ESP_LOGV(TAG, "[%s] RESUME: Element has finished, state:%d", el->tag, el->state);
        audio_element_report_status(el, AEL_STATUS_STATE_FINISHED);
        return ESP_OK;
    }
    if (wait_for_rb_threshold > 1 || wait_for_rb_threshold < 0) {
        return ESP_FAIL;
    }

    int ret =  ESP_FAIL;
    audio_element_clear_state_event(el, RESUMED_BIT);
    audio_element_cmd_send(el, AEL_MSG_CMD_RESUME);
    OS_THREAD_MUTEX_LOCK(el->state_lock);
    while ((el->state_event & RESUMED_BIT) == 0) {
        if (OS_THREAD_COND_TIMEDWAIT(el->state_cond, el->state_lock, DEFAULT_WAIT_TIMEOUT_MS*1000) != 0)
            break;
    }
    if ((el->state_event & RESUMED_BIT) != 0 && el->is_running) {
        ESP_LOGD(TAG, "[%s] Element resumed", el->tag);
        ret = ESP_OK;
    }
    else {
        ESP_LOGE(TAG, "[%s] Element failed when AUDIO_ELEMENT_RESUME", el->tag);
    }
    OS_THREAD_MUTEX_UNLOCK(el->state_lock);

    if (ret == ESP_OK && el->out.output_rb) {
        int size_threshold = rb_get_size(el->out.output_rb) * wait_for_rb_threshold;
        if (size_threshold != 0 && rb_bytes_filled(el->out.output_rb) < size_threshold)
            audio_element_wait_for_buffer(el, size_threshold, timeout_ms);
    }
    return ret;
}

esp_err_t audio_element_stop(audio_element_handle_t el)
{
    if (!el->task_run) {
        ESP_LOGV(TAG, "[%s] Element has not create when AUDIO_ELEMENT_STOP", el->tag);
        return ESP_FAIL;
    }
    if (el->state == AEL_STATE_RUNNING) {
        audio_element_clear_state_event(el, STOPPED_BIT);
    }
    if (el->state == AEL_STATE_PAUSED) {
        el->is_running = true;
        audio_event_iface_set_cmd_waiting_timeout(el->iface_event, 0);
    }
    if (el->is_running == false) {
        audio_element_set_state_event(el, STOPPED_BIT);
        audio_element_report_status(el, AEL_STATUS_STATE_STOPPED);
        ESP_LOGV(TAG, "[%s] Element already stopped", el->tag);
        return ESP_OK;
    }
    if (el->stopping) {
        ESP_LOGV(TAG, "[%s] Stop command has already sent, %d", el->tag, el->stopping);
        return ESP_OK;
    }
    el->stopping = true;
    audio_element_abort_output_ringbuf(el);
    audio_element_abort_input_ringbuf(el);
    if (audio_element_cmd_send(el, AEL_MSG_CMD_STOP) != ESP_OK) {
        el->stopping = false;
        return ESP_FAIL;
    }
    ESP_LOGV(TAG, "[%s] Send stop command", el->tag);
    return ESP_OK;
}

esp_err_t audio_element_wait_for_stop_ms(audio_element_handle_t el, int timeout_ms)
{
    if (el->state == AEL_STATE_STOPPED
        || el->state == AEL_STATE_INIT
        || el->state == AEL_STATE_FINISHED) {
        ESP_LOGV(TAG, "[%s] Element already stopped, return without waiting", el->tag);
        return ESP_OK;
    }
    esp_err_t ret = ESP_FAIL;
    OS_THREAD_MUTEX_LOCK(el->state_lock);
    while ((el->state_event & STOPPED_BIT) == 0) {
        if (OS_THREAD_COND_TIMEDWAIT(el->state_cond, el->state_lock, timeout_ms*1000) != 0)
            break;
    }
    if ((el->state_event & STOPPED_BIT) != 0)
        ret = ESP_OK;
    OS_THREAD_MUTEX_UNLOCK(el->state_lock);
    return ret;
}

esp_err_t audio_element_multi_input(audio_element_handle_t el, char *buffer, int wanted_size, int index, int timeout_ms)
{
    esp_err_t ret = ESP_OK;
    if (index >= el->multi_in.max_rb_num) {
        ESP_LOGE(TAG, "The index of ringbuffer is gather than and equal to ringbuffer maximum (%d)", el->multi_in.max_rb_num);
        return ESP_FAIL;
    }
    if (el->multi_in.rb[index]) {
        ret = rb_read(el->multi_in.rb[index], buffer, wanted_size, timeout_ms);
    }
    return ret;
}

esp_err_t audio_element_multi_output(audio_element_handle_t el, char *buffer, int wanted_size, int timeout_ms)
{
    esp_err_t ret = ESP_OK;
    for (int i = 0; i < el->multi_out.max_rb_num; ++i) {
        if (el->multi_out.rb[i]) {
            ret |= rb_write(el->multi_out.rb[i], buffer, wanted_size, timeout_ms);
        }
    }
    return ret;
}

esp_err_t audio_element_set_multi_input_ringbuf(audio_element_handle_t el, ringbuf_handle_t rb, int index)
{
    if ((index < el->multi_in.max_rb_num) && rb) {
        el->multi_in.rb[index] = rb;
        return ESP_OK;
    }
    return ESP_ERR_INVALID_ARG;
}

esp_err_t audio_element_set_multi_output_ringbuf(audio_element_handle_t el, ringbuf_handle_t rb, int index)
{
    if ((index < el->multi_out.max_rb_num) && rb) {
        el->multi_out.rb[index] = rb;
        return ESP_OK;
    }
    return ESP_ERR_INVALID_ARG;
}

ringbuf_handle_t audio_element_get_multi_input_ringbuf(audio_element_handle_t el, int index)
{
    if (index < el->multi_in.max_rb_num) {
        return el->multi_in.rb[index];
    }
    return NULL;
}

ringbuf_handle_t audio_element_get_multi_output_ringbuf(audio_element_handle_t el, int index)
{
    if (index < el->multi_out.max_rb_num) {
        return el->multi_out.rb[index];
    }
    return NULL;
}
