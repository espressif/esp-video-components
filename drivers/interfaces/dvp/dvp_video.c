/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_video.h"
#include "esp_video_log.h"
#include "dvp.h"

#define DVP_DMA_BUFFER_SIZE         CONFIG_DVP_DMA_BUFFER_MAX_SIZE

struct dvp_video {
    dvp_device_handle_t handle;
};

static const char *TAG = "dvp_video";

static dvp_rx_cb_ret_t dvp_rx_cb(dvp_rx_ret_t rx_ret, uint8_t *buffer, size_t size, void *priv)
{
    struct esp_video *video = (struct esp_video *)priv;
    struct esp_video_buffer_info *info = &video->buf_info;

    ESP_LOGD(TAG, "rx_ret=%d size=%" PRIu32, rx_ret, (uint32_t)size);

    if (rx_ret == DVP_RX_OVERFLOW) {
        ESP_LOGD(TAG, "RX overflow expected<=%" PRIu32 " actual=%" PRIu32, info->size, (uint32_t)size);
        return DVP_RX_CB_DONE;
    }

#ifdef CONFIG_ESP_VIDEO_MEDIA_CONTROLLER
    esp_video_media_recvdone_buffer(video, buffer, size, 0);
#else
    esp_video_recvdone_buffer(video, buffer, size, 0);
#endif

    return DVP_RX_CB_CACHED;
}

static void dvp_free_buf_cb(uint8_t *buffer, void *priv)
{
    struct esp_video *video = (struct esp_video *)priv;

    esp_video_free_buffer(video, buffer);
}

static esp_err_t dvp_video_init(struct esp_video *video)
{
    esp_err_t ret;
    uint32_t frame_size;
    sensor_format_t sensor_format;
    struct esp_video_buffer_info *info = &video->buf_info;
    struct esp_video_format *format = &video->format;
    struct dvp_video *dvp_video = (struct dvp_video *)video->priv;

    ret = esp_camera_get_format(video->cam_dev, &sensor_format);
    if (ret != ESP_OK) {
        return ret;
    }

    format->fps = sensor_format.fps;
    format->width = sensor_format.width;
    format->height = sensor_format.height;
    format->pixel_format = sensor_format.format;
    format->pixel_bytes = sensor_format.bpp / 8;

    frame_size = format->width * format->height * format->pixel_bytes;

    ESP_LOGI(TAG, "DVP %s frame size=%" PRIu32, video->dev_name, frame_size);

    ret = dvp_setup_dma_receive_buffer(dvp_video->handle, frame_size);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = dvp_get_frame_buffer_info(dvp_video->handle, &info->size, &info->align_size, &info->caps);

    return ret;
}

static esp_err_t dvp_video_deinit(struct esp_video *video)
{
    return ESP_OK;
}

static esp_err_t dvp_video_start_capture(struct esp_video *video)
{
    esp_err_t ret;
    int flags = 1;
    uint8_t *buffer;
    struct esp_video_buffer_info *info = &video->buf_info;
    struct dvp_video *dvp_video = (struct dvp_video *)video->priv;

    for (int i = 0; i < info->count; i++) {
        buffer = esp_video_alloc_buffer(video);
        if (!buffer) {
            return ESP_ERR_NOT_FOUND;
        }

        ret = dvp_device_add_buffer(dvp_video->handle, buffer, info->size);
        if (ret != ESP_OK) {
            esp_video_free_buffer(video, buffer);
            return ret;
        }
    }

    ret = esp_camera_ioctl(video->cam_dev, CAM_SENSOR_IOC_S_STREAM, &flags);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = dvp_device_start(dvp_video->handle);
    if (ret != ESP_OK) {
        return ret;
    }

    return ESP_OK;
}

static esp_err_t dvp_video_stop_capture(struct esp_video *video)
{
    esp_err_t ret;
    int flags = 0;
    struct dvp_video *dvp_video = (struct dvp_video *)video->priv;

    ret = dvp_device_stop(dvp_video->handle);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_camera_ioctl(video->cam_dev, CAM_SENSOR_IOC_S_STREAM, &flags);
    if (ret != ESP_OK) {
        return ret;
    }

    return ESP_OK;
}

static esp_err_t dvp_video_set_format(struct esp_video *video, const struct esp_video_format *format)
{
    /**
     * Todo: AEG-1220
     */

    return ESP_OK;
}

static esp_err_t dvp_video_capability(struct esp_video *video, struct esp_video_capability *capability)
{
    if (!capability) {
        ESP_VIDEO_LOGE("capability=NULL");
        return ESP_ERR_INVALID_ARG;
    }

    memset(capability, 0, sizeof(struct esp_video_capability));

    return ESP_OK;
}

static esp_err_t dvp_video_description(struct esp_video *video, char *buffer, uint32_t size)
{
    int ret;

    ret = snprintf(buffer, size, "DVP Interface Camera\n");
    if (ret <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static void dvp_video_notify(struct esp_video *video, enum esp_video_event event, void *arg)
{
    if (event == ESP_VIDEO_BUFFER_VALID) {
        esp_err_t ret;
        uint8_t *buffer;
        struct esp_video_buffer_info *info = &video->buf_info;
        struct dvp_video *dvp_video = (struct dvp_video *)video->priv;

        buffer = esp_video_alloc_buffer(video);
        if (!buffer) {
            return;
        }

        ret = dvp_device_add_buffer(dvp_video->handle, buffer, info->size);
        if (ret != ESP_OK) {
            return;
        }
    }
}

static const struct esp_video_ops s_dvp_video_ops = {
    .init          = dvp_video_init,
    .deinit        = dvp_video_deinit,
    .start_capture = dvp_video_start_capture,
    .stop_capture  = dvp_video_stop_capture,
    .set_format    = dvp_video_set_format,
    .capability    = dvp_video_capability,
    .description   = dvp_video_description,
    .notify        = dvp_video_notify,
};

/**
 * @brief Create DVP video device
 *
 * @param cam_dev camera devcie
 * @param port    DVP port
 * @param pin     DVP pin configuration
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t dvp_create_camera_video_device(esp_camera_device_t *cam_dev, uint8_t port, const dvp_pin_config_t *pin)
{
    const char *name;
    esp_err_t ret;
    struct esp_video *video;
    struct dvp_video *dvp_video;
    dvp_device_interface_config_t dvp_cfg;

    name = esp_camera_get_name(cam_dev);
    if (!name) {
        return ESP_ERR_INVALID_ARG;
    }

    dvp_video = heap_caps_malloc(sizeof(struct dvp_video), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!dvp_video) {
        return ESP_ERR_NO_MEM;
    }

    video = esp_video_create(name, cam_dev, &s_dvp_video_ops, dvp_video);
    if (!video) {
        heap_caps_free(dvp_video);
        return ESP_FAIL;
    }

    dvp_cfg.port = port;
    dvp_cfg.rx_cb = dvp_rx_cb;
    dvp_cfg.priv = video;
    dvp_cfg.free_buf_cb = dvp_free_buf_cb;
    dvp_cfg.dma_buffer_max_size = DVP_DMA_BUFFER_SIZE;
    memcpy(&dvp_cfg.pin, pin, sizeof(dvp_pin_config_t));
    ret = dvp_device_create(&dvp_video->handle, &dvp_cfg);
    if (ret != ESP_OK) {
        esp_video_destroy(video);
        heap_caps_free(dvp_video);
        return ret;
    }

    return ESP_OK;
}
