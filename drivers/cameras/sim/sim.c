/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_camera.h"

static const char *TAG = "sim";

esp_camera_device_t sim_detect(void)
{
    ESP_LOGI(TAG, "sim_detect");
    return NULL;
}