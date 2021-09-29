#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "osal/os_misc.h"
#include "osal/os_thread.h"
#include "cutils/log_helper.h"
#include "cutils/memory_helper.h"
#include "liteplayer_main.h"

#include "source_httpclient_wrapper.h"
#include "sink_esp8266_i2s_wrapper.h"

#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#define TAG "liteplayer_demo"

//#define HTTP_URL "https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.mp3"
#define HTTP_URL "http://ailabsaicloudservice.alicdn.com/player/resources/23a2d715f019c0e345235f379fa26a30.mp3"

static int liteplayer_demo_state_listener(enum liteplayer_state state, int errcode, void *priv)
{
    enum liteplayer_state *player_state = (enum liteplayer_state *)priv;
    bool state_sync = true;

    switch (state) {
    case LITEPLAYER_IDLE:
        OS_LOGD(TAG, "-->LITEPLAYER_IDLE");
        break;
    case LITEPLAYER_INITED:
        OS_LOGD(TAG, "-->LITEPLAYER_INITED");
        break;
    case LITEPLAYER_PREPARED:
        OS_LOGD(TAG, "-->LITEPLAYER_PREPARED");
        break;
    case LITEPLAYER_STARTED:
        OS_LOGD(TAG, "-->LITEPLAYER_STARTED");
        break;
    case LITEPLAYER_PAUSED:
        OS_LOGD(TAG, "-->LITEPLAYER_PAUSED");
        break;
    case LITEPLAYER_NEARLYCOMPLETED:
        OS_LOGD(TAG, "-->LITEPLAYER_NEARLYCOMPLETED");
        state_sync = false;
        break;
    case LITEPLAYER_COMPLETED:
        OS_LOGD(TAG, "-->LITEPLAYER_COMPLETED");
        break;
    case LITEPLAYER_STOPPED:
        OS_LOGD(TAG, "-->LITEPLAYER_STOPPED");
        break;
    case LITEPLAYER_ERROR:
        OS_LOGE(TAG, "-->LITEPLAYER_ERROR: %d", errcode);
        break;
    default:
        OS_LOGE(TAG, "-->LITEPLAYER_UNKNOWN: %d", state);
        state_sync = false;
        break;
    }

    if (state_sync)
        *player_state = state;
    return 0;
}

static void *liteplayer_demo(void *arg)
{
    OS_LOGI(TAG, "liteplayer_demo thread enter");

    liteplayer_handle_t player = liteplayer_create();
    if (player == NULL)
        return NULL;

    enum liteplayer_state player_state = LITEPLAYER_IDLE;
    liteplayer_register_state_listener(player, liteplayer_demo_state_listener, (void *)&player_state);

    struct sink_wrapper sink_ops = {
        .priv_data = NULL,
        .name = esp8266_i2s_wrapper_name,
        .open = esp8266_i2s_wrapper_open,
        .write = esp8266_i2s_wrapper_write,
        .close = esp8266_i2s_wrapper_close,
    };
    liteplayer_register_sink_wrapper(player, &sink_ops);

    struct source_wrapper http_ops = {
        .async_mode = false,
        .buffer_size = 2*1024,
        .priv_data = NULL,
        .url_protocol = httpclient_wrapper_url_protocol,
        .open = httpclient_wrapper_open,
        .read = httpclient_wrapper_read,
        .content_pos = httpclient_wrapper_content_pos,
        .content_len = httpclient_wrapper_content_len,
        .seek = httpclient_wrapper_seek,
        .close = httpclient_wrapper_close,
    };
    liteplayer_register_source_wrapper(player, &http_ops);

    if (liteplayer_set_data_source(player, HTTP_URL) != 0) {
        OS_LOGE(TAG, "Failed to set data source");
        goto test_done;
    }

    if (liteplayer_prepare_async(player) != 0) {
        OS_LOGE(TAG, "Failed to prepare player");
        goto test_done;
    }
    while (player_state != LITEPLAYER_PREPARED && player_state != LITEPLAYER_ERROR) {
        os_thread_sleep_msec(100);
    }
    if (player_state == LITEPLAYER_ERROR) {
        OS_LOGE(TAG, "Failed to prepare player");
        goto test_done;
    }

    if (liteplayer_start(player) != 0) {
        OS_LOGE(TAG, "Failed to start player");
        goto test_done;
    }
    OS_MEMORY_DUMP();
    while (player_state != LITEPLAYER_COMPLETED && player_state != LITEPLAYER_ERROR) {
        os_thread_sleep_msec(100);
    }

    if (liteplayer_stop(player) != 0) {
        OS_LOGE(TAG, "Failed to stop player");
        goto test_done;
    }
    while (player_state != LITEPLAYER_STOPPED) {
        os_thread_sleep_msec(100);
    }

test_done:
    liteplayer_reset(player);
    while (player_state != LITEPLAYER_IDLE) {
        os_thread_sleep_msec(100);
    }
    os_thread_sleep_msec(1000);
    liteplayer_destroy(player);
    os_thread_sleep_msec(100);

    OS_LOGI(TAG, "liteplayer_demo thread leave");
    return NULL;
}

void app_main()
{
    printf("Hello liteplayer!\n");

    {
        unsigned long buffer[4];
        for (int i = 0; i < 3; i++) {
            os_random(buffer, sizeof(buffer));
            OS_LOGI(TAG, "os_random[%d]: %08x %08x %08x %08x", i, buffer[0], buffer[1], buffer[2], buffer[3]);
        }
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());
    OS_LOGI(TAG, "Connected to AP");

    struct os_thread_attr attr = {
        .name = "liteplayer_demo",
        .priority = OS_THREAD_PRIO_NORMAL,
        .stacksize = 8192,
        .joinable = false,
    };
    os_thread_create(&attr, liteplayer_demo, NULL);

    while (1)
        os_thread_sleep_msec(100);
}
