/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "esp_video.h"
#include "mipi_csi.h"
#include "camera_isp.h"

struct csi_video {
    esp_mipi_csi_handle_t handle;
};

static const char *TAG = "csi_video";
static struct esp_video *g_video;

esp_err_t csi_video_recv_vb(uint8_t *buffer, uint32_t offset, uint32_t len)
{
    return ESP_OK;
}

uint8_t *IRAM_ATTR csi_video_get_new_vb(uint32_t len)
{
    return esp_video_alloc_buffer(g_video);
}

static esp_err_t csi_video_init(struct esp_video *video)
{
    return ESP_OK;
}

static esp_err_t csi_video_deinit(struct esp_video *video)
{
    return ESP_OK;
}

static esp_err_t csi_video_start_capture(struct esp_video *video)
{
    return ESP_OK;
}

static esp_err_t csi_video_stop_capture(struct esp_video *video)
{
    return ESP_OK;
}

static esp_err_t csi_video_set_format(struct esp_video *video, const struct esp_video_format *format)
{
    return ESP_OK;
}

static esp_err_t csi_video_capability(struct esp_video *video, struct esp_video_capability *capability)
{
    if (!capability) {
        ESP_LOGE(TAG, "capability=NULL");
        return ESP_ERR_INVALID_ARG;
    }

    memset(capability, 0, sizeof(struct esp_video_capability));
    capability->fmt_jpeg = 1;

    return ESP_OK;
}

static esp_err_t csi_video_description(struct esp_video *video, char *buffer, uint32_t size)
{
    int ret;

    ret = snprintf(buffer, size, "CSI Camera:\n\tFormat: RGB565\n\tPixel: 1080P\n");
    if (ret <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static const struct esp_video_ops s_csi_video_ops = {
    .init          = csi_video_init,
    .deinit        = csi_video_deinit,
    .start_capture = csi_video_start_capture,
    .stop_capture  = csi_video_stop_capture,
    .set_format    = csi_video_set_format,
    .capability    = csi_video_capability,
    .description   = csi_video_description
};

esp_err_t csi_create_camera_video_device(esp_camera_device_t *cam_dev, uint8_t port)
{
    const char *name;
    struct esp_video *video;
    struct csi_video *csi_video;

    name = esp_camera_get_name(cam_dev);
    if (!name) {
        return ESP_ERR_INVALID_ARG;
    }

    csi_video = heap_caps_malloc(sizeof(struct csi_video), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!csi_video) {
        return ESP_ERR_NO_MEM;
    }

    video = esp_video_create(name, cam_dev, &s_csi_video_ops, NULL);
    if (!video) {
        heap_caps_free(csi_video);
        return ESP_FAIL;
    }
    return ESP_OK;
}
