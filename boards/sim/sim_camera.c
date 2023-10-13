/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_video.h"
#include "sim_picture.h"

#define SIM_CAMERA_DEVICE_NAME      "sim_camera"
#define SIM_CAMERA_FPS_DEFAULT      20
#define SIM_CAMERA_BUFFER_COUNT     4
#define SIM_CAMERA_BUFFER_SIZE      (sim_picture_jpeg_len)

struct sim_camera {
    esp_timer_handle_t capture_timer;
    int fps;
};

static const char *TAG = "sim_camera";

static void sim_camera_capture_timer_isr(void *arg)
{
    uint8_t *buffer;
    struct esp_video *video = (struct esp_video *)arg;

    buffer = esp_video_alloc_buffer(video);
    if (!buffer) {
        ESP_EARLY_LOGE(TAG, "Failed to allocte video buffer");
        return;
    }

    memcpy(buffer, sim_picture_jpeg, sim_picture_jpeg_len);
    esp_video_recvdone_buffer(video, buffer);
}

static esp_err_t sim_camera_init(struct esp_video *video)
{
    esp_err_t ret;
    struct sim_camera *camera = (struct sim_camera *)video->priv;

    camera->capture_timer = NULL;
    camera->fps = SIM_CAMERA_FPS_DEFAULT;

    esp_timer_create_args_t capture_timer_args = {
        .callback = sim_camera_capture_timer_isr,
        .dispatch_method = ESP_TIMER_ISR,
        .arg = video,
        .name = "camera_capture",
    };

    ret = esp_timer_create(&capture_timer_args, &camera->capture_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create timer ret=%x", ret);
        return ret;
    }

    return ESP_OK;
}

static esp_err_t sim_camera_deinit(struct esp_video *video)
{
    esp_err_t ret;
    struct sim_camera *camera = (struct sim_camera *)video->priv;
    
    ret = esp_timer_delete(camera->capture_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete timer ret=%x", ret);
        return ret;
    }

    camera->capture_timer = NULL;

    return ESP_OK;
}

static esp_err_t sim_camera_start_capture(struct esp_video *video)
{
    esp_err_t ret;
    struct sim_camera *camera = (struct sim_camera *)video->priv;
    
    ret = esp_timer_start_periodic(camera->capture_timer, 1000000 / camera->fps);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start timer ret=%x", ret);
        return ret;
    }

    return ESP_OK;
}

static esp_err_t sim_camera_stop_capture(struct esp_video *video)
{
    esp_err_t ret;
    struct sim_camera *camera = (struct sim_camera *)video->priv;
    
    ret = esp_timer_stop(camera->capture_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop timer ret=%x", ret);
        return ret;
    }

    return ESP_OK;
}

static esp_err_t sim_camera_set_attr(struct esp_video *video, int cmd, void *arg)
{
    esp_err_t ret = ESP_FAIL;
    struct sim_camera *camera = (struct sim_camera *)video->priv;

    switch (cmd) {
        case ESP_VIDEO_CTRL_FPS:
            if (!arg) {
                ESP_LOGE(TAG, "arg=NULL");
                return ESP_ERR_INVALID_ARG;
            }

            camera->fps = *(int *)arg;
            ret = ESP_OK;
            break;
        default:
            ESP_LOGE(TAG, "cmd=%d is not supported", cmd);
            break;
    }

    return ret;
}

static esp_err_t sim_camera_get_attr(struct esp_video *video, int cmd, void *arg)
{
    esp_err_t ret = ESP_FAIL;
    struct sim_camera *camera = (struct sim_camera *)video->priv;

    switch (cmd) {
        case ESP_VIDEO_CTRL_FPS:
            if (!arg) {
                ESP_LOGE(TAG, "arg=NULL");
                return ESP_ERR_INVALID_ARG;
            }

            *(int *)arg = camera->fps;
            ret = ESP_OK;
            break;
        default:
            ESP_LOGE(TAG, "cmd=%d is not supported", cmd);
            break;
    }

    return ret;
}

static esp_err_t sim_camera_capability(struct esp_video *video, struct esp_video_capability *capability)
{
    if (!capability) {
        ESP_LOGE(TAG, "capability=NULL");
        return ESP_ERR_INVALID_ARG;
    }

    memset(capability, 0, sizeof(struct esp_video_capability));
    capability->fmt_jpeg = 1;

    return ESP_OK;
}

static esp_err_t sim_camera_description(struct esp_video *video, char *buffer, size_t size)
{
    int ret;

    ret = snprintf(buffer, size, "Simulation Camera:\n\tFormat: RGB565\n\tPixel: 1080P\n");
    if (ret <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static const struct esp_video_ops s_sim_camera_ops = {
    .init          = sim_camera_init,
    .deinit        = sim_camera_deinit,
    .start_capture = sim_camera_start_capture,
    .stop_capture  = sim_camera_stop_capture,
    .set_attr      = sim_camera_set_attr,
    .get_attr      = sim_camera_get_attr,
    .capability    = sim_camera_capability,
    .description   = sim_camera_description
};

esp_err_t sim_initialize_camera(void)
{
    struct esp_video *video;
    struct sim_camera *camera;

    camera = heap_caps_malloc(sizeof(struct sim_camera), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!camera) {
        return ESP_ERR_NO_MEM;
    }

    video = esp_video_create(SIM_CAMERA_DEVICE_NAME, &s_sim_camera_ops, camera,
                             SIM_CAMERA_BUFFER_COUNT, SIM_CAMERA_BUFFER_SIZE);
    if (!video) {
        heap_caps_free(camera);
        return ESP_FAIL;
    }

    return ESP_OK;
}
