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
#include <stdbool.h>
#include <string.h>

#include "msgutils/os_thread.h"
#include "msgutils/ringbuf.h"
#include "msgutils/os_logger.h"
#include "esp_adf/audio_common.h"
#include "liteplayer_config.h"
#include "liteplayer_debug.h"

#if defined(OS_FREERTOS)
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

#define TAG "[liteplayer]DEBUG"

struct socket_upload_priv {
    int fd;
    const char *addr;
    int port;
    ringbuf_handle_t rb;
    os_thread_t tid;
    char buffer[2048];
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

static void socket_upload_cleanup(struct socket_upload_priv *priv)
{
    if (priv->rb != NULL)
        rb_destroy(priv->rb);
    if (priv->addr != NULL)
        OS_FREE(priv->addr);
    OS_FREE(priv);
}

static void *socket_upload_thread(void *arg)
{
    struct socket_upload_priv *priv = (struct socket_upload_priv *)arg;
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
        }
        else {
            if (bytes_read == RB_DONE) {
                OS_LOGV(TAG, "RB done");
            }
            else if(bytes_read == RB_ABORT) {
                OS_LOGV(TAG, "RB abort");
            }
            else {
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

socket_upload_handle_t socket_upload_start(const char *server_addr, int server_port)
{
    struct socket_upload_priv *priv = OS_CALLOC(1, sizeof(struct socket_upload_priv));
    if (priv == NULL)
        return NULL;

    priv->port = server_port;
    priv->addr = OS_STRDUP(server_addr);
    if (priv->addr == NULL)
        goto start_failed;

    priv->rb = rb_create(DEFAULT_SOCKET_UPLOAD_RINGBUF_SIZE);
    if (priv->rb == NULL)
        goto start_failed;

    struct os_threadattr attr = {
        .name = "ael-debug",
        .priority = DEFAULT_SOCKET_UPLOAD_TASK_PRIO,
        .stacksize = DEFAULT_SOCKET_UPLOAD_TASK_STACKSIZE,
        .joinable = true,
    };
    priv->tid = OS_THREAD_CREATE(&attr, socket_upload_thread, priv);
    if (priv->tid == NULL)
        goto start_failed;

    OS_LOGD(TAG, "Socket upload start");
    return priv;

start_failed:
    socket_upload_cleanup(priv);
    return NULL;
}

int socket_upload_fill_data(socket_upload_handle_t handle, char *data, int size)
{
    struct socket_upload_priv *priv = (struct socket_upload_priv *)handle;
    if (priv == NULL || priv->stop || data == NULL || size <= 0)
        return -1;

    int ret = rb_write(priv->rb, data, size, DEFAULT_SOCKET_UPLOAD_WRITE_TIMEOUT*1000);
    if (ret == RB_TIMEOUT) {
        OS_LOGE(TAG, "Timeout to write ringbuf");
    }
    else if (ret > 0) {
        ret = 0;
    }

    return ret;
}

void socket_upload_stop(socket_upload_handle_t handle)
{
    struct socket_upload_priv *priv = (struct socket_upload_priv *)handle;
    if (priv == NULL)
        return;

    OS_LOGD(TAG, "Socket upload stop");

    rb_done_write(priv->rb);
    priv->stop = true;
    OS_THREAD_JOIN(priv->tid, NULL);
    socket_upload_cleanup(priv);
}
