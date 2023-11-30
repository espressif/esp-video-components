/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_err.h"
#include "esp_log.h"

#include "esp_camera.h"

static const char *TAG = "esp_camera";

esp_err_t esp_camera_ioctl(esp_camera_device_t handle, uint32_t cmd, void *value, size_t *size)
{
    esp_err_t ret = ESP_OK;
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_camera_ops_t *ops = (esp_camera_ops_t *)handle;
    switch (cmd) {
    case CAM_SENSOR_G_NAME:
        if (ops->get_name) {
            ops->get_name(value, size);
        }
        break;
    case CAM_SENSOR_G_FORMAT_ARRAY:
        if (ops->query_support_formats(value)) {
            ret = ESP_FAIL;
        }
        break;
    case CAM_SENSOR_G_FORMAT:
        if (ops->get_format(value)) {
            ret = ESP_FAIL;
        }
        break;
    case CAM_SENSOR_G_CAP:
        if (ops->query_support_capability(value)) {
            ret = ESP_FAIL;
        }
        break;
    case CAM_SENSOR_S_FORMAT:
        if (ops->set_format(value)) {
            ret = ESP_FAIL;
        }
        break;
    default:
        if (ops->priv_ioctl(cmd, value)) {
            ret = ESP_FAIL;
        }
        break;
    }
    return ret;
}

esp_err_t esp_camera_init(const esp_camera_config_t *config)
{
    extern esp_camera_detect_fn_t __esp_camera_detect_fn_array_start;
    extern esp_camera_detect_fn_t __esp_camera_detect_fn_array_end;

    esp_camera_detect_fn_t *p;

    for (p = &__esp_camera_detect_fn_array_start; p < &__esp_camera_detect_fn_array_end; ++p) {
        if (p->inf == CAMERA_INF_CSI && config->csi != NULL) {
            esp_camera_device_t device = (*(p->fn))(config->csi);

            // ToDo: initialize the csi driver and video layer
        }

        if (p->inf == CAMERA_INF_DVP && config->dvp_num > 0 && config->dvp != NULL) {
            for (size_t i = 0; i < config->dvp_num; i++) {
                // ToDo: define the number according to the chip
                if (i == 2) {
                    ESP_LOGW(TAG, "Support for a maximum of %d DVP cameras only.", i);
                    break;
                }
                esp_camera_device_t device = (*(p->fn))(config->dvp + i);

                // ToDo: initialize the dvp and video layer
            }
        }

        if (p->inf == CAMERA_INF_SIM && config->sim_num && config->sim != NULL) {
            for (size_t i = 0; i < config->sim_num; i++) {
                esp_camera_device_t device = (*(p->fn))(config->sim + i);

                // ToDo: initialize the sim & video layer
            }
        }
    }

    return ESP_OK;
}
