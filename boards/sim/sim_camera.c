/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "esp_timer.h"
#include "esp_video.h"
#include "private/esp_video_log.h"
#include "sim_picture.h"

#define SIM_CAMERA_DEVICE_NAME      CONFIG_ESP_VIDEO_SIMULATION_CAMERA_NAME
#define SIM_CAMERA_BUFFER_COUNT     CONFIG_ESP_VIDEO_SIMULATION_CAMERA_BUFFER_COUNT
#define SIM_CAMERA_COUNT            CONFIG_ESP_VIDEO_SIMULATION_CAMERA_COUNT
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
    esp_video_recvdone_buffer(video, buffer, sim_picture_jpeg_len);
}

static esp_err_t sim_camera_init(struct esp_video *video)
{
    esp_err_t ret;
    struct sim_camera *camera = (struct sim_camera *)video->priv;

    camera->capture_timer = NULL;
    camera->fps = 0;

    esp_timer_create_args_t capture_timer_args = {
        .callback = sim_camera_capture_timer_isr,
        .dispatch_method = ESP_TIMER_ISR,
        .arg = video,
        .name = "camera_capture",
    };

    ret = esp_timer_create(&capture_timer_args, &camera->capture_timer);
    if (ret != ESP_OK) {
        ESP_VIDEO_LOGE("Failed to create timer ret=%x", ret);
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
        ESP_VIDEO_LOGE("Failed to delete ret=%x", ret);
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
        ESP_VIDEO_LOGE("Failed to start timer ret=%x", ret);
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
        ESP_VIDEO_LOGE("Failed to stop timer ret=%x", ret);
        return ret;
    }

    return ESP_OK;
}

static esp_err_t sim_camera_set_format(struct esp_video *video, const struct esp_video_format *format)
{
    struct sim_camera *camera = (struct sim_camera *)video->priv;

    camera->fps = (int)format->fps;

    return ESP_OK;
}

static esp_err_t sim_camera_capability(struct esp_video *video, struct esp_video_capability *capability)
{
    if (!capability) {
        ESP_VIDEO_LOGE("capability=NULL");
        return ESP_ERR_INVALID_ARG;
    }

    memset(capability, 0, sizeof(struct esp_video_capability));
    capability->fmt_jpeg = 1;

    return ESP_OK;
}

static esp_err_t sim_camera_description(struct esp_video *video, char *buffer, uint32_t size)
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
    .set_format    = sim_camera_set_format,
    .capability    = sim_camera_capability,
    .description   = sim_camera_description
};

esp_err_t sim_initialize_camera(void)
{
    int ret;
    char *name;
    struct esp_video *video;
    struct sim_camera *camera;

    for (int i = 0; i < SIM_CAMERA_COUNT; i++) {
        ret = asprintf(&name, "%s%d", SIM_CAMERA_DEVICE_NAME, i);
        assert(ret > 0);

        camera = heap_caps_malloc(sizeof(struct sim_camera), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
        assert(camera);

        video = esp_video_create(name, &s_sim_camera_ops, camera,
                                 SIM_CAMERA_BUFFER_COUNT, SIM_CAMERA_BUFFER_SIZE);
        assert(video);
        free(name);
    }

    return ESP_OK;
}
