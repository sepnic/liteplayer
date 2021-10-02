// Copyright (c) 2019-2021 Qinglong<sysu.zqlong@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "osal/os_thread.h"
#include "cutils/ringbuf.h"
#include "cutils/log_helper.h"
#include "esp_adf/audio_common.h"
#include "liteplayer_config.h"
#include "liteplayer_debug.h"

#if defined(OS_RTOS)
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/tcp.h"
#include "lwip/err.h"
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#define TAG "[liteplayer]debug"

#define MIN_SOCKET_UPLOAD_RINGBUF_SIZE  ( 1024*16 )
#define MAX_SOCKET_UPLOAD_RINGBUF_SIZE  ( 1024*1024 )

#define DEFAULT_SOCKET_UPLOAD_TIMEOUT_MS    ( 2000 )

struct socketupload_priv {
    struct socketupload uploader;
    int fd;
    const char *addr;
    int port;
    ringbuf_handle rb;
    os_thread tid;
    char buffer[2048];
    int buffer_size;
    bool stop;
};

static int socket_connect(const char *addr, int port)
{
    struct sockaddr_in sockaddr_in = { 0 };

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        OS_LOGE(TAG, "Cannot create socket");
        return -1;
    }

    sockaddr_in.sin_family = AF_INET;
    sockaddr_in.sin_port = htons(port);
    sockaddr_in.sin_addr.s_addr = inet_addr(addr);
    memset(&(sockaddr_in.sin_zero), 0x0, sizeof(sockaddr_in.sin_zero));

    if (connect(fd, (struct sockaddr *)(&sockaddr_in), sizeof(sockaddr_in)) < 0) {
        OS_LOGE(TAG, "Cannot connect to server\n");
        close(fd);
        return -1;
    }
    return fd;
}

static int socket_send(int fd, void *data, int length)
{
    if (fd >= 0) {
        ssize_t ret = send(fd, data, length, 0);
        if (ret < 0) {
            OS_LOGE(TAG, "Failed to send data to server");
        }
        return ret;
    }
    return -1;
}

static void socket_disconnet(int fd)
{
    if (fd >= 0)
        close(fd);
}

static void socketupload_cleanup(struct socketupload_priv *priv)
{
    if (priv->rb != NULL) {
        rb_destroy(priv->rb);
        priv->rb = NULL;
    }
    if (priv->addr != NULL) {
        OS_FREE(priv->addr);
        priv->addr = NULL;
    }
}

static void *socketupload_thread(void *arg)
{
    struct socketupload_priv *priv = (struct socketupload_priv *)arg;
    int ret = 0, bytes_read = 0;

    priv->fd = socket_connect(priv->addr, priv->port);
    if (priv->fd < 0) {
        OS_LOGE(TAG, "Failed to connect server:[%s:%d]", priv->addr, priv->port);
        goto thread_exit;
    }

    ret = socket_send(priv->fd, DEFAULT_SOCKET_UPLOAD_START, strlen(DEFAULT_SOCKET_UPLOAD_START));
    if (ret < 0) {
        OS_LOGE(TAG, "Failed to send SOCKET_UPLOAD_START to server");
        goto thread_exit;
    }

    while (!priv->stop || rb_bytes_filled(priv->rb) > 0) {
        bytes_read = rb_read(priv->rb, priv->buffer, sizeof(priv->buffer), AUDIO_MAX_DELAY);
        if (bytes_read > 0) {
            ret = socket_send(priv->fd, priv->buffer, bytes_read);
            if (ret < 0) {
                OS_LOGE(TAG, "Failed to send pcm data to server");
                break;
            }
        } else {
            if (bytes_read == RB_DONE) {
                OS_LOGV(TAG, "RB done");
            } else if(bytes_read == RB_ABORT) {
                OS_LOGV(TAG, "RB abort");
            } else {
                OS_LOGE(TAG, "RB read fail, ret=%d", bytes_read);
            }
            break;
        }
    }

    ret = socket_send(priv->fd, DEFAULT_SOCKET_UPLOAD_END, strlen(DEFAULT_SOCKET_UPLOAD_END));
    if (ret < 0) {
        OS_LOGE(TAG, "Failed to send SOCKET_UPLOAD_END to server");
        goto thread_exit;
    }

thread_exit:
    priv->stop = true;
    rb_done_read(priv->rb);
    if (priv->fd >= 0)
        socket_disconnet(priv->fd);
    OS_LOGD(TAG, "Socket upload task leave");
    return NULL;
}

static int socketupload_start(socketupload_handle_t self, const char *server_addr, int server_port)
{
    struct socketupload_priv *priv = (struct socketupload_priv *)self;
    priv->port = server_port;
    priv->addr = OS_STRDUP(server_addr);
    if (priv->addr == NULL)
        goto start_failed;

    priv->rb = rb_create(priv->buffer_size);
    if (priv->rb == NULL)
        goto start_failed;

    struct os_thread_attr attr = {
        .name = "ael-socketupload",
        .priority = DEFAULT_SOCKET_UPLOAD_TASK_PRIO,
        .stacksize = DEFAULT_SOCKET_UPLOAD_TASK_STACKSIZE,
        .joinable = true,
    };
    priv->tid = os_thread_create(&attr, socketupload_thread, priv);
    if (priv->tid == NULL)
        goto start_failed;

    OS_LOGD(TAG, "Socket upload start");
    return 0;

start_failed:
    socketupload_cleanup(priv);
    priv->stop = true;
    return -1;
}

static int socketupload_fill_data(socketupload_handle_t self, char *data, int size)
{
    struct socketupload_priv *priv = (struct socketupload_priv *)self;
    if (priv->stop || priv->rb == NULL || data == NULL || size <= 0)
        return -1;
    int ret = rb_write(priv->rb, data, size, DEFAULT_SOCKET_UPLOAD_TIMEOUT_MS*1000);
    if (ret == RB_TIMEOUT) {
        OS_LOGE(TAG, "Timeout to write ringbuf");
    } else if (ret > 0) {
        ret = 0;
    }
    return ret;
}

static void socketupload_stop(socketupload_handle_t self)
{
    struct socketupload_priv *priv = (struct socketupload_priv *)self;
    OS_LOGD(TAG, "Socket upload stop");
    if (priv->rb != NULL)
        rb_done_write(priv->rb);
    priv->stop = true;
    if (priv->tid != NULL) {
        os_thread_join(priv->tid, NULL);
        priv->tid = NULL;
    }
    socketupload_cleanup(priv);
}

static void socketupload_destroy(socketupload_handle_t self)
{
    struct socketupload_priv *priv = (struct socketupload_priv *)self;
    if (!priv->stop)
        socketupload_stop(self);
    audio_free(self);
}

socketupload_handle_t socketupload_init(int buffer_size)
{
    struct socketupload_priv *handle = audio_calloc(1, sizeof(struct socketupload_priv));
    AUDIO_MEM_CHECK(TAG, handle, return NULL);

    handle->buffer_size = (buffer_size/1024)*1024;
    if (buffer_size < MIN_SOCKET_UPLOAD_RINGBUF_SIZE)
        handle->buffer_size = MIN_SOCKET_UPLOAD_RINGBUF_SIZE;
    else if (buffer_size > MAX_SOCKET_UPLOAD_RINGBUF_SIZE)
        handle->buffer_size = MAX_SOCKET_UPLOAD_RINGBUF_SIZE;
    handle->uploader.start     = socketupload_start;
    handle->uploader.fill_data = socketupload_fill_data;
    handle->uploader.stop      = socketupload_stop;
    handle->uploader.destroy   = socketupload_destroy;
    return (socketupload_handle_t)handle;
}
