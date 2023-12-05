/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_camera.h"

static const char *TAG = "sim";

esp_camera_device_t *sim_detect(const esp_camera_driver_config_t *config)
{
    ESP_LOGI(TAG, "sim_detect");
    return NULL;
}

#if CONFIG_CAMERA_SIM_AUTO_DETECT
ESP_CAMERA_DETECT_FN(sim_detect, CAMERA_INTF_SIM)
{
    return sim_detect(config);
}
#endif
