/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_video_wrap.h"

void app_main(void)
{
    const esp_video_init_config_t config = {0};

    ESP_ERROR_CHECK(esp_video_wrap_init(&config));
    ESP_ERROR_CHECK(esp_video_wrap_deinit());
}
