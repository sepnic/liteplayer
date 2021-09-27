/*
 * Copyright (c) 2019-2021 Qinglong <sysu.zqlong@gmail.com>
 *
 * This file is part of Liteplayer
 * (see https://github.com/sepnic/liteplayer_priv).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 2.1 of the License, or
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
#include <stdint.h>
#include <string.h>

#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "sink_ESP32-LyraT-Mini_wrapper.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/i2s.h"
#include "i2c_bus.h"
#include "audio_hal.h"

#define TAG "[liteplayer]esp32_lyrat_mini"

#define I2S_DMA_BUF_LEN 300
#define I2S_DMA_BUF_CNT 4
#define DEFAULT_VOLUME 50

static i2c_bus_handle_t esp32_i2c_handle = NULL;
static audio_hal_handle_t esp32_codec_handle = NULL;

extern audio_hal_func_t AUDIO_CODEC_ES8311_DEFAULT_HANDLE;

int8_t get_pa_enable_gpio(void)
{
    return GPIO_NUM_21;
}

static int get_i2c_pins(i2c_port_t port, i2c_config_t *i2c_config)
{
    if (port == I2C_NUM_0 || port == I2C_NUM_1) {
        i2c_config->sda_io_num = GPIO_NUM_18;
        i2c_config->scl_io_num = GPIO_NUM_23;
    } else {
        i2c_config->sda_io_num = -1;
        i2c_config->scl_io_num = -1;
        OS_LOGE(TAG, "i2c port %d is not supported", port);
        return -1;
    }
    return 0;
}

static int get_i2s_pins(i2s_port_t port, i2s_pin_config_t *i2s_config)
{
    if (port == I2S_NUM_0) {
        i2s_config->bck_io_num = GPIO_NUM_5;
        i2s_config->ws_io_num = GPIO_NUM_25;
        i2s_config->data_out_num = GPIO_NUM_26;
        i2s_config->data_in_num = GPIO_NUM_35;
    } else if (port == I2S_NUM_1) {
        i2s_config->bck_io_num = GPIO_NUM_32;
        i2s_config->ws_io_num = GPIO_NUM_33;
        i2s_config->data_out_num = -1;
        i2s_config->data_in_num = GPIO_NUM_36;
    } else {
        memset(i2s_config, -1, sizeof(i2s_pin_config_t));
        OS_LOGE(TAG, "i2s port %d is not supported", port);
        return -1;
    }
    return 0;
}

static void pa_power_enable(bool enable)
{
    gpio_config_t  io_conf;
    memset(&io_conf, 0, sizeof(io_conf));
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT64(get_pa_enable_gpio());
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    if (enable) {
        gpio_set_level(get_pa_enable_gpio(), 1);
    } else {
        gpio_set_level(get_pa_enable_gpio(), 0);
    }
}

static int i2s_mclk_gpio_select(i2s_port_t i2s_num, gpio_num_t gpio_num)
{
    if (i2s_num >= I2S_NUM_MAX) {
        OS_LOGE(TAG, "Does not support i2s number(%d)", i2s_num);
        return -1;
    }
    if (gpio_num != GPIO_NUM_0 && gpio_num != GPIO_NUM_1 && gpio_num != GPIO_NUM_3) {
        OS_LOGE(TAG, "Only support GPIO0/GPIO1/GPIO3, gpio_num:%d", gpio_num);
        return -1;
    }
    OS_LOGI(TAG, "I2S%d, MCLK output by GPIO%d", i2s_num, gpio_num);
    if (i2s_num == I2S_NUM_0) {
        if (gpio_num == GPIO_NUM_0) {
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
            WRITE_PERI_REG(PIN_CTRL, 0xFFF0);
        } else if (gpio_num == GPIO_NUM_1) {
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD_CLK_OUT3);
            WRITE_PERI_REG(PIN_CTRL, 0xF0F0);
        } else {
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD_CLK_OUT2);
            WRITE_PERI_REG(PIN_CTRL, 0xFF00);
        }
    } else if (i2s_num == I2S_NUM_1) {
        if (gpio_num == GPIO_NUM_0) {
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
            WRITE_PERI_REG(PIN_CTRL, 0xFFFF);
        } else if (gpio_num == GPIO_NUM_1) {
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD_CLK_OUT3);
            WRITE_PERI_REG(PIN_CTRL, 0xF0FF);
        } else {
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD_CLK_OUT2);
            WRITE_PERI_REG(PIN_CTRL, 0xFF0F);
        }
    }
    return 0;
}

const char *esp32_lyrat_mini_wrapper_name()
{
    return "esp32_lyrat_mini";
}

sink_handle_t esp32_lyrat_mini_wrapper_open(int samplerate, int channels, int bits, void *priv_data)
{
    OS_LOGD(TAG, "Opening esp32_lyrat_mini: samplerate=%d, channels=%d, bits=%d", samplerate, channels, bits);
    if (esp32_i2c_handle == NULL) {
        i2c_config_t i2c_cfg = {
            .mode = I2C_MODE_MASTER,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = 100000,
        };
        if (get_i2c_pins(I2C_NUM_0, &i2c_cfg) != 0) {
            OS_LOGE(TAG, "get_i2c_pins failed");
            return NULL;
        }
        esp32_i2c_handle = i2c_bus_create(I2C_NUM_0, &i2c_cfg);
        if (esp32_i2c_handle == NULL) {
            OS_LOGE(TAG, "i2c_bus_create failed");
            return NULL;
        }
    }

    if (esp32_codec_handle == NULL) {
        audio_hal_codec_config_t codec_cfg = AUDIO_CODEC_DEFAULT_CONFIG();
        codec_cfg.i2c_handle = esp32_i2c_handle;
        esp32_codec_handle = audio_hal_init(&codec_cfg, &AUDIO_CODEC_ES8311_DEFAULT_HANDLE);
        if (esp32_codec_handle == NULL) {
            OS_LOGE(TAG, "audio_hal_init failed");
            return NULL;
        }
    }

    {
        i2s_config_t i2s_config = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX),
            .sample_rate = samplerate,
            .bits_per_sample = bits,
            .channel_format = channels == 1 ? I2S_CHANNEL_FMT_ONLY_LEFT : I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2 | ESP_INTR_FLAG_IRAM,
            .dma_buf_count = I2S_DMA_BUF_CNT,
            .dma_buf_len = I2S_DMA_BUF_LEN,
            .use_apll = true,
            .tx_desc_auto_clear = true,
            .fixed_mclk = 0,
        };
        if (i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL) != 0) {
            OS_LOGE(TAG, "i2s_driver_install failed");
            return NULL;
        }

        i2s_pin_config_t i2s_pin_cfg = {0};
        get_i2s_pins(I2S_NUM_0, &i2s_pin_cfg);
        if (i2s_set_pin(I2S_NUM_0, &i2s_pin_cfg) != 0) {
            OS_LOGE(TAG, "i2s_set_pin failed");
            i2s_driver_uninstall(I2S_NUM_0);
            return NULL;
        }
        i2s_mclk_gpio_select(I2S_NUM_0, GPIO_NUM_0);
    }

    if (i2s_set_clk(I2S_NUM_0, samplerate, bits, channels) != 0) {
        OS_LOGE(TAG, "i2s_set_clk failed");
        i2s_driver_uninstall(I2S_NUM_0);
        return NULL;
    }

    if (audio_hal_ctrl_codec(esp32_codec_handle, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START) != 0) {
        OS_LOGE(TAG, "audio_hal_ctrl_codec start failed");
        i2s_driver_uninstall(I2S_NUM_0);
        return NULL;
    }
    audio_hal_set_volume(esp32_codec_handle, DEFAULT_VOLUME);

    pa_power_enable(true);
    return (sink_handle_t)0xffff;
}

int esp32_lyrat_mini_wrapper_write(sink_handle_t handle, char *buffer, int size)
{
    //OS_LOGD(TAG, "Writing esp32_lyrat_mini: size=%d", size);
    size_t bytes_written = 0;
    if (i2s_write(I2S_NUM_0, buffer, size, &bytes_written, 0x7fffffff) != 0) {
        OS_LOGE(TAG, "i2s_write failed");
        return -1;
    }
    return bytes_written;
}

void esp32_lyrat_mini_wrapper_close(sink_handle_t handle)
{
    OS_LOGD(TAG, "closing esp32_lyrat_mini");
    pa_power_enable(false);
    audio_hal_ctrl_codec(esp32_codec_handle, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_STOP);
    i2s_driver_uninstall(I2S_NUM_0);
}
