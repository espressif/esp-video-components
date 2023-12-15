/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_camera.h"
#include "private_include/sim_picture.h"

#define SIM_CAM_ID_MAX  2

#define SIM_DEFAULT_FPS 1
#define SIM_DEFAULT_3A_LOCK 0
#define SIM_DEFAULT_FLASH_LEDC V4L2_FLASH_LED_MODE_NONE

struct sim_cam {
    int id;
    int fps;
    int _3a_lock;
    int flash_led;
    esp_timer_handle_t timer;
    struct sim_cam_rx rx;
};

static const char *TAG = "sim";
static const uint8_t s_sim_flash_led_dims[] = {
    V4L2_FLASH_LED_MODE_NONE,
    V4L2_FLASH_LED_MODE_FLASH,
    V4L2_FLASH_LED_MODE_TORCH
};

static void IRAM_ATTR sim_cam_capture_timer_isr(void *arg)
{
    struct sim_cam *sim_cam = (struct sim_cam *)arg;
    struct sim_cam_rx *rx = &sim_cam->rx;

    rx->cb(rx->priv, sim_picture_jpeg, sim_picture_jpeg_len);
}

static int sim_query_para_desc(esp_camera_device_t *dev, struct v4l2_query_ext_ctrl *qctrl)
{
    esp_err_t ret = ESP_OK;

    switch (qctrl->id) {
    case CAM_SENSOR_FPS:
        qctrl->type = V4L2_CTRL_TYPE_INTEGER;
        qctrl->minimum = 1;
        qctrl->maximum = 100;
        qctrl->step = 1;
        qctrl->elems = 1;
        qctrl->nr_of_dims = 0;
        qctrl->default_value = SIM_DEFAULT_FPS;
        break;
    case CAM_SENSOR_3A_LOCK:
        qctrl->type = V4L2_CTRL_TYPE_BITMASK;
        qctrl->minimum = 0;
        qctrl->maximum = 0x7;
        qctrl->step = 1;
        qctrl->elems = 1;
        qctrl->nr_of_dims = 0;
        qctrl->default_value = SIM_DEFAULT_3A_LOCK;
        break;
    case CAM_SENSOR_FLASH_LED:
        qctrl->type = V4L2_CTRL_TYPE_INTEGER_MENU;
        qctrl->elem_size = sizeof(uint8_t);
        qctrl->elems = ARRAY_SIZE(s_sim_flash_led_dims);
        qctrl->nr_of_dims = ARRAY_SIZE(s_sim_flash_led_dims);
        for (int i = 0; i < qctrl->nr_of_dims; i++) {
            qctrl->dims[i] = s_sim_flash_led_dims[i];
        }
        qctrl->default_value = SIM_DEFAULT_FLASH_LEDC;
        break;
    default: {
        ESP_LOGE(TAG, "id=%" PRIx32 " is not supported", qctrl->id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }

    return ret;
}

static int sim_get_para_value(esp_camera_device_t *dev, struct v4l2_ext_control *ctrl)
{
    esp_err_t ret = ESP_OK;
    struct sim_cam *sim_cam = (struct sim_cam *)dev->priv;

    switch (ctrl->id) {
    case CAM_SENSOR_FPS: {
        ctrl->value = sim_cam->fps;
        break;
    }
    case CAM_SENSOR_3A_LOCK: {
        ctrl->value = sim_cam->_3a_lock;
        break;
    }
    case CAM_SENSOR_FLASH_LED: {
        ctrl->value = sim_cam->flash_led;
        break;
    }
    default: {
        ESP_LOGE(TAG, "get id=%" PRIx32 " is not supported", ctrl->id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }

    return ret;
}

static int sim_set_para_value(esp_camera_device_t *dev, const struct v4l2_ext_control *ctrl)
{
    esp_err_t ret = ESP_OK;
    struct sim_cam *sim_cam = (struct sim_cam *)dev->priv;

    switch (ctrl->id) {
    case CAM_SENSOR_FPS: {
        sim_cam->fps = ctrl->value;
        break;
    }
    case CAM_SENSOR_3A_LOCK: {
        sim_cam->_3a_lock = ctrl->value;
        break;
    }
    case CAM_SENSOR_FLASH_LED: {
        sim_cam->flash_led = ctrl->value;
        break;
    }
    default: {
        ESP_LOGE(TAG, "set id=%" PRIx32 " is not supported", ctrl->id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }

    return ret;
}

static int sim_query_support_formats(esp_camera_device_t *dev, void *parry)
{
    return ESP_FAIL;
}

static int sim_query_support_capability(esp_camera_device_t *dev, void *arg)
{
    return ESP_FAIL;
}

static int sim_set_format(esp_camera_device_t *dev, const sensor_format_t *format)
{
    return ESP_FAIL;
}

static int sim_get_format(esp_camera_device_t *dev, sensor_format_t *format)
{
    return ESP_FAIL;
}

static int sim_priv_ioctl(esp_camera_device_t *dev, unsigned int cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    struct sim_cam *sim_cam = (struct sim_cam *)dev->priv;

    switch (cmd) {
    case CAM_SENSOR_IOC_S_STREAM: {
        int enable = *(int *)arg;

        if (enable) {
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
    /**
     * Todo: AEG-1098
     */
    case CAM_SIM_IOC_S_RXCB: {
        struct sim_cam_rx *rx = (struct sim_cam_rx *)arg;

        memcpy(&sim_cam->rx, rx, sizeof(struct sim_cam_rx));
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

static esp_camera_ops_t s_sim_cam_ops = {
    .query_para_desc          = sim_query_para_desc,
    .get_para_value           = sim_get_para_value,
    .set_para_value           = sim_set_para_value,
    .query_support_formats    = sim_query_support_formats,
    .query_support_capability = sim_query_support_capability,
    .set_format               = sim_set_format,
    .get_format               = sim_get_format,
    .priv_ioctl               = sim_priv_ioctl,
};

esp_camera_device_t *sim_detect(const esp_camera_sim_config_t *config)
{
    int ret;
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

    ret = asprintf(&cam_dev->name, "%s%d", CONFIG_CAMERA_SIM_NAME, config->id);
    if (ret < 0) {
        heap_caps_free(cam_dev);
        return NULL;
    }

    sim_cam = heap_caps_calloc(1, sizeof(struct sim_cam), MALLOC_CAP_8BIT);
    if (!sim_cam) {
        ESP_LOGE(TAG, "failed to call calloc sim_cam");
        free(cam_dev->name);
        heap_caps_free(cam_dev);
        return NULL;
    }

    sim_cam->id = config->id;
    sim_cam->fps = SIM_DEFAULT_FPS;
    sim_cam->_3a_lock = SIM_DEFAULT_3A_LOCK;
    sim_cam->flash_led = SIM_DEFAULT_FLASH_LEDC;

    cam_dev->ops  = &s_sim_cam_ops;
    cam_dev->priv = sim_cam;

    return cam_dev;
}

#if CONFIG_CAMERA_SIM_AUTO_DETECT
ESP_CAMERA_DETECT_FN(sim_detect, CAMERA_INTF_SIM)
{
    return sim_detect(config);
}
#endif
