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

#include "esp_adf/esp_log.h"
#include "esp_adf/audio_common.h"
#include "audio_stream/http_stream.h"
#include "httpclient.h"
#include "httpclient_wrapper.h"

#define TAG "httpwrapper"

#define HTTPCLIENT_HEADER_BUFFER_SIZE 1024
#define HTTPCLIENT_RETRY_COUNT        3

typedef struct httpclient_wrapper_priv {
    const char          *url;
    char                 header_buf[HTTPCLIENT_HEADER_BUFFER_SIZE];
    httpclient_t         client;
    httpclient_data_t    client_data;
    long long            content_pos;
    long long            content_len;
    int                  retrieve_len;
    bool                 first_request;
    bool                 first_response;
    int                  retrycount;
} httpclient_wrapper_priv_t;

static int httpclient_wrapper_connect(http_handle_t handle)
{
    httpclient_wrapper_priv_t *priv = (httpclient_wrapper_priv_t *)handle;
    HTTPCLIENT_RESULT ret = HTTPCLIENT_OK;

    memset(&priv->client, 0, sizeof(httpclient_t));
    memset(&priv->client_data, 0, sizeof(httpclient_data_t));
    memset(&priv->header_buf[0], 0, sizeof(priv->header_buf));
    priv->client.socket = -1;
    priv->retrieve_len = -1;
    priv->content_len = 0;
    priv->first_request = false;
    priv->first_response = false;

reconnect:
    ret = httpclient_connect(&priv->client, (char *)priv->url);
    if (ret != HTTPCLIENT_OK) {
        ESP_LOGE(TAG, "Failed to connect, ret=%d, retry=%d", ret, priv->retrycount);
        if (priv->retrycount++ < HTTPCLIENT_RETRY_COUNT) {
            OS_THREAD_SLEEP_MSEC(2000);
            goto reconnect;
        }
        httpclient_close(&priv->client);
    }

    return ret;
}

static void httpclient_wrapper_disconnect(http_handle_t handle)
{
    httpclient_wrapper_priv_t *priv = (httpclient_wrapper_priv_t *)handle;
    httpclient_close(&priv->client);
}

static int httpclient_wrapper_parse_content_length(char *header_buf, long long *content_len)
{
    int val_pos = 0, val_len = 0;
    int ret = httpclient_get_response_header_value(header_buf, "Content-Length", &val_pos, &val_len);

    if (ret == 0) {
        char header_content[64] = {0};
        memcpy(header_content, header_buf+val_pos, val_len);
        header_content[val_len] = '\0';
        *content_len = atoi(header_content);
        ESP_LOGV(TAG, "Content-Length=%d", (int)(*content_len));
    }
    return ret;
}

http_handle_t httpclient_wrapper_open(const char *url, long long content_pos, void *priv)
{
    httpclient_wrapper_priv_t *handle = audio_calloc(1, sizeof(httpclient_wrapper_priv_t));
    if (handle == NULL)
        return NULL;

    handle->url = audio_strdup(url);
    handle->content_pos = content_pos;
    ESP_LOGD(TAG, "Connecting url:%s, content_pos:%d", url, (int)content_pos);
    if (httpclient_wrapper_connect(handle) != HTTPCLIENT_OK) {
        audio_free(handle->url);
        audio_free(handle);
        return NULL;
    }
    return handle;
}

int httpclient_wrapper_read(http_handle_t handle, char *buffer, int size)
{
    httpclient_wrapper_priv_t *priv = (httpclient_wrapper_priv_t *)handle;
    httpclient_t *client = &priv->client;
    httpclient_data_t *client_data = &priv->client_data;
    char *url = (char *)priv->url;
    int resp_len = 0;
    HTTPCLIENT_RESULT ret = HTTPCLIENT_ERROR;

contiune_read:
    client_data->header_buf       = priv->header_buf;
    client_data->header_buf_len   = HTTPCLIENT_HEADER_BUFFER_SIZE;
    client_data->response_buf     = buffer;
    client_data->response_buf_len = size;

    if (!priv->first_request) {
        if (priv->content_pos > 0) {
            char tmp_buf[32] = {0};
            snprintf(tmp_buf, sizeof(tmp_buf), "Range: bytes=%ld-\r\n", (long)priv->content_pos);
            ESP_LOGV(TAG, "Set http range: %s", tmp_buf);
            httpclient_set_custom_header(client, tmp_buf);
        }

        ret = httpclient_send_request(client, url, HTTPCLIENT_GET, client_data);
        if (ret < 0) {
            ESP_LOGE(TAG, "httpclient_send_request failed, ret=%d, retry=%d", ret, priv->retrycount);
            if (priv->retrycount++ >= HTTPCLIENT_RETRY_COUNT)
                return -1;
            OS_THREAD_SLEEP_MSEC(2000);
            goto reconnect;
        }
        priv->first_request = true;
        priv->retrycount = 0;
    }

    //ESP_LOGD(TAG, "httpclient reading: %d/%d", (int)priv->content_pos, (int)priv->content_len);
    if (priv->content_len > 0 && priv->content_pos >= priv->content_len) {
        ESP_LOGD(TAG, "httpclient read done: %d/%d", (int)priv->content_pos, (int)priv->content_len);
        return 0;
    }

    ret = httpclient_recv_response(client, client_data);
    if (ret < 0) {
        ESP_LOGE(TAG, "httpclient_recv_response failed, ret=%d, reconnect", ret);
        goto reconnect;
    }

    if (!priv->first_response) {
        ret = httpclient_wrapper_parse_content_length(client_data->header_buf, &priv->content_len);
        if (ret != 0)
            priv->content_len = client_data->response_content_len;
        priv->content_len += priv->content_pos;
        ESP_LOGD(TAG, "content_pos=%d, response_content_len=%d, content_len=%d",
                 (int)priv->content_pos, (int)client_data->response_content_len, (int)priv->content_len);
        priv->first_response = true;
    }

    if (priv->retrieve_len == -1)
        priv->retrieve_len = client_data->response_content_len;
    resp_len = priv->retrieve_len - client_data->retrieve_len;
    priv->retrieve_len = client_data->retrieve_len;

    //ESP_LOGV(TAG, "resp_len=%d, is_more=%d, response_buf_len=%d, retrieve_len=%d, content_len=%d",
    //         resp_len, client_data->is_more,
    //         client_data->response_buf_len, priv->retrieve_len, (int)priv->content_len);

    if (resp_len < 0 || resp_len > client_data->response_buf_len) {
        ESP_LOGE(TAG, "Invalid resp_len:%d", resp_len);
        return HTTPCLIENT_ERROR;
    }

    priv->content_pos += resp_len;

    //ESP_LOGV(TAG, "-->resp_len len=%d", resp_len);
    return resp_len;

reconnect:
    httpclient_wrapper_disconnect(priv);
    ret = httpclient_wrapper_connect(priv);
    if (ret != HTTPCLIENT_OK) {
        ESP_LOGE(TAG, "httpclient reconnect failed, ret=%d", ret);
        return ret;
    } else {
        goto contiune_read;
    }
    return -1;
}

long long httpclient_wrapper_filesize(http_handle_t handle)
{
    httpclient_wrapper_priv_t *priv = (httpclient_wrapper_priv_t *)handle;
    return priv->content_len;
}

int httpclient_wrapper_seek(http_handle_t handle, long offset)
{
    httpclient_wrapper_priv_t *priv = (httpclient_wrapper_priv_t *)handle;

    ESP_LOGD(TAG, "Seeking http client, content_pos=%d", offset);
    priv->content_pos = offset;
    httpclient_wrapper_disconnect(priv);
    OS_THREAD_SLEEP_MSEC(50);
    return httpclient_wrapper_connect(priv);
}

void httpclient_wrapper_close(http_handle_t handle)
{
    httpclient_wrapper_priv_t *priv = (httpclient_wrapper_priv_t *)handle;

    ESP_LOGD(TAG, "Closing http client");
    httpclient_close(&priv->client);
    audio_free(priv->url);
    audio_free(priv);
    ESP_LOGV(TAG, "Closed http client");
}
