/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_log.h"
#include "esp_cam_als.h"
#include "pt1411.h"

static const char *TAG = "pt1411";

static bool pt1411_adc_calibration_init(adc_unit_t unit, adc_channel_t channel,
                                        adc_atten_t atten, adc_bitwidth_t bitwidth,
                                        adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGD(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = bitwidth,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGD(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = bitwidth,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

static void pt1411_adc_calibration_deinit(adc_cali_handle_t handle)
{
    if (handle == NULL) {
        return;
    }

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    if (adc_cali_delete_scheme_curve_fitting(handle) == ESP_OK) {
        return;
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
    adc_cali_delete_scheme_line_fitting(handle);
#endif
}

/**
 * @brief Sample: read ADC twice and average (+ optional cali_raw_to_voltage), update raw_value.
 * @note Caller must hold dev->lock.
 */
static esp_err_t pt1411_sample_update_raw(esp_cam_als_device_t *dev)
{
    int adc_raw0 = 0;
    int adc_raw1 = 0;
    int adc_raw = 0;
    int voltage_mv = 0;
    esp_err_t ret;

    ret = adc_oneshot_read(dev->adc_handle, dev->channel, &adc_raw0);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "raw read failed");
        return ret;
    }
    ret = adc_oneshot_read(dev->adc_handle, dev->channel, &adc_raw1);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "raw read failed");
        return ret;
    }
    adc_raw = (adc_raw0 + adc_raw1) / 2;

    if (dev->cali_enable && dev->cali_handle) {
        ret = adc_cali_raw_to_voltage(dev->cali_handle, adc_raw, &voltage_mv);
        if (ret == ESP_OK) {
            /* Store calibrated voltage in mV when calibration is available */
            dev->raw_value = (uint16_t)voltage_mv;
            return ESP_OK;
        }
        ESP_LOGW(TAG, "cali_raw_to_voltage failed");
    }

    dev->raw_value = (uint16_t)adc_raw;
    return ESP_OK;
}

static void pt1411_timer_callback(TimerHandle_t timer)
{
    esp_cam_als_device_t *dev = (esp_cam_als_device_t *)pvTimerGetTimerID(timer);

    if (dev == NULL) {
        return;
    }

    /* Timer task context: do not block if the device is busy */
    if (pthread_mutex_trylock(&dev->lock) != 0) {
        return;
    }

    pt1411_sample_update_raw(dev);
    pthread_mutex_unlock(&dev->lock);
}

static esp_err_t pt1411_priv_ioctl(esp_cam_als_device_t *dev, uint32_t cmd, void *arg)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    esp_err_t ret = ESP_OK;

    switch (cmd) {
    case ESP_CAM_ALS_IOCTL_GET_RAW:
        ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);
        if (pthread_mutex_lock(&dev->lock) != 0) {
            return ESP_ERR_INVALID_STATE;
        }
        *(uint32_t *)arg = (uint32_t)dev->raw_value;
        pthread_mutex_unlock(&dev->lock);
        break;

    case ESP_CAM_ALS_IOCTL_GET_RAW_FORCE_READ:
        ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);
        if (pthread_mutex_lock(&dev->lock) != 0) {
            return ESP_ERR_INVALID_STATE;
        }
        ret = pt1411_sample_update_raw(dev);
        if (ret == ESP_OK) {
            *(uint32_t *)arg = (uint32_t)dev->raw_value;
        }
        pthread_mutex_unlock(&dev->lock);
        break;

    case ESP_CAM_ALS_IOCTL_SET_SAMPLE_RATE: {
        ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);
        uint16_t period_ms = *(uint16_t *)arg;
        if (period_ms == 0) {
            return ESP_ERR_INVALID_ARG;
        }
        if (pthread_mutex_lock(&dev->lock) != 0) {
            return ESP_ERR_INVALID_STATE;
        }
        if (dev->timer == NULL) {
            pthread_mutex_unlock(&dev->lock);
            return ESP_ERR_INVALID_STATE;
        }
        ALS_TIMER_STOP(dev->timer);
        if (ALS_TIMER_CHANGE_PERIOD(dev->timer, period_ms) != pdPASS) {
            ALS_TIMER_START(dev->timer);
            pthread_mutex_unlock(&dev->lock);
            return ESP_FAIL;
        }
        ALS_TIMER_START(dev->timer);
        dev->sample_rate_ms = (uint16_t)period_ms;
        pthread_mutex_unlock(&dev->lock);
        break;
    }

    case ESP_CAM_ALS_IOCTL_GET_SAMPLE_RATE:
        ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);
        if (pthread_mutex_lock(&dev->lock) != 0) {
            return ESP_ERR_INVALID_STATE;
        }
        *(uint16_t *)arg = (uint16_t)dev->sample_rate_ms;
        pthread_mutex_unlock(&dev->lock);
        break;

    case ESP_CAM_ALS_IOCTL_GET_INFO:
        ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);
        memcpy((esp_cam_als_dev_info_t *)arg, dev->info, sizeof(esp_cam_als_dev_info_t));
        break;

    case ESP_CAM_ALS_IOCTL_LOW_POWER_SWITCH: {
        ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);
        int enable = *(int *)arg;
        if (pthread_mutex_lock(&dev->lock) != 0) {
            return ESP_ERR_INVALID_STATE;
        }
        if (dev->timer == NULL) {
            pthread_mutex_unlock(&dev->lock);
            return ESP_ERR_INVALID_STATE;
        }
        if (enable) {
            ALS_TIMER_STOP(dev->timer);
        } else {
            ALS_TIMER_START(dev->timer);
        }
        pthread_mutex_unlock(&dev->lock);
        break;
    }

    default:
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }

    return ret;
}

static esp_err_t pt1411_delete(esp_cam_als_device_t *dev)
{
    ESP_LOGW(TAG, "del pt1411 (%p)", dev);
    if (dev) {
        if (dev->timer) {
            ALS_TIMER_STOP(dev->timer);
            ALS_TIMER_DELETE(dev->timer);
            dev->timer = NULL;
        }
        if (dev->cali_enable) {
            pt1411_adc_calibration_deinit(dev->cali_handle);
            dev->cali_handle = NULL;
            dev->cali_enable = false;
        }
        pthread_mutex_destroy(&dev->lock);
        free(dev);
        dev = NULL;
    }

    return ESP_OK;
}

static const esp_cam_als_ops_t pt1411_ops = {
    .priv_ioctl = pt1411_priv_ioctl,
    .del = pt1411_delete
};

static const esp_cam_als_dev_info_t pt1411_info = {
    .type = ESP_CAM_ALS_HW_TYPE_ADC,
    .spectrum = ALS_SPECTRUM_VISIBLE,
    .raw_max = 4095,
};

esp_cam_als_device_t *pt1411_detect(const esp_cam_als_config_t *config)
{
    esp_cam_als_device_t *dev = NULL;
    uint16_t sample_rate_ms;

    if (config == NULL) {
        return NULL;
    }

    sample_rate_ms = config->sample_rate_ms ? config->sample_rate_ms : CONFIG_PT1411_DEFAULT_SAMPLE_RATE_MS;

    dev = calloc(1, sizeof(esp_cam_als_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for ALS");
        return NULL;
    }

    if (pthread_mutex_init(&dev->lock, NULL) != 0) {
        ESP_LOGE(TAG, "failed to init ALS mutex");
        free(dev);
        return NULL;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = config->atten,
        .bitwidth = config->bitwidth,
    };
    if (adc_oneshot_config_channel(config->adc_handle, config->channel, &chan_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "failed to config ADC channel");
        goto err_free_handler;
    }

    dev->name = (char *)TAG;
    dev->ops = &pt1411_ops;
    dev->info = &pt1411_info;
    dev->sample_rate_ms = sample_rate_ms;
    dev->adc_handle = config->adc_handle;
    dev->channel = config->channel;
    dev->cali_enable = false;
    dev->cali_handle = NULL;
    dev->raw_value = 0;

    if (config->cali_enable) { // Todo, Move to ESP video if possible
        adc_cali_handle_t cali_handle = NULL;
        if (pt1411_adc_calibration_init(config->unit, config->channel,
                                        config->atten, config->bitwidth,
                                        &cali_handle)) {
            dev->cali_handle = cali_handle;
            dev->cali_enable = true;
        } else {
            ESP_LOGW(TAG, "ADC calibration unavailable");
        }
    }

    /* Initial sample before starting periodic timer */
    if (pthread_mutex_lock(&dev->lock) == 0) {
        pt1411_sample_update_raw(dev);
        pthread_mutex_unlock(&dev->lock);
    }

    dev->timer = ALS_TIMER_CREATE("pt1411_tmr", sample_rate_ms, pt1411_timer_callback, dev);
    if (dev->timer == NULL) {
        ESP_LOGE(TAG, "failed to create ALS timer");
        goto err_free_handler;
    }
    if (ALS_TIMER_START(dev->timer) != pdPASS) {
        ESP_LOGE(TAG, "failed to start ALS timer");
        goto err_free_handler;
    }

    ESP_LOGI(TAG, "Detected Cam ALS, sample_rate=%u ms, cali=%d",
             sample_rate_ms, (int)dev->cali_enable);

    return dev;

err_free_handler:
    if (dev->timer) {
        ALS_TIMER_STOP(dev->timer);
        ALS_TIMER_DELETE(dev->timer);
        dev->timer = NULL;
    }
    if (dev->cali_enable) {
        pt1411_adc_calibration_deinit(dev->cali_handle);
        dev->cali_handle = NULL;
        dev->cali_enable = false;
    }
    pthread_mutex_destroy(&dev->lock);
    free(dev);

    return NULL;
}
