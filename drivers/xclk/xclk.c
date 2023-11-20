/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"

#include "xclk.h"

static const char *TAG = "cam_xclk";

#define NO_CAMERA_LEDC_CHANNEL 0xFF
/*Note that when setting the resolution to 1, we can only choose to divide the frequency by 1 times from CLK*/
#define LEDC_DUTY_RES_DEFAULT           LEDC_TIMER_1_BIT

static esp_err_t xclk_timer_conf(int ledc_timer, int xclk_freq_hz)
{
    ledc_timer_config_t timer_conf;
    timer_conf.duty_resolution = LEDC_DUTY_RES_DEFAULT;
    timer_conf.freq_hz = xclk_freq_hz;
    timer_conf.speed_mode = LEDC_LOW_SPEED_MODE;
    timer_conf.clk_cfg = LEDC_AUTO_CLK;
#if ESP_IDF_VERSION_MAJOR >= 4
    timer_conf.clk_cfg = LEDC_AUTO_CLK;
#endif
    timer_conf.timer_num = (ledc_timer_t)ledc_timer;
    esp_err_t err = ledc_timer_config(&timer_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_timer_config failed for freq %d, rc=%x", xclk_freq_hz, err);
    }
    return err;
}

esp_err_t xclk_enable_out_clock(ledc_timer_t ledc_timer, ledc_channel_t ledc_channel, int xclk_freq_hz, int pin_xclk)
{
    esp_err_t err = xclk_timer_conf(ledc_timer, xclk_freq_hz);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_timer_config failed, rc=%x", err);
        return err;
    }

    ledc_channel_config_t ch_conf;
    ch_conf.gpio_num = pin_xclk;
    ch_conf.speed_mode = LEDC_LOW_SPEED_MODE;
    ch_conf.channel = ledc_channel;
    ch_conf.intr_type = LEDC_INTR_DISABLE;
    ch_conf.timer_sel = ledc_timer;
    ch_conf.duty = 1;
    ch_conf.hpoint = 0;
    err = ledc_channel_config(&ch_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_channel_config failed, rc=%x", err);
        return err;
    }
    return ESP_OK;
}

esp_err_t xclk_disable_out_clock(ledc_channel_t ledc_channel)
{
    esp_err_t ret = ESP_OK;
    if (ledc_channel != NO_CAMERA_LEDC_CHANNEL) {
        ret = ledc_stop(LEDC_LOW_SPEED_MODE, ledc_channel, 0);
        ledc_channel = NO_CAMERA_LEDC_CHANNEL;
    }
    return ret;
}
