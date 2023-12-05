/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_err.h"
#include "esp_log.h"

#include "esp_camera.h"

static const char *TAG = "esp_camera";

esp_err_t esp_camera_ioctl(esp_camera_device_t *dev, uint32_t cmd, void *value, size_t *size)
{
    esp_err_t ret = ESP_OK;

    if (dev == NULL || dev->ops == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (cmd) {
    case CAM_SENSOR_G_NAME:
        if (dev->ops->get_name) {
            dev->ops->get_name(dev, value, size);
        }
        break;
    case CAM_SENSOR_G_FORMAT_ARRAY:
        if (dev->ops->query_support_formats(dev, value)) {
            ret = ESP_FAIL;
        }
        break;
    case CAM_SENSOR_G_FORMAT:
        if (dev->ops->get_format(dev, value)) {
            ret = ESP_FAIL;
        }
        break;
    case CAM_SENSOR_G_CAP:
        if (dev->ops->query_support_capability(dev, value)) {
            ret = ESP_FAIL;
        }
        break;
    case CAM_SENSOR_S_FORMAT:
        if (dev->ops->set_format(dev, value)) {
            ret = ESP_FAIL;
        }
        break;
    default:
        if (dev->ops->priv_ioctl(dev, cmd, value)) {
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

    if (config == NULL || config->sccb_num > 2 || config->dvp_num > 2) {
        ESP_LOGW(TAG, "Please validate camera config");
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < config->sccb_num; i++) {
        if (config->sccb[i].sda_pin != -1 && config->sccb[i].scl_pin != -1) {
            ESP_LOGI(TAG, "Initializing SCCB[%d]", i);

            // ToDo: initialize the sccb driver, if i2c_freq == 0, using the default freq of 100000
        }
    }

    for (p = &__esp_camera_detect_fn_array_start; p < &__esp_camera_detect_fn_array_end; ++p) {
        if (p->intf == CAMERA_INTF_CSI &&  config->sccb_num != 0 && config->csi != NULL) {
            esp_camera_driver_config_t cfg = {
                .sccb_port = config->sccb[config->csi->sccb_config_index].i2c_port,
                .xclk_pin = config->csi->xclk_pin,
                .reset_pin = config->csi->reset_pin,
                .pwdn_pin = config->csi->pwdn_pin,
            };
            esp_camera_device_t *cam_dev = (*(p->fn))(&cfg);

            // ToDo: initialize the csi driver and video layer
        }

        if (p->intf == CAMERA_INTF_DVP &&  config->sccb_num != 0 && config->dvp_num > 0 && config->dvp != NULL) {
            for (size_t i = 0; i < config->dvp_num; i++) {
                esp_camera_driver_config_t cfg = {
                    .sccb_port = config->sccb[config->dvp->sccb_config_index].i2c_port,
                    .xclk_pin = config->dvp[i].xclk_pin,
                    .reset_pin = config->dvp[i].reset_pin,
                    .pwdn_pin = config->dvp[i].pwdn_pin,
                };
                esp_camera_device_t *cam_dev = (*(p->fn))(&cfg);

                // ToDo: initialize the dvp driver and video layer
            }
        }

        if (p->intf == CAMERA_INTF_SIM && config->sim_num && config->sim != NULL) {
            for (size_t i = 0; i < config->sim_num; i++) {
                esp_camera_driver_config_t cfg = {0};
                esp_camera_device_t *cam_dev = (*(p->fn))(&cfg);

                // ToDo: initialize the sim & video layer
            }
        }
    }

    return ESP_OK;
}
