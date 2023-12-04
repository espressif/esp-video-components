/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_camera.h"
#include "private_include/sim_picture.h"

#define SIM_CAM_ID_MAX  2

static const char *TAG = "sim";

struct sim_cam {
    int id;
    int fps;
    esp_timer_handle_t timer;
    struct sim_cam_rx rx;
};

static void IRAM_ATTR sim_cam_capture_timer_isr(void *arg)
{
    struct sim_cam *sim_cam = (struct sim_cam *)arg;
    struct sim_cam_rx *rx = &sim_cam->rx;

    rx->cb(rx->priv, sim_picture_jpeg, sim_picture_jpeg_len);
}

static int sim_get_supported_para_value(esp_camera_device_t *dev, uint32_t para_id, sensor_para_supported_value_t *value)
{
    return ESP_FAIL;
}

static int sim_get_para_value(esp_camera_device_t *dev, uint32_t para_id, uint32_t size, sensor_para_value_t *value)
{
    return ESP_FAIL;
}

static int sim_set_para_value(esp_camera_device_t *dev, uint32_t para_id, uint32_t size, sensor_para_value_t value)
{
    return ESP_FAIL;
}

static int sim_query_support_formats(esp_camera_device_t *dev, void *parry)
{
    return ESP_FAIL;
}

static int sim_query_support_capability(esp_camera_device_t *dev, void *arg)
{
    return ESP_FAIL;
}

static int sim_set_format(esp_camera_device_t *dev, void *format)
{
    return ESP_FAIL;
}

static int sim_get_format(esp_camera_device_t *dev, void *ret_format)
{
    return ESP_FAIL;
}

static int sim_priv_ioctl(esp_camera_device_t *dev, unsigned int cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    struct sim_cam *sim_cam = (struct sim_cam *)dev->priv;

    switch (cmd) {
    case CAM_SIM_S_RXCB: {
        struct sim_cam_rx *rx = (struct sim_cam_rx *)arg;

        memcpy(&sim_cam->rx, rx, sizeof(struct sim_cam_rx));
        break;
    }
    case CAM_SENSOR_S_STREAM: {
        int *flags = (int *)arg;

        if (*flags) {
            esp_timer_create_args_t capture_timer_args = {
                .callback = sim_cam_capture_timer_isr,
                .dispatch_method = ESP_TIMER_ISR,
                .arg = sim_cam,
                .name = "sim_cam_capture",
            };

            ret = esp_timer_create(&capture_timer_args, &sim_cam->timer);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to create timer ret=%x", ret);
                return ret;
            }

            ret = esp_timer_start_periodic(sim_cam->timer, 1000000 / sim_cam->fps);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to start timer ret=%x", ret);
                return ret;
            }
        } else {
            ret = esp_timer_stop(sim_cam->timer);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to stop timer ret=%x", ret);
                return ret;
            }

            ret = esp_timer_delete(sim_cam->timer);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to delete ret=%x", ret);
                return ret;
            }

            sim_cam->timer = NULL;
        }

        break;
    }
    case CAM_SENSOR_S_FPS: {
        int *fps = (int *)arg;

        sim_cam->fps = *fps;
        break;
    }
    default: {
        ESP_LOGE(TAG, "cmd=%08x is not supported", cmd);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }

    return ret;
}

static int sim_get_name(esp_camera_device_t *dev, void *buffer, size_t *size)
{
    int ret;
    struct sim_cam *sim_cam = (struct sim_cam *)dev->priv;

    ret = sprintf(buffer, "%s%d", CONFIG_CAMERA_SIM_NAME, sim_cam->id);
    if (ret <= 0) {
        return ESP_FAIL;
    }

    *size = (size_t)ret;

    return ESP_OK;
}

static esp_camera_ops_t s_sim_cams = {
    .get_supported_para_value = sim_get_supported_para_value,
    .get_para_value           = sim_get_para_value,
    .set_para_value           = sim_set_para_value,
    .query_support_formats    = sim_query_support_formats,
    .query_support_capability = sim_query_support_capability,
    .set_format               = sim_set_format,
    .get_format               = sim_get_format,
    .priv_ioctl               = sim_priv_ioctl,
    .get_name                 = sim_get_name,
};

esp_camera_device_t *sim_detect(const esp_camera_sim_config_t *config)
{
    esp_camera_device_t *cam_dev;
    struct sim_cam *sim_cam;

    ESP_LOGI(TAG, "sim_detect");

    if (config->id >= SIM_CAM_ID_MAX) {
        ESP_LOGE(TAG, "sim camera id=%d is not supported", config->id);
        return NULL;
    }

    cam_dev = heap_caps_calloc(1, sizeof(esp_camera_device_t), MALLOC_CAP_8BIT);
    if (!cam_dev) {
        ESP_LOGE(TAG, "failed to call calloc cam_dev");
        return NULL;
    }

    sim_cam = heap_caps_calloc(1, sizeof(struct sim_cam), MALLOC_CAP_8BIT);
    if (!sim_cam) {
        ESP_LOGE(TAG, "failed to call calloc sim_cam");
        heap_caps_free(cam_dev);
        return NULL;
    }

    sim_cam->id = config->id;

    cam_dev->ops  = &s_sim_cams;
    cam_dev->priv = sim_cam;

    return cam_dev;
}

#if CONFIG_CAMERA_SIM_AUTO_DETECT
ESP_CAMERA_DETECT_FN(sim_detect, CAMERA_INTF_SIM)
{
    return sim_detect(config);
}
#endif
