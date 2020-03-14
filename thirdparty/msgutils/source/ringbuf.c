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

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "msgutils/os_thread.h"
#include "msgutils/os_memory.h"
#include "msgutils/os_logger.h"
#include "msgutils/ringbuf.h"

#define LOG_TAG "ringbuf"

struct ringbuf {
    char *p_o;                   /**< Original pointer */
    char *volatile p_r;          /**< Read pointer */
    char *volatile p_w;          /**< Write pointer */
    unsigned int fill_cnt;       /**< Number of filled slots */
    unsigned int size;           /**< Buffer size */
    os_cond_t can_read;
    os_cond_t can_write;
    os_mutex_t lock;
    bool abort_read;
    bool abort_write;
    bool is_done_write;         /**< To signal that we are done writing */
    bool unblock_reader_flag;   /**< To unblock instantly from rb_read */
};

ringbuf_handle_t rb_create(unsigned int size)
{
    ringbuf_handle_t rb;
    char *buf = NULL;
    bool _success =
        (
            (rb             = OS_MALLOC(sizeof(struct ringbuf))) &&
            (buf            = OS_CALLOC(1, size))                &&
            (rb->lock       = OS_THREAD_MUTEX_CREATE())          &&
            (rb->can_read   = OS_THREAD_COND_CREATE())           &&
            (rb->can_write  = OS_THREAD_COND_CREATE())
        );

    if (!_success) {
        rb_destroy(rb);
        return NULL;
    }

    rb->p_o = rb->p_r = rb->p_w = buf;
    rb->fill_cnt = 0;
    rb->size = size;
    rb->is_done_write = false;
    rb->unblock_reader_flag = false;
    rb->abort_read = false;
    rb->abort_write = false;
    return rb;
}

void rb_destroy(ringbuf_handle_t rb)
{
    if (rb == NULL)
        return;
    if (rb->p_o)
        OS_FREE(rb->p_o);
    if (rb->can_read)
        OS_THREAD_COND_DESTROY(rb->can_read);
    if (rb->can_write)
        OS_THREAD_COND_DESTROY(rb->can_write);
    if (rb->lock)
        OS_THREAD_MUTEX_DESTROY(rb->lock);
    OS_FREE(rb);
}

void rb_reset(ringbuf_handle_t rb)
{
    OS_THREAD_MUTEX_LOCK(rb->lock);
    rb->p_r = rb->p_w = rb->p_o;
    rb->fill_cnt = 0;
    rb->is_done_write = false;
    rb->unblock_reader_flag = false;
    rb->abort_read = false;
    rb->abort_write = false;
    OS_THREAD_COND_SIGNAL(rb->can_write);
    OS_THREAD_MUTEX_UNLOCK(rb->lock);
}

int rb_bytes_available(ringbuf_handle_t rb)
{
    return (rb->size - rb->fill_cnt);
}

int rb_bytes_filled(ringbuf_handle_t rb)
{
    return rb->fill_cnt;
}

int rb_read(ringbuf_handle_t rb, char *buf, int buf_len, unsigned int timeout_ms)
{
    int read_size = 0;
    int total_read_size = 0;
    int ret_val = 0;

    //take buffer lock
    OS_THREAD_MUTEX_LOCK(rb->lock);

    while (buf_len > 0) {
        if (rb->fill_cnt < buf_len) {
            read_size = rb->fill_cnt;
            /**
             * When non-multiple of 4(word size) bytes are written to I2S, there is noise.
             * Below is the kind of workaround to read only in multiple of 4. Avoids noise when rb is read in small chunks.
             * Note that, when we have buf_len bytes available in rb, we still read those irrespective of if it's multiple of 4.
             */
            //read_size = read_size & 0xfffffffc;
            //if ((read_size == 0) && rb->is_done_write) {
            //    read_size = rb->fill_cnt;
            //}
        }
        else {
            read_size = buf_len;
        }

        if (read_size == 0) {
            if (rb->is_done_write) {
                ret_val = RB_DONE;
                goto read_err;
            }
            if (rb->abort_read) {
                ret_val = RB_ABORT;
                goto read_err;
            }
            if (rb->unblock_reader_flag) {
                //reader_unblock is nothing but forced timeout
                ret_val = RB_TIMEOUT;
                goto read_err;
            }
            OS_THREAD_COND_SIGNAL(rb->can_write);
            //wait till some data available to read
            if (timeout_ms == 0)
                ret_val = OS_THREAD_COND_WAIT(rb->can_read, rb->lock);
            else
                ret_val = OS_THREAD_COND_TIMEDWAIT(rb->can_read, rb->lock, timeout_ms*1000);
            if (ret_val != 0) {
                ret_val = RB_TIMEOUT;
                goto read_err;
            }
            continue;
        }

        if ((rb->p_r + read_size) > (rb->p_o + rb->size)) {
            int rlen1 = rb->p_o + rb->size - rb->p_r;
            int rlen2 = read_size - rlen1;
            memcpy(buf, rb->p_r, rlen1);
            memcpy(buf + rlen1, rb->p_o, rlen2);
            rb->p_r = rb->p_o + rlen2;
        }
        else {
            memcpy(buf, rb->p_r, read_size);
            rb->p_r = rb->p_r + read_size;
        }

        buf_len -= read_size;
        rb->fill_cnt -= read_size;
        total_read_size += read_size;
        buf += read_size;
    }

read_err:
    if (total_read_size > 0) {
        OS_THREAD_COND_SIGNAL(rb->can_write);
    }
    OS_THREAD_MUTEX_UNLOCK(rb->lock);
    if ((ret_val == RB_FAIL) || (ret_val == RB_ABORT)) {
        total_read_size = ret_val;
    }
    return total_read_size > 0 ? total_read_size : ret_val;
}

int rb_write(ringbuf_handle_t rb, char *buf, int buf_len, unsigned int timeout_ms)
{
    int write_size = 0;
    int total_write_size = 0;
    int ret_val = 0;

    //take buffer lock
    OS_THREAD_MUTEX_LOCK(rb->lock);

    while (buf_len > 0) {
        write_size = rb_bytes_available(rb);
        if (buf_len < write_size) {
            write_size = buf_len;
        }

        if (write_size == 0) {
            if (rb->is_done_write) {
                ret_val = RB_DONE;
                goto write_err;
            }
            if (rb->abort_write) {
                ret_val = RB_ABORT;
                goto write_err;
            }
            OS_THREAD_COND_SIGNAL(rb->can_read);
            //wait till we have some empty space to write
            if (timeout_ms == 0)
                ret_val = OS_THREAD_COND_WAIT(rb->can_write, rb->lock);
            else
                ret_val = OS_THREAD_COND_TIMEDWAIT(rb->can_write, rb->lock, timeout_ms*1000);
            if (ret_val != 0) {
                ret_val = RB_TIMEOUT;
                goto write_err;
            }
            continue;
        }

        if ((rb->p_w + write_size) > (rb->p_o + rb->size)) {
            int wlen1 = rb->p_o + rb->size - rb->p_w;
            int wlen2 = write_size - wlen1;
            memcpy(rb->p_w, buf, wlen1);
            memcpy(rb->p_o, buf + wlen1, wlen2);
            rb->p_w = rb->p_o + wlen2;
        }
        else {
            memcpy(rb->p_w, buf, write_size);
            rb->p_w = rb->p_w + write_size;
        }

        buf_len -= write_size;
        rb->fill_cnt += write_size;
        total_write_size += write_size;
        buf += write_size;
    }

write_err:
    if (total_write_size > 0) {
        OS_THREAD_COND_SIGNAL(rb->can_read);
    }
    OS_THREAD_MUTEX_UNLOCK(rb->lock);
    if ((ret_val == RB_FAIL) || (ret_val == RB_ABORT)) {
        total_write_size = ret_val;
    }
    return total_write_size > 0 ? total_write_size : ret_val;
}

int rb_read_nonblock(ringbuf_handle_t rb, char *buf, int buf_len)
{
    int read_size = 0;
    int total_read_size = 0;
    int ret_val = 0;

    //take buffer lock
    OS_THREAD_MUTEX_LOCK(rb->lock);

    while (buf_len > 0) {
        if (rb->fill_cnt < buf_len) {
            read_size = rb->fill_cnt;
            /**
             * When non-multiple of 4(word size) bytes are written to I2S, there is noise.
             * Below is the kind of workaround to read only in multiple of 4. Avoids noise when rb is read in small chunks.
             * Note that, when we have buf_len bytes available in rb, we still read those irrespective of if it's multiple of 4.
             */
            //read_size = read_size & 0xfffffffc;
            //if ((read_size == 0) && rb->is_done_write) {
            //    read_size = rb->fill_cnt;
            //}
        }
        else {
            read_size = buf_len;
        }

        if (read_size == 0) {
            if (rb->is_done_write) {
                ret_val = RB_DONE;
            }
            if (rb->abort_read) {
                ret_val = RB_ABORT;
            }
            goto read_done;
        }

        if ((rb->p_r + read_size) > (rb->p_o + rb->size)) {
            int rlen1 = rb->p_o + rb->size - rb->p_r;
            int rlen2 = read_size - rlen1;
            memcpy(buf, rb->p_r, rlen1);
            memcpy(buf + rlen1, rb->p_o, rlen2);
            rb->p_r = rb->p_o + rlen2;
        }
        else {
            memcpy(buf, rb->p_r, read_size);
            rb->p_r = rb->p_r + read_size;
        }

        buf_len -= read_size;
        rb->fill_cnt -= read_size;
        total_read_size += read_size;
        buf += read_size;
    }

read_done:
    if (total_read_size > 0) {
        OS_THREAD_COND_SIGNAL(rb->can_write);
    }
    OS_THREAD_MUTEX_UNLOCK(rb->lock);
    if (buf_len > total_read_size) {
        OS_LOGV(LOG_TAG, "Insufficient filled data, read %d/%d bytes, lacked %d bytes",
                total_read_size, buf_len, buf_len - total_read_size);
    }
    if ((ret_val == RB_FAIL) || (ret_val == RB_ABORT)) {
        total_read_size = ret_val;
    }
    return total_read_size >= 0 ? total_read_size : ret_val;
}

int rb_write_nonblock(ringbuf_handle_t rb, char *buf, int buf_len)
{
    int write_size = 0;
    int total_write_size = 0;
    int ret_val = 0;

    //take buffer lock
    OS_THREAD_MUTEX_LOCK(rb->lock);

    while (buf_len > 0) {
        write_size = rb_bytes_available(rb);
        if (buf_len < write_size) {
            write_size = buf_len;
        }

        if (write_size == 0) {
            if (rb->is_done_write) {
                ret_val = RB_DONE;
            }
            if (rb->abort_write) {
                ret_val = RB_ABORT;
            }
            goto write_done;
        }

        if ((rb->p_w + write_size) > (rb->p_o + rb->size)) {
            int wlen1 = rb->p_o + rb->size - rb->p_w;
            int wlen2 = write_size - wlen1;
            memcpy(rb->p_w, buf, wlen1);
            memcpy(rb->p_o, buf + wlen1, wlen2);
            rb->p_w = rb->p_o + wlen2;
        }
        else {
            memcpy(rb->p_w, buf, write_size);
            rb->p_w = rb->p_w + write_size;
        }

        buf_len -= write_size;
        rb->fill_cnt += write_size;
        total_write_size += write_size;
        buf += write_size;
    }

write_done:
    if (total_write_size > 0) {
        OS_THREAD_COND_SIGNAL(rb->can_read);
    }
    OS_THREAD_MUTEX_UNLOCK(rb->lock);
    if (buf_len > total_write_size) {
        OS_LOGW(LOG_TAG, "Insufficient available space, wrote %d/%d bytes, discarded %d bytes",
                total_write_size, buf_len, buf_len - total_write_size);
    }
    if ((ret_val == RB_FAIL) || (ret_val == RB_ABORT)) {
        total_write_size = ret_val;
    }
    return total_write_size >= 0 ? total_write_size : ret_val;
}

static void rb_abort_read(ringbuf_handle_t rb)
{
    OS_THREAD_MUTEX_LOCK(rb->lock);
    rb->abort_read = true;
    OS_THREAD_COND_SIGNAL(rb->can_read);
    OS_THREAD_MUTEX_UNLOCK(rb->lock);
}

static void rb_abort_write(ringbuf_handle_t rb)
{
    OS_THREAD_MUTEX_LOCK(rb->lock);
    rb->abort_write = true;
    OS_THREAD_COND_SIGNAL(rb->can_write);
    OS_THREAD_MUTEX_UNLOCK(rb->lock);
}

void rb_abort(ringbuf_handle_t rb)
{
    rb_abort_read(rb);
    rb_abort_write(rb);
}

bool rb_is_full(ringbuf_handle_t rb)
{
    return (rb->size == rb->fill_cnt);
}

void rb_done_write(ringbuf_handle_t rb)
{
    OS_THREAD_MUTEX_LOCK(rb->lock);
    rb->is_done_write = true;
    OS_THREAD_COND_SIGNAL(rb->can_read);
    OS_THREAD_MUTEX_UNLOCK(rb->lock);
}

void rb_done_read(ringbuf_handle_t rb)
{
    OS_THREAD_MUTEX_LOCK(rb->lock);
    rb->is_done_write = true;
    OS_THREAD_COND_SIGNAL(rb->can_write);
    OS_THREAD_MUTEX_UNLOCK(rb->lock);
}

void rb_unblock_reader(ringbuf_handle_t rb)
{
    OS_THREAD_MUTEX_LOCK(rb->lock);
    rb->unblock_reader_flag = true;
    OS_THREAD_COND_SIGNAL(rb->can_read);
    OS_THREAD_MUTEX_UNLOCK(rb->lock);
}

bool rb_is_done_write(ringbuf_handle_t rb)
{
    return rb->is_done_write;
}

int rb_get_size(ringbuf_handle_t rb)
{
    return rb->size;
}
