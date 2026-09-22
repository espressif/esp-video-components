/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_log.h"
#include "esp_cam_ircut.h"
#include "ap1511b.h"

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))

static const char *TAG = "ap1511b";

/* When gpio_pwdn is used, priv holds non-zero iff the IR-CUT supply is enabled. */
static bool ap1511b_is_powered(const esp_cam_ircut_device_t *dev)
{
    if (dev->gpio_pwdn < 0) {
        return true;
    }
    return dev->priv != NULL;
}

static void ap1511b_set_powered(esp_cam_ircut_device_t *dev, bool en)
{
    if (dev->gpio_pwdn < 0) {
        return;
    }
    dev->priv = en ? (void *)1 : NULL;
    if (!en) {
        /*
         * Without supply, FBC edges do not move the filter. Drop the cached mode
         * so a later SET_MODE after power-on cannot skip the required pulse.
         */
        dev->current_mode = ESP_CAM_IRCUT_MODE_UNKNOWN;
    }
}

static esp_err_t ap1511b_hw_power_on(esp_cam_ircut_device_t *dev, bool en)
{
    esp_err_t ret = ESP_OK;

    if (dev->gpio_pwdn >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->gpio_pwdn;
        conf.mode = GPIO_MODE_OUTPUT;
        ret = gpio_config(&conf);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "gpio config failed");

        if (en) {
            gpio_set_level(dev->gpio_pwdn, 1);
        } else {
            gpio_set_level(dev->gpio_pwdn, 0);
        }
        delay_ms(20);
        if (ret == ESP_OK) {
            ap1511b_set_powered(dev, en);
        }
    }

    return ret;
}

/* Caller must hold dev->lock. When force is true, always emit a pulse even if mode unchanged. */
static esp_err_t ap1511b_hw_switch_unlocked(esp_cam_ircut_device_t *dev, esp_cam_ircut_mode_t mode, bool force)
{
    esp_err_t ret = ESP_OK;

    if (!ap1511b_is_powered(dev)) {
        ESP_LOGW(TAG, "IR-CUT powered off, refuse mode switch");
        return ESP_ERR_INVALID_STATE;
    }

    if (!force && dev->current_mode == mode) {
        return ESP_OK;
    }

    /*
     * AP1511B single trigger pulse control
     * Send a pulse of a specified width, the chip automatically completes the drive, hold and power off
     */
    if (mode == ESP_CAM_IRCUT_MODE_NIGHT) {
        ret |= gpio_set_level(dev->gpio_fbc, 1);
        delay_ms(500);
        ret |= gpio_set_level(dev->gpio_fbc, 0);
    } else {
        ret |= gpio_set_level(dev->gpio_fbc, 0);
        delay_ms(500);
        ret |= gpio_set_level(dev->gpio_fbc, 1);
    }
    /* The power is automatically turned off by the AP1511B, no software manual reset is required */
    if (ret == ESP_OK) {
        dev->current_mode = mode;
    }
    /* Hold lock across mechanical settle to avoid overlapping pulses from other threads */
    delay_ms(CONFIG_CAM_IRCUT_AP1511B_SWITCH_DELAY_MS);
    ESP_LOGD(TAG, "IR-CUT switched to %s mode", mode == ESP_CAM_IRCUT_MODE_NIGHT ? "NIGHT" : "DAY");
    return ret;
}

static esp_err_t ap1511b_hw_switch(esp_cam_ircut_device_t *dev, esp_cam_ircut_mode_t mode)
{
    esp_err_t ret;
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);

    if (mode != ESP_CAM_IRCUT_MODE_DAY && mode != ESP_CAM_IRCUT_MODE_NIGHT) {
        return ESP_ERR_INVALID_ARG;
    }

    if (pthread_mutex_lock(&dev->lock) != 0) {
        return ESP_ERR_INVALID_STATE;
    }

    ret = ap1511b_hw_switch_unlocked(dev, mode, false);

    if (pthread_mutex_unlock(&dev->lock) != 0) {
        return ESP_ERR_INVALID_STATE;
    }
    return ret;
}

static esp_err_t ap1511b_force_rst(esp_cam_ircut_device_t *dev, esp_cam_ircut_mode_t target_mode)
{
    esp_err_t ret = ESP_OK;
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);

    if (target_mode != ESP_CAM_IRCUT_MODE_DAY && target_mode != ESP_CAM_IRCUT_MODE_NIGHT) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_cam_ircut_mode_t reverse_mode = (target_mode == ESP_CAM_IRCUT_MODE_NIGHT) ?
                                        ESP_CAM_IRCUT_MODE_DAY : ESP_CAM_IRCUT_MODE_NIGHT;

    ESP_LOGD(TAG, "IR-CUT congestion detected! Starting force reset to mode %d...", target_mode);

    if (pthread_mutex_lock(&dev->lock) != 0) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Keep the whole reverse -> target sequence atomic under one lock */
    ret |= ap1511b_hw_switch_unlocked(dev, reverse_mode, true);
    delay_ms(100);
    ret |= ap1511b_hw_switch_unlocked(dev, target_mode, true);

    if (pthread_mutex_unlock(&dev->lock) != 0) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "IR-CUT force reset failed");
    return ret;
}

static esp_err_t ap1511b_priv_ioctl(esp_cam_ircut_device_t *dev, uint32_t cmd, void *arg)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    esp_err_t ret = ESP_OK;

    switch (cmd) {
    case ESP_CAM_IRCUT_IOC_SET_MODE:
        ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);
        ret = ap1511b_hw_switch(dev, *(esp_cam_ircut_mode_t *)arg);
        break;
    case ESP_CAM_IRCUT_IOC_GET_MODE:
        ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);
        if (pthread_mutex_lock(&dev->lock) != 0) {
            return ESP_ERR_INVALID_STATE;
        }
        *(esp_cam_ircut_mode_t *)arg = dev->current_mode;
        if (pthread_mutex_unlock(&dev->lock) != 0) {
            return ESP_ERR_INVALID_STATE;
        }
        break;
    case ESP_CAM_IRCUT_IOC_GET_HW_TYPE:
        ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);
        *(esp_cam_ircut_hw_type_t *)arg = dev->hw_type;
        break;
    case ESP_CAM_IRCUT_IOC_FORCE_RST:
        ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);
        ret = ap1511b_force_rst(dev, *(esp_cam_ircut_mode_t *)arg);
        break;
    case ESP_CAM_IRCUT_IOC_HW_POWER_ON:
        ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);
        if (pthread_mutex_lock(&dev->lock) != 0) {
            return ESP_ERR_INVALID_STATE;
        }
        ret = ap1511b_hw_power_on(dev, *(int *)arg != 0);
        if (pthread_mutex_unlock(&dev->lock) != 0) {
            return ESP_ERR_INVALID_STATE;
        }
        break;
    default:
        ret = ESP_ERR_INVALID_ARG;
        break;
    }

    return ret;
}

static esp_err_t ap1511b_delete(esp_cam_ircut_device_t *dev)
{
    ESP_LOGD(TAG, "del ap1511b (%p)", dev);
    if (dev) {
        pthread_mutex_destroy(&dev->lock);
        free(dev);
        dev = NULL;
    }

    return ESP_OK;
}

static const esp_cam_ircut_ops_t ap1511b_ops = {
    .priv_ioctl = ap1511b_priv_ioctl,
    .del = ap1511b_delete
};

esp_cam_ircut_device_t *ap1511b_detect(const esp_cam_ircut_config_t *config)
{
    esp_cam_ircut_device_t *dev = NULL;
    if (config == NULL) {
        return NULL;
    }

    dev = calloc(1, sizeof(esp_cam_ircut_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for IRCUT");
        return NULL;
    }

    if (pthread_mutex_init(&dev->lock, NULL) != 0) {
        ESP_LOGE(TAG, "failed to init IRCUT mutex");
        free(dev);
        return NULL;
    }

    dev->name = (char *)TAG;
    dev->gpio_pwdn = config->gpio_pwdn;
    dev->gpio_fbc = config->gpio_fbc;
    dev->ops = &ap1511b_ops;
    dev->hw_type = ESP_CAM_IRCUT_HW_SINGLE_LINE;

    // Configure IRCUT power
    if (ap1511b_hw_power_on(dev, true) != ESP_OK) {
        ESP_LOGE(TAG, "IRCUT power on failed");
        goto err_free_handler;
    }

    gpio_config_t conf = { 0 };
    conf.pin_bit_mask = 1LL << dev->gpio_fbc;
    conf.mode = GPIO_MODE_OUTPUT;
    if (gpio_config(&conf) != ESP_OK) {
        ESP_LOGE(TAG, "gpio config failed");
        goto err_free_handler;
    }

    ESP_LOGI(TAG, "Detected Cam IRCUT");

    return dev;

err_free_handler:
    ap1511b_hw_power_on(dev, false);
    pthread_mutex_destroy(&dev->lock);
    free(dev);

    return NULL;
}
