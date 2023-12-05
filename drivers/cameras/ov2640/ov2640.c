/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_camera.h"

static const char *TAG = "ov2640";

esp_camera_device_t *ov2640_dvp_detect(const esp_camera_driver_config_t *config)
{
    ESP_LOGI(TAG, "ov2640_dvp_detect");
    return NULL;
}

#if CONFIG_CAMERA_OV2640_AUTO_DETECT
ESP_CAMERA_DETECT_FN(ov2640_dvp_detect, CAMERA_INTF_DVP)
{
    return ov2640_dvp_detect(config);
}
#endif
