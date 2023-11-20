/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdio.h>
#include "driver/ledc.h"

/**
 * @brief Configure LEDC timer&channel for generating XCLK
 *        Configure LEDC timer with the given source timer/frequency(Hz)
 *
 * @param  ledc_timer The timer source of channel (0 - 3)
 * @param  ledc_channel LEDC channel used for XCLK (0 - 7)
 * @param  xclk_freq_hz XCLK output frequency (Hz)
 * @param  pin_xclk the XCLK output gpio_num, if you want to use gpio16, gpio_num = 16
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_FAIL Can not find a proper pre-divider number base on the given frequency and the current duty_resolution.
 */
esp_err_t xclk_enable_out_clock(ledc_timer_t ledc_timer, ledc_channel_t ledc_channel, int xclk_freq_hz, int pin_xclk);

/**
 * @brief Disable xclk output, and set idle level
 *
 * @param  ledc_channel LEDC channel (0-7), select from ledc_channel_t
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t xclk_disable_out_clock(ledc_channel_t ledc_channel);
