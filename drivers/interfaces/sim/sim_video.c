/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "esp_video.h"
#include "esp_video_log.h"

#define SIM_NAME                    "SIM"
#define SIM_CAMERA_BUFFER_COUNT     CONFIG_SIMULATED_INTF_DEVICE_BUFFER_COUNT
#define SIM_CAMERA_BUFFER_SIZE      (sim_picture_jpeg_len)

extern unsigned int sim_picture_jpeg_len;

static int g_sim_count;
static const char *TAG = "sim_video";

static void IRAM_ATTR sim_video_rxcb(void *arg, const uint8_t *buffer, size_t n)
{
    uint8_t *video_buffer;
    struct esp_video *video = (struct esp_video *)arg;

    assert(n <= SIM_CAMERA_BUFFER_SIZE);

    video_buffer = CAPTURE_VIDEO_GET_QUEUED_BUF(video);
    if (!video_buffer) {
        ESP_EARLY_LOGE(TAG, "Failed to allocte video buffer");
        return;
    }

    memcpy(video_buffer, buffer, n);

#ifdef CONFIG_ESP_VIDEO_MEDIA_CONTROLLER
    esp_video_media_recvdone_buffer(video, video_buffer, n, 0);
#else
    CAPTURE_VIDEO_DONE_BUF(video, video_buffer, n);
#endif
}

static esp_err_t sim_video_init(struct esp_video *video)
{
    CAPTURE_VIDEO_SET_BUF_INFO(video, SIM_CAMERA_BUFFER_SIZE, 1, MALLOC_CAP_8BIT);

    return ESP_OK;
}

static esp_err_t sim_video_deinit(struct esp_video *video)
{
    return ESP_OK;
}

static esp_err_t sim_video_start(struct esp_video *video, uint32_t type)
{
    esp_err_t ret;
    int flags = 1;
    struct sim_cam_rx param = {
        .cb   = sim_video_rxcb,
        .priv = video
    };
    esp_camera_device_t *cam_dev = VIDEO_CAM_DEV(video);

    ret = esp_camera_ioctl(cam_dev, CAM_SIM_IOC_S_RXCB, &param);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_camera_ioctl(cam_dev, CAM_SENSOR_IOC_S_STREAM, &flags);
    if (ret != ESP_OK) {
        return ret;
    }

    return ESP_OK;
}

static esp_err_t sim_video_stop(struct esp_video *video, uint32_t type)
{
    esp_err_t ret;
    int flags = 0;
    esp_camera_device_t *cam_dev = VIDEO_CAM_DEV(video);

    ret = esp_camera_ioctl(cam_dev, CAM_SENSOR_IOC_S_STREAM, &flags);
    if (ret != ESP_OK) {
        return ret;
    }

    return ESP_OK;
}

static esp_err_t sim_video_set_format(struct esp_video *video, uint32_t type, const struct esp_video_format *format)
{
    esp_err_t ret;
    struct v4l2_ext_control ctrl = {
        .id = CAM_SENSOR_FPS,
        .value = format->fps
    };
    esp_camera_device_t *cam_dev = VIDEO_CAM_DEV(video);

    ret = esp_camera_set_para_value(cam_dev, &ctrl);
    if (ret != ESP_OK) {
        return ret;
    }

    CAPTURE_VIDEO_SET_BUF_INFO(video, SIM_CAMERA_BUFFER_SIZE, 1, MALLOC_CAP_8BIT);

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
    .start         = sim_video_start,
    .stop          = sim_video_stop,
    .set_format    = sim_video_set_format,
    .capability    = sim_video_capability,
    .description   = sim_video_description
};

esp_err_t sim_create_camera_video_device(esp_camera_device_t *cam_dev)
{
    int ret;
    char name[8];
    struct esp_video *video;
    uint32_t device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_READWRITE | V4L2_CAP_EXT_PIX_FORMAT |
                           V4L2_CAP_STREAMING;
    uint32_t caps = device_caps | V4L2_CAP_DEVICE_CAPS;

    ret = snprintf(name, sizeof(name), SIM_NAME "%d", g_sim_count);
    if (ret <= 0) {
        return ESP_ERR_NO_MEM;
    }

    video = esp_video_create(name, cam_dev, &s_sim_video_ops, NULL, caps, device_caps);
    if (!video) {
        return ESP_FAIL;
    }

    g_sim_count++;

    return ESP_OK;
}
