/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_camera.h"

static const char *TAG = "ov5640";

esp_camera_device_t ov5640_csi_detect(const esp_camera_csi_config_t *config)
{
    ESP_LOGI(TAG, "ov5640_csi_detect");
    return NULL;
}

#if CONFIG_CAMERA_OV5640_AUTO_DETECT
ESP_CAMERA_DETECT_FN(ov5640_csi_detect, CAMERA_INF_CSI)
{
    return ov5640_csi_detect(config);
}
#endif