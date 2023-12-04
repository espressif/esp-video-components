/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "esp_video.h"
#include "esp_video_log.h"

#define SIM_CAMERA_BUFFER_COUNT     CONFIG_SIMULATED_INTF_DEVICE_BUFFER_COUNT
#define SIM_CAMERA_BUFFER_SIZE      (sim_picture_jpeg_len)

extern unsigned int sim_picture_jpeg_len;

static const char *TAG = "sim_video";

static void IRAM_ATTR sim_video_rxcb(void *arg, const uint8_t *buffer, size_t n)
{
    uint8_t *video_buffer;
    struct esp_video *video = (struct esp_video *)arg;

    assert(n <= SIM_CAMERA_BUFFER_SIZE);

    video_buffer = esp_video_alloc_buffer(video);
    if (!video_buffer) {
        ESP_EARLY_LOGE(TAG, "Failed to allocte video buffer");
        return;
    }

    memcpy(video_buffer, buffer, n);
    esp_video_recvdone_buffer(video, video_buffer, n, 0);
}

static esp_err_t sim_video_init(struct esp_video *video)
{
    return ESP_OK;
}

static esp_err_t sim_video_deinit(struct esp_video *video)
{
    return ESP_OK;
}

static esp_err_t sim_video_start_capture(struct esp_video *video)
{
    esp_err_t ret;
    int flags = 1;
    struct sim_cam_rx param = {
        .cb   = sim_video_rxcb,
        .priv = video
    };

    ret = esp_camera_ioctl(video->cam_dev, CAM_SIM_S_RXCB, &param, NULL);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_camera_ioctl(video->cam_dev, CAM_SENSOR_S_STREAM, &flags, NULL);
    if (ret != ESP_OK) {
        return ret;
    }

    return ESP_OK;
}

static esp_err_t sim_video_stop_capture(struct esp_video *video)
{
    esp_err_t ret;
    int flags = 0;

    ret = esp_camera_ioctl(video->cam_dev, CAM_SENSOR_S_STREAM, &flags, NULL);
    if (ret != ESP_OK) {
        return ret;
    }

    return ESP_OK;
}

static esp_err_t sim_video_set_format(struct esp_video *video, const struct esp_video_format *format)
{
    esp_err_t ret;
    int fps = format->fps;

    ret = esp_camera_ioctl(video->cam_dev, CAM_SENSOR_S_FPS, &fps, NULL);
    if (ret != ESP_OK) {
        return ret;
    }

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

esp_err_t sim_create_camera_video_device(esp_camera_device_t *cam_dev)
{
    esp_err_t ret;
    char name[64];
    size_t n;
    struct esp_video *video;

    ret = esp_camera_ioctl(cam_dev, CAM_SENSOR_G_NAME, name, &n);
    if (ret != ESP_OK) {
        return ret;
    }

    video = esp_video_create(name, cam_dev, &s_sim_video_ops, SIM_CAMERA_BUFFER_COUNT,
                             SIM_CAMERA_BUFFER_SIZE, NULL);
    if (!video) {
        return ESP_FAIL;
    }

    return ESP_OK;
}
