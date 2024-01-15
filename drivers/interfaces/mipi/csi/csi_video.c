/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "mipi_csi.h"
#include "camera_isp.h"
#include "esp_video.h"
#include "esp_color_formats.h"

#define CSI_INIT_FORMAT PIXFORMAT_RGB565

struct csi_video {
    esp_mipi_csi_handle_t handle;
};

static const char *TAG = "csi_video";

static esp_err_t csi_video_recv_vb(uint8_t *buffer, uint32_t offset, uint32_t size, void *priv)
{
    struct esp_video *video = (struct esp_video *)priv;

    ESP_LOGD(TAG, "size=%" PRIu32, size);

#ifdef CONFIG_ESP_VIDEO_MEDIA_CONTROLLER
    esp_video_media_recvdone_buffer(video, buffer, size, 0);
#else
    esp_video_recvdone_buffer(video, buffer, size, 0);
#endif

    return ESP_OK;
}

static uint8_t *IRAM_ATTR csi_video_get_new_vb(uint32_t len, void *priv)
{
    struct esp_video *video = (struct esp_video *)priv;

    return esp_video_alloc_buffer(video);
}

static esp_err_t csi_video_init(struct esp_video *video)
{
    esp_err_t ret;
    uint32_t frame_size;
    esp_mipi_csi_ops_t csi_ops;
    sensor_format_t sensor_format;
    mipi_csi_port_config_t csi_config;
    struct esp_video_format *format = &video->format;
    struct esp_video_buffer_info *info = &video->buf_info;
    struct csi_video *csi_video = (struct csi_video *)video->priv;

    ret = esp_camera_get_format(video->cam_dev, &sensor_format);
    if (ret != ESP_OK) {
        return ret;
    }

    format->fps = sensor_format.fps;
    format->width = sensor_format.width;
    format->height = sensor_format.height;
    format->pixel_format = CSI_INIT_FORMAT;
    format->bpp = sensor_format.bpp;

    frame_size = format->width * format->height * esp_video_get_bpp_by_format(format->pixel_format) / 8;

    ESP_LOGI(TAG, "DVP %s frame size=%" PRIu32, video->dev_name, frame_size);

    csi_config.frame_width = sensor_format.width;
    csi_config.frame_height = sensor_format.height;
    csi_config.out_format = CSI_INIT_FORMAT;
    csi_config.isp_enable = sensor_format.isp_info ? true : false;
    csi_config.mipi_clk_freq_hz = sensor_format.mipi_info.mipi_clk;
    if (sensor_format.format == CAM_SENSOR_PIXFORMAT_RAW10) {
        csi_config.in_format = PIXFORMAT_RAW10;
    } else if (sensor_format.format == CAM_SENSOR_PIXFORMAT_RAW8) {
        csi_config.in_format = PIXFORMAT_RAW8;
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    if (sensor_format.port == MIPI_CSI_OUTPUT_LANE1) {
        csi_config.lane_num = 1;
    } else if (sensor_format.port == MIPI_CSI_OUTPUT_LANE2) {
        csi_config.lane_num = 2;
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    ret = esp_mipi_csi_driver_install(MIPI_CSI_PORT0, &csi_config, 0, &csi_video->handle);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = isp_init(csi_config.frame_width, csi_config.frame_height, csi_config.in_format, csi_config.out_format, csi_config.isp_enable, NULL);
    if (ret != ESP_OK) {
        return ret;
    }

    csi_ops.priv = video;
    csi_ops.alloc_buffer = csi_video_get_new_vb;
    csi_ops.recved_data = csi_video_recv_vb;

    ret = esp_mipi_csi_ops_regist(csi_video->handle, &csi_ops);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_mipi_csi_get_fb_info(csi_video->handle, &info->size, &info->align_size, &info->caps);

    return ret;
}

static esp_err_t csi_video_deinit(struct esp_video *video)
{
    esp_err_t ret;
    struct csi_video *csi_video = (struct csi_video *)video->priv;

    ret = esp_mipi_csi_driver_delete(csi_video->handle);
    if (ret == ESP_OK) {
        csi_video->handle = NULL;
    }

    return ret;
}

static esp_err_t csi_video_start_capture(struct esp_video *video)
{
    esp_err_t ret;
    int flags = 1;
    struct csi_video *csi_video = (struct csi_video *)video->priv;

    ret = esp_mipi_csi_start(csi_video->handle);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_camera_ioctl(video->cam_dev, CAM_SENSOR_IOC_S_STREAM, &flags);

    return ret;
}

static esp_err_t csi_video_stop_capture(struct esp_video *video)
{
    esp_err_t ret;
    int flags = 0;
    struct csi_video *csi_video = (struct csi_video *)video->priv;

    ret = esp_camera_ioctl(video->cam_dev, CAM_SENSOR_IOC_S_STREAM, &flags);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_mipi_csi_stop(csi_video->handle);

    return ret;
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

    return ESP_OK;
}

static esp_err_t csi_video_description(struct esp_video *video, char *buffer, uint32_t size)
{
    int ret;

    ret = snprintf(buffer, size, "MIPI-CSI Interface Camera\n");
    if (ret <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static void csi_video_notify(struct esp_video *video, enum esp_video_event event, void *arg)
{
    esp_err_t ret;

    if (event == ESP_VIDEO_BUFFER_VALID) {
        struct csi_video *csi_video = (struct csi_video *)video->priv;

        ret = esp_mipi_csi_new_buffer_available(csi_video->handle);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "failed to call esp_mipi_csi_new_buffer_available");
        }
    }
}

static const struct esp_video_ops s_csi_video_ops = {
    .init          = csi_video_init,
    .deinit        = csi_video_deinit,
    .start_capture = csi_video_start_capture,
    .stop_capture  = csi_video_stop_capture,
    .set_format    = csi_video_set_format,
    .capability    = csi_video_capability,
    .description   = csi_video_description,
    .notify        = csi_video_notify,
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

    video = esp_video_create(name, cam_dev, &s_csi_video_ops, csi_video);
    if (!video) {
        heap_caps_free(csi_video);
        return ESP_FAIL;
    }

    return ESP_OK;
}
