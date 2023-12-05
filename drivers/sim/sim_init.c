/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sim_init.h"

esp_err_t esp_video_bsp_init(void)
{
    ESP_ERROR_CHECK(sim_initialize_video_device());

    return ESP_OK;
}
