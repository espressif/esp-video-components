/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_private/esp_cache_private.h"
#include "esp_cam_ctlr_spi.h"
#include "esp_video.h"
#include "esp_video_device_internal.h"
#include "esp_video_device_common.h"
#include "esp_video_cam.h"

#if CONFIG_SPIRAM
#define SPI_MEM_CAPS                    (MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM | MALLOC_CAP_CACHE_ALIGNED)
#else
#define SPI_MEM_CAPS                    (MALLOC_CAP_8BIT | MALLOC_CAP_DMA)
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x)                   (sizeof(x) / sizeof((x)[0]))
#endif

#if CONFIG_IDF_TARGET_ESP32P4
#define PARLIO_RX_CLK_MAX_HZ            (48 * 1000 * 1000)
#else
#define PARLIO_RX_CLK_MAX_HZ            (30 * 1000 * 1000)
#endif

struct spi_video {
    esp_video_device_common_t *common;      /* Must be first for esp_video_device_common access */
    esp_video_spi_device_config_t spi_config;
};

static const char *TAG = "spi_video";

static esp_err_t spi_start_init_config(esp_video_device_common_t *common, esp_video_device_common_init_data_t *config)
{
    // Note: No need to check common and config here, it will be checked in upper layer
    const esp_cam_sensor_spi_info_t *spi_info = &common->sensor_format->spi_info;

#ifdef CONFIG_CAM_CTLR_SPI_ENABLE_PARLIO
    struct spi_video *spi_video = (struct spi_video *)common->priv;

    if (spi_video->spi_config.intf == ESP_CAM_CTLR_SPI_CAM_INTF_PARLIO) {
        if (spi_info->pclk > PARLIO_RX_CLK_MAX_HZ) {
            ESP_LOGE(TAG, "sensor output data clock frequency %" PRIu32 " is too high, beyond the parlio maximum supported clock frequency %d", spi_info->pclk, PARLIO_RX_CLK_MAX_HZ);
            return ESP_FAIL;
        }
    }
#endif

    if (spi_info->frame_info) {
        config->sizeimage = spi_info->frame_info->frame_size;
    }

    return ESP_OK;
}

static esp_err_t spi_video_start(esp_video_device_common_t *common, esp_cam_ctlr_handle_t *cam_ctrl_handle_ret)
{
    const esp_cam_sensor_spi_info_t *spi_info = &common->sensor_format->spi_info;
    struct spi_video *spi_video = (struct spi_video *)common->priv;
    esp_video_spi_device_config_t *config = &spi_video->spi_config;

    esp_cam_ctlr_spi_config_t spi_config = {
        .intf = config->intf,
        .io_mode = config->io_mode,
        .spi_port = config->spi_port,
        .spi_cs_pin = config->spi_cs_pin,
        .spi_sclk_pin = config->spi_sclk_pin,
        .spi_data0_io_pin = config->spi_data0_io_pin,
        .spi_data1_io_pin = config->spi_data1_io_pin,
        .spi_data2_io_pin = config->spi_data2_io_pin,
        .spi_data3_io_pin = config->spi_data3_io_pin,
        .input_data_color_type = common->in_color,
        .h_res = CAPTURE_VIDEO_GET_FORMAT_WIDTH(common->video),
        .v_res = CAPTURE_VIDEO_GET_FORMAT_HEIGHT(common->video),
        .frame_info = spi_info->frame_info,
        .frame_buffer_count = CAPTURE_VIDEO_BUF_COUNT(common->video),
        .auto_decode_dis = 1,
    };
    ESP_RETURN_ON_ERROR(esp_cam_new_spi_ctlr(&spi_config, cam_ctrl_handle_ret), TAG, "failed to create SPI");

    return ESP_OK;
}

static esp_err_t spi_check_set_format(esp_video_device_common_t *common, const struct v4l2_format *format)
{
    const esp_cam_sensor_spi_info_t *spi_info = &common->sensor_format->spi_info;
    const struct v4l2_pix_format *pix = &format->fmt.pix;

    if (pix->pixelformat != CAPTURE_VIDEO_GET_FORMAT_PIXEL_FORMAT(common->video)) {
        ESP_LOGE(TAG, "format is not supported");
        return ESP_ERR_INVALID_ARG;
    }

    if (spi_info->frame_info) {
        if (pix->sizeimage > 0) {
            if (pix->sizeimage < spi_info->frame_info->frame_size) {
                ESP_LOGE(TAG, "sizeimage is less than the required size");
                return ESP_ERR_INVALID_ARG;
            } else {
                /**
                 * Update the sizeimage to the frame size
                 */
                ESP_RETURN_ON_ERROR(esp_video_config_buffer(common->video, format, SPI_MEM_CAPS), TAG, "failed to configure stream buffer");
            }
        }
    }

    return ESP_OK;
}

static esp_err_t spi_video_reprocess(esp_video_device_common_t *common, uint8_t *src, size_t src_size, uint8_t *dst, size_t dst_size, size_t *dst_out_size)
{
    return esp_cam_spi_decode_frame(common->cam_ctrl_handle, src, src_size, dst, dst_size, (uint32_t *)dst_out_size);
}

static const esp_video_device_intf_t s_spi_device_intf = {
    .start_init_config = spi_start_init_config,
    .start            = spi_video_start,
    .check_set_format = spi_check_set_format,
    .reprocess        = spi_video_reprocess,
};

/**
 * @brief Create SPI video device
 *
 * @param cam_dev       Camera sensor device
 * @param config        SPI video device configuration
 * @param index         SPI video device index
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_create_spi_video_device(esp_cam_sensor_device_t *cam_dev, const esp_video_spi_device_config_t *config, uint8_t index)
{
    esp_err_t ret;
    const char *name;
    uint8_t id;
    struct spi_video *spi_video;

    if (index == 0) {
        name = ESP_VIDEO_SPI_DEVICE_0_NAME;
        id = ESP_VIDEO_SPI_DEVICE_0_ID;
    } else if (index == 1) {
        name = ESP_VIDEO_SPI_DEVICE_1_NAME;
        id = ESP_VIDEO_SPI_DEVICE_1_ID;
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    spi_video = heap_caps_calloc(1, sizeof(struct spi_video), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!spi_video) {
        return ESP_ERR_NO_MEM;
    }
    spi_video->spi_config = *config;

    esp_video_device_common_config_t common_config = {
        .name = name,
        .id = id,
        .priv = spi_video,
        .intf = &s_spi_device_intf,
        .cam = {
            .sensor = cam_dev,
        },
        .mem_caps = SPI_MEM_CAPS,
        .use_backup_element = false,
    };
    ret = esp_video_device_common_create(&common_config, &spi_video->common);
    if (ret != ESP_OK) {
        heap_caps_free(spi_video);
        return ret;
    }

    return ESP_OK;
}

/**
 * @brief Destroy SPI video device
 *
 * @param index         SPI video device index
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_destroy_spi_video_device(uint8_t index)
{
    const char *name;
    struct spi_video *spi_video;

    if (index == 0) {
        name = ESP_VIDEO_SPI_DEVICE_0_NAME;
    } else if (index == 1) {
        name = ESP_VIDEO_SPI_DEVICE_1_NAME;
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(esp_video_device_common_get_priv(name, (void **)&spi_video), TAG, "failed to get private data");
    ESP_RETURN_ON_ERROR(esp_video_device_common_free(spi_video->common), TAG, "failed to free common video device");
    heap_caps_free(spi_video);

    return ESP_OK;
}
