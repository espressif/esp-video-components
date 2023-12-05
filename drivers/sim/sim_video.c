/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "esp_timer.h"
#include "esp_video.h"
#include "esp_video_log.h"
#include "sim_picture.h"

#define SIM_CAMERA_DEVICE_NAME      CONFIG_SIMULATED_INTF_DEVICE_NAME
#define SIM_CAMERA_BUFFER_COUNT     CONFIG_SIMULATED_INTF_DEVICE_BUFFER_COUNT
#define SIM_CAMERA_COUNT            CONFIG_SIMULATED_INTF_DEVICE_COUNT
#define SIM_CAMERA_BUFFER_SIZE      (sim_picture_jpeg_len)

struct sim_video {
    esp_timer_handle_t capture_timer;
    int fps;
};

static const char *TAG = "sim_video";

static void sim_video_capture_timer_isr(void *arg)
{
    uint8_t *buffer;
    struct esp_video *video = (struct esp_video *)arg;

    buffer = esp_video_alloc_buffer(video);
    if (!buffer) {
        ESP_EARLY_LOGE(TAG, "Failed to allocte video buffer");
        return;
    }

    memcpy(buffer, sim_picture_jpeg, sim_picture_jpeg_len);
    esp_video_recvdone_buffer(video, buffer, sim_picture_jpeg_len, 0);
}

static esp_err_t sim_video_init(struct esp_video *video)
{
    esp_err_t ret;
    struct sim_video *sim_video = (struct sim_video *)video->priv;

    sim_video->capture_timer = NULL;
    sim_video->fps = 0;

    esp_timer_create_args_t capture_timer_args = {
        .callback = sim_video_capture_timer_isr,
        .dispatch_method = ESP_TIMER_ISR,
        .arg = video,
        .name = "sim_video_capture",
    };

    ret = esp_timer_create(&capture_timer_args, &sim_video->capture_timer);
    if (ret != ESP_OK) {
        ESP_VIDEO_LOGE("Failed to create timer ret=%x", ret);
        return ret;
    }

    return ESP_OK;
}

static esp_err_t sim_video_deinit(struct esp_video *video)
{
    esp_err_t ret;
    struct sim_video *sim_video = (struct sim_video *)video->priv;

    ret = esp_timer_delete(sim_video->capture_timer);
    if (ret != ESP_OK) {
        ESP_VIDEO_LOGE("Failed to delete ret=%x", ret);
        return ret;
    }

    sim_video->capture_timer = NULL;

    return ESP_OK;
}

static esp_err_t sim_video_start_capture(struct esp_video *video)
{
    esp_err_t ret;
    struct sim_video *sim_video = (struct sim_video *)video->priv;

    ret = esp_timer_start_periodic(sim_video->capture_timer, 1000000 / sim_video->fps);
    if (ret != ESP_OK) {
        ESP_VIDEO_LOGE("Failed to start timer ret=%x", ret);
        return ret;
    }

    return ESP_OK;
}

static esp_err_t sim_video_stop_capture(struct esp_video *video)
{
    esp_err_t ret;
    struct sim_video *sim_video = (struct sim_video *)video->priv;

    ret = esp_timer_stop(sim_video->capture_timer);
    if (ret != ESP_OK) {
        ESP_VIDEO_LOGE("Failed to stop timer ret=%x", ret);
        return ret;
    }

    return ESP_OK;
}

static esp_err_t sim_video_set_format(struct esp_video *video, const struct esp_video_format *format)
{
    struct sim_video *sim_video = (struct sim_video *)video->priv;

    sim_video->fps = (int)format->fps;

    return ESP_OK;
}

static esp_err_t sim_video_capability(struct esp_video *video, struct esp_video_capability *capability)
{
    if (!capability) {
        ESP_VIDEO_LOGE("capability=NULL");
        return ESP_ERR_INVALID_ARG;
    }

    memset(capability, 0, sizeof(struct esp_video_capability));
    capability->fmt_jpeg = 1;

    return ESP_OK;
}

static esp_err_t sim_video_description(struct esp_video *video, char *buffer, uint32_t size)
{
    int ret;

    ret = snprintf(buffer, size, "Simulation Camera:\n\tFormat: RGB565\n\tPixel: 1080P\n");
    if (ret <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static const struct esp_video_ops s_sim_video_ops = {
    .init          = sim_video_init,
    .deinit        = sim_video_deinit,
    .start_capture = sim_video_start_capture,
    .stop_capture  = sim_video_stop_capture,
    .set_format    = sim_video_set_format,
    .capability    = sim_video_capability,
    .description   = sim_video_description
};

esp_err_t sim_initialize_video_device(void)
{
    int ret;
    char *name;
    struct esp_video *video;
    struct sim_video *sim_video;

    for (int i = 0; i < SIM_CAMERA_COUNT; i++) {
        ret = asprintf(&name, "%s%d", SIM_CAMERA_DEVICE_NAME, i);
        assert(ret > 0);

        sim_video = heap_caps_malloc(sizeof(struct sim_video), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
        assert(sim_video);

        video = esp_video_create(name, &s_sim_video_ops, sim_video,
                                 SIM_CAMERA_BUFFER_COUNT, SIM_CAMERA_BUFFER_SIZE);
        assert(video);
        free(name);
    }

    return ESP_OK;
}
