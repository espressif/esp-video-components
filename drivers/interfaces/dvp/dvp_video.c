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

#define DVP_NAME                    "DVP"
#define DVP_DMA_BUFFER_SIZE         CONFIG_DVP_DMA_BUFFER_MAX_SIZE

struct dvp_video {
    dvp_device_handle_t handle;
};

static const char *TAG = "dvp_video";

static dvp_rx_cb_ret_t dvp_rx_cb(dvp_rx_ret_t rx_ret, uint8_t *buffer, size_t size, void *priv)
{
    struct esp_video *video = (struct esp_video *)priv;

    ESP_LOGD(TAG, "rx_ret=%d size=%zu", rx_ret, size);

    if (rx_ret == DVP_RX_OVERFLOW) {
        ESP_LOGD(TAG, "RX overflow expected<=%" PRIu32 " actual=%zu", CAPTURE_VIDEO_BUF_SIZE(video), size);
        return DVP_RX_CB_DONE;
    } else if (rx_ret == DVP_RX_DATALOST) {
        ESP_LOGD(TAG, "RX data partially lost");
        return DVP_RX_CB_DONE;
    }

#ifdef CONFIG_ESP_VIDEO_MEDIA_CONTROLLER
    esp_video_media_recvdone_buffer(video, buffer, size, 0);
#else
    CAPTURE_VIDEO_DONE_BUF(video, buffer, size);
#endif

    return DVP_RX_CB_CACHED;
}

static void dvp_free_buf_cb(uint8_t *buffer, void *priv)
{
}

static esp_err_t dvp_video_init(struct esp_video *video)
{
    esp_err_t ret;
    uint32_t buf_size;
    uint32_t align_size;
    uint32_t buf_caps;
    uint32_t frame_size;
    sensor_format_t sensor_format;
    struct dvp_video *dvp_video = VIDEO_PRIV_DATA(struct dvp_video *, video);
    esp_camera_device_t *cam_dev = VIDEO_CAM_DEV(video);

    ret = esp_camera_get_format(cam_dev, &sensor_format);
    if (ret != ESP_OK) {
        return ret;
    }

    CAPTURE_VIDEO_SET_FORMAT(video,
                             sensor_format.fps,
                             sensor_format.width,
                             sensor_format.height,
                             sensor_format.format,
                             sensor_format.bpp);

    frame_size = sensor_format.width * sensor_format.height * sensor_format.bpp / 8;

    ESP_LOGI(TAG, "DVP %s frame size=%" PRIu32, video->dev_name, frame_size);

    ret = dvp_setup_dma_receive_buffer(dvp_video->handle, frame_size, sensor_format.format == V4L2_PIX_FMT_JPEG);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = dvp_get_frame_buffer_info(dvp_video->handle, &buf_size, &align_size, &buf_caps);
    if (ret == ESP_OK) {
        CAPTURE_VIDEO_SET_BUF_INFO(video, buf_size, align_size, buf_caps);
    }

    return ret;
}

static esp_err_t dvp_video_deinit(struct esp_video *video)
{
    return ESP_OK;
}

static esp_err_t dvp_video_start(struct esp_video *video, uint32_t type)
{
    esp_err_t ret;
    int flags = 1;
    esp_camera_device_t *cam_dev = VIDEO_CAM_DEV(video);
    struct dvp_video *dvp_video = VIDEO_PRIV_DATA(struct dvp_video *, video);

    ret = esp_camera_ioctl(cam_dev, CAM_SENSOR_IOC_S_STREAM, &flags);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = dvp_device_start(dvp_video->handle);
    if (ret != ESP_OK) {
        return ret;
    }

    return ESP_OK;
}

static esp_err_t dvp_video_stop(struct esp_video *video, uint32_t type)
{
    esp_err_t ret;
    int flags = 0;
    esp_camera_device_t *cam_dev = VIDEO_CAM_DEV(video);
    struct dvp_video *dvp_video = VIDEO_PRIV_DATA(struct dvp_video *, video);

    ret = esp_camera_ioctl(cam_dev, CAM_SENSOR_IOC_S_STREAM, &flags);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = dvp_device_stop(dvp_video->handle);
    if (ret != ESP_OK) {
        return ret;
    }

    return ESP_OK;
}

static esp_err_t dvp_video_set_format(struct esp_video *video, uint32_t type, const struct esp_video_format *format)
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

static esp_err_t dvp_video_notify(struct esp_video *video, enum esp_video_event event, void *arg)
{
    if (event == ESP_VIDEO_BUFFER_VALID) {
        esp_err_t ret;
        uint8_t *buffer;
        struct dvp_video *dvp_video = VIDEO_PRIV_DATA(struct dvp_video *, video);

        buffer = CAPTURE_VIDEO_GET_QUEUED_BUF(video);
        if (!buffer) {
            return ESP_FAIL;
        }

        ret = dvp_device_add_buffer(dvp_video->handle, buffer, CAPTURE_VIDEO_BUF_SIZE(video));
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return ESP_OK;
}

static const struct esp_video_ops s_dvp_video_ops = {
    .init          = dvp_video_init,
    .deinit        = dvp_video_deinit,
    .start         = dvp_video_start,
    .stop          = dvp_video_stop,
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
    esp_err_t ret;
    struct esp_video *video;
    struct dvp_video *dvp_video;
    dvp_device_interface_config_t dvp_cfg;
    uint32_t device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_READWRITE | V4L2_CAP_EXT_PIX_FORMAT |
                           V4L2_CAP_STREAMING;
    uint32_t caps = device_caps | V4L2_CAP_DEVICE_CAPS;

    dvp_video = heap_caps_malloc(sizeof(struct dvp_video), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!dvp_video) {
        return ESP_ERR_NO_MEM;
    }

    video = esp_video_create(DVP_NAME, cam_dev, &s_dvp_video_ops, dvp_video, caps, device_caps);
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
