/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_log.h"
#include "esp_cam_led.h"
#include "ir2835.h"

static const char *TAG = "ir2835";

static esp_err_t ir2835_set_brightness(esp_cam_led_device_t *dev, uint32_t brightness)
{
    /* GPIO high turns the IR LED on. Any non-zero request is stored as brightness 1. */
    uint32_t level = (brightness != 0) ? 1 : 0;

    if (gpio_set_level(dev->gpio_ctrl, (uint32_t)level) != ESP_OK) {
        return ESP_FAIL;
    }
    dev->brightness = (uint32_t)level;
    return ESP_OK;
}

static esp_err_t ir2835_priv_ioctl(esp_cam_led_device_t *dev, uint32_t cmd, void *arg)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);
    esp_err_t ret = ESP_OK;

    switch (cmd) {
    case ESP_CAM_LED_IOCTL_SET_BRIGHTNESS: {
        uint32_t brightness = *(uint32_t *)arg;
        if (pthread_mutex_lock(&dev->lock) != 0) {
            return ESP_ERR_INVALID_STATE;
        }
        ret = ir2835_set_brightness(dev, brightness);
        pthread_mutex_unlock(&dev->lock);
        break;
    }
    case ESP_CAM_LED_IOCTL_GET_BRIGHTNESS: {
        if (pthread_mutex_lock(&dev->lock) != 0) {
            return ESP_ERR_INVALID_STATE;
        }
        *(uint32_t *)arg = (uint32_t)dev->brightness;
        pthread_mutex_unlock(&dev->lock);
        break;
    }
    case ESP_CAM_LED_IOCTL_GET_INFO:
        memcpy((esp_cam_led_dev_info_t *)arg, dev->info, sizeof(esp_cam_led_dev_info_t));
        break;
    default:
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    return ret;
}

static esp_err_t ir2835_delete(esp_cam_led_device_t *dev)
{
    ESP_LOGW(TAG, "delete ir2835 (%p)", dev);
    if (dev->gpio_ctrl != -1) {
        gpio_reset_pin(dev->gpio_ctrl);
    }
    pthread_mutex_destroy(&dev->lock);
    free(dev);
    return ESP_OK;
}

static const esp_cam_led_ops_t ir2835_ops = {
    .priv_ioctl = ir2835_priv_ioctl,
    .del = ir2835_delete
};

static const esp_cam_led_dev_info_t ir2835_info = {
    .type = ESP_CAM_LED_HW_TYPE_GPIO,
    .spectrum = ESP_CAM_LED_SPECTRUM_IR,
    .brightness_range_min = 0,
    .brightness_range_max = 1,
};

esp_cam_led_device_t *ir2835_detect(const esp_cam_led_config_t *config)
{
    esp_cam_led_device_t *dev = NULL;

    if (config == NULL) {
        return NULL;
    }

    if (config->type != ESP_CAM_LED_HW_TYPE_GPIO) {
        ESP_LOGE(TAG, "invalid LED type");
        return NULL;
    }

    if (config->gpio_ctrl == -1) {
        ESP_LOGE(TAG, "GPIO control pin is not set");
        return NULL;
    } else {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << config->gpio_ctrl;
        conf.mode = GPIO_MODE_OUTPUT;
        if (gpio_config(&conf) != ESP_OK) {
            ESP_LOGE(TAG, "failed to config GPIO");
            return NULL;
        }
        if (gpio_set_level(config->gpio_ctrl, 0) != ESP_OK) {
            return NULL;
        }
    }

    dev = calloc(1, sizeof(esp_cam_led_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for LED");
        return NULL;
    }

    if (pthread_mutex_init(&dev->lock, NULL) != 0) {
        ESP_LOGE(TAG, "failed to init LED mutex");
        free(dev);
        return NULL;
    }

    dev->name = (char *)TAG;
    dev->info = &ir2835_info;
    dev->gpio_ctrl = config->gpio_ctrl;
    dev->brightness = 0;
    dev->ops = &ir2835_ops;

    ESP_LOGI(TAG, "Detected Cam LED");

    return dev;
}
