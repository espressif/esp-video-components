/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_cam_led.h"
#include "tcj60.h"

#define TCJ60_SPEED_MODE          LEDC_LOW_SPEED_MODE
#define TCJ60_TIMER               ((ledc_timer_t)CONFIG_CAM_LED_TCJ60_LEDC_TIMER)
#define TCJ60_CHANNEL             ((ledc_channel_t)CONFIG_CAM_LED_TCJ60_LEDC_CHANNEL)
#define TCJ60_FREQ_HZ             CONFIG_CAM_LED_TCJ60_FREQ_HZ
#define TCJ60_MAX_DUTY_RES        LEDC_TIMER_11_BIT
#define TCJ60_BRIGHTNESS_MAX      2       /* 3 levels: 0 off, 1 half, 2 full, converted to the LEDC duty here */

typedef struct {
    esp_cam_led_dev_info_t info;
    uint32_t duty_max;                        /* LEDC duty of the full brightness, 2^duty_resolution */
} tcj60_priv_t;

static const char *TAG = "tcj60";

static esp_err_t tcj60_set_brightness(esp_cam_led_device_t *dev, uint32_t brightness)
{
    tcj60_priv_t *priv = (tcj60_priv_t *)dev->reserved;
    esp_err_t ret;
    uint32_t duty;

    if (brightness > priv->info.brightness_range_max) {
        brightness = priv->info.brightness_range_max;
    }
    /* The PWM duty is linear to the LED current, so to the light output */
    duty = (brightness * priv->duty_max + TCJ60_BRIGHTNESS_MAX / 2) / TCJ60_BRIGHTNESS_MAX;

    /* Takes effect at once, the IPA white light algorithm measures the luma right after the brightness changes */
    ret = ledc_set_duty(TCJ60_SPEED_MODE, TCJ60_CHANNEL, duty);
    if (ret == ESP_OK) {
        ret = ledc_update_duty(TCJ60_SPEED_MODE, TCJ60_CHANNEL);
    }
    if (ret != ESP_OK) {
        return ret;
    }

    dev->brightness = brightness;
    return ESP_OK;
}

static esp_err_t tcj60_priv_ioctl(esp_cam_led_device_t *dev, uint32_t cmd, void *arg)
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
        ret = tcj60_set_brightness(dev, brightness);
        pthread_mutex_unlock(&dev->lock);
        break;
    }
    case ESP_CAM_LED_IOCTL_GET_BRIGHTNESS: {
        if (pthread_mutex_lock(&dev->lock) != 0) {
            return ESP_ERR_INVALID_STATE;
        }
        *(uint32_t *)arg = dev->brightness;
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

static esp_err_t tcj60_delete(esp_cam_led_device_t *dev)
{
    tcj60_priv_t *priv = (tcj60_priv_t *)dev->reserved;

    ESP_LOGW(TAG, "delete tcj60 (%p)", dev);
    /* The pin stays driven at the off level, a reset pin is pulled up and may turn the LED on */
    ledc_stop(TCJ60_SPEED_MODE, TCJ60_CHANNEL, 0);
    if (dev->gpio_en != -1) {
        gpio_set_level(dev->gpio_en, 0);
    }
    /* The LEDC timer may be shared with other channels, so it is kept */
    pthread_mutex_destroy(&dev->lock);
    free(priv);
    free(dev);
    return ESP_OK;
}

static const esp_cam_led_ops_t tcj60_ops = {
    .priv_ioctl = tcj60_priv_ioctl,
    .del = tcj60_delete
};

esp_cam_led_device_t *tcj60_detect(const esp_cam_led_config_t *config)
{
    esp_cam_led_device_t *dev = NULL;
    tcj60_priv_t *priv = NULL;
    esp_err_t ret;

    if (config == NULL) {
        return NULL;
    }

    if (config->type != ESP_CAM_LED_HW_TYPE_LEDC) {
        ESP_LOGE(TAG, "invalid LED type");
        return NULL;
    }

    if (config->gpio_ctrl == -1) {
        ESP_LOGE(TAG, "PWM output pin is not set");
        return NULL;
    }

    ledc_timer_config_t timer = {
        .speed_mode = TCJ60_SPEED_MODE,
        .duty_resolution = TCJ60_MAX_DUTY_RES,
        .timer_num = TCJ60_TIMER,
        .freq_hz = TCJ60_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    /*
     * The LEDC clock must reach freq_hz * 2^duty_resolution. LEDC_AUTO_CLK does not tell which clock it selects,
     * so the resolution is lowered until the timer accepts it. The brightness has only 3 levels, so a lower
     * resolution does not matter.
     */
    while ((ret = ledc_timer_config(&timer)) != ESP_OK && timer.duty_resolution > LEDC_TIMER_1_BIT) {
        timer.duty_resolution--;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to config LEDC timer, %d Hz", TCJ60_FREQ_HZ);
        return NULL;
    }

    ledc_channel_config_t channel = {
        .gpio_num = config->gpio_ctrl,
        .speed_mode = TCJ60_SPEED_MODE,
        .channel = TCJ60_CHANNEL,
        .timer_sel = TCJ60_TIMER,
        .duty = 0,
        .hpoint = 0,
#if CONFIG_CAM_LED_TCJ60_OUTPUT_INVERT
        .flags.output_invert = 1,
#endif
    };
    if (ledc_channel_config(&channel) != ESP_OK) {
        ESP_LOGE(TAG, "failed to config LEDC channel");
        return NULL;
    }

    if (config->gpio_en != -1) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1ULL << config->gpio_en;
        conf.mode = GPIO_MODE_OUTPUT;
        if (gpio_config(&conf) != ESP_OK || gpio_set_level(config->gpio_en, 1) != ESP_OK) {
            ESP_LOGE(TAG, "failed to config enable GPIO");
            goto err_stop;
        }
    }

    dev = calloc(1, sizeof(esp_cam_led_device_t));
    priv = calloc(1, sizeof(tcj60_priv_t));
    if (dev == NULL || priv == NULL) {
        ESP_LOGE(TAG, "No memory for LED");
        goto err_free;
    }

    if (pthread_mutex_init(&dev->lock, NULL) != 0) {
        ESP_LOGE(TAG, "failed to init LED mutex");
        goto err_free;
    }

    priv->info.type = ESP_CAM_LED_HW_TYPE_LEDC;
    priv->info.spectrum = ESP_CAM_LED_SPECTRUM_VISIBLE;
    priv->info.brightness_range_min = 0;
    priv->info.brightness_range_max = TCJ60_BRIGHTNESS_MAX;
    /* LEDC accepts 2^duty_resolution as fully on */
    priv->duty_max = 1UL << timer.duty_resolution;

    dev->name = (char *)TAG;
    dev->info = &priv->info;
    dev->gpio_ctrl = config->gpio_ctrl;
    dev->gpio_en = config->gpio_en;
    dev->brightness = 0;
    dev->ops = &tcj60_ops;
    dev->reserved = priv;

    ESP_LOGI(TAG, "Detected Cam LED, GPIO%d, %d Hz, %d bits", config->gpio_ctrl, TCJ60_FREQ_HZ,
             (int)timer.duty_resolution);

    return dev;

err_free:
    free(priv);
    free(dev);
    if (config->gpio_en != -1) {
        gpio_set_level(config->gpio_en, 0);
    }
err_stop:
    ledc_stop(TCJ60_SPEED_MODE, TCJ60_CHANNEL, 0);
    return NULL;
}
