/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_cam_led.h"

static const char *TAG = "cam_led";

esp_err_t esp_cam_led_ioctl(esp_cam_led_device_t *dev, uint32_t cmd, void *arg)
{
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(dev->ops && dev->ops->priv_ioctl, ESP_ERR_NOT_SUPPORTED, TAG, "unsupported operation");

    return dev->ops->priv_ioctl(dev, cmd, arg);
}

const char *esp_cam_led_get_name(esp_cam_led_device_t *dev)
{
    ESP_RETURN_ON_FALSE(dev, NULL, TAG, "invalid argument");

    return dev->name;
}

esp_err_t esp_cam_led_del_dev(esp_cam_led_device_t *dev)
{
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(dev->ops && dev->ops->del, ESP_ERR_NOT_SUPPORTED, TAG, "unsupported operation");
    return dev->ops->del(dev);
}
