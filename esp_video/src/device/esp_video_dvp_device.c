/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_cam_ctlr_dvp_ext.h"

#include "esp_video.h"
#include "esp_video_cam.h"
#include "esp_video_device_internal.h"
#include "esp_video_device_common.h"
#include "esp_video_ioctl.h"
#if CONFIG_ESP_VIDEO_ENABLE_SWAP_BYTE
#include "esp_video_swap_byte.h"
#endif

#define DVP_CTLR_ID                 0

#if CONFIG_SPIRAM
#define DVP_MEM_CAPS                (MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM | MALLOC_CAP_CACHE_ALIGNED)
#else
#define DVP_MEM_CAPS                (MALLOC_CAP_8BIT | MALLOC_CAP_DMA)
#endif

/**
 * @brief IDF version v5.5.1 and later versions support external xtal for DVP
 *        IDF version v5.4.x(x>=3) supports external xtal for DVP
 */
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 1)) || \
    ((ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 3)) && (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 5, 0)))
#define DVP_DRIVER_HAS_EXTERNAL_XTAL    1
#else
#define DVP_DRIVER_HAS_EXTERNAL_XTAL    0
#endif

struct dvp_video {
    esp_video_device_common_t *common; /* Must be first for esp_video_device_common access */

#if CONFIG_ESP_VIDEO_ENABLE_SWAP_BYTE
    esp_video_swap_byte_t *swap_byte;
#endif
};

static const char *TAG = "dvp_video";

#if CONFIG_ESP_VIDEO_ENABLE_SWAP_BYTE
static esp_err_t dvp_check_set_format(esp_video_device_common_t *common, const struct v4l2_format *format)
{
    const struct v4l2_pix_format *pix = &format->fmt.pix;

    if (common->in_color == CAM_CTLR_COLOR_RGB565) {
        if (pix->pixelformat != V4L2_PIX_FMT_RGB565 && pix->pixelformat != V4L2_PIX_FMT_RGB565X) {
            ESP_LOGE(TAG, "format=" V4L2_FMT_STR " is not supported", V4L2_FMT_STR_ARG(pix->pixelformat));
            return ESP_ERR_INVALID_ARG;
        }
    }

    return ESP_OK;
}

static void IRAM_ATTR dvp_prepare_on_get_new_trans(esp_video_device_common_t *common)
{
    struct dvp_video *dvp_video = (struct dvp_video *)common->priv;

    if (dvp_video->swap_byte) {
        esp_video_swap_byte_start(dvp_video->swap_byte);
    }
}

static esp_err_t dvp_video_reprocess(esp_video_device_common_t *common, uint8_t *src, size_t src_size, uint8_t *dst, size_t dst_size, size_t *dst_out_size)
{
    esp_err_t ret = ESP_OK;
    struct dvp_video *dvp_video = (struct dvp_video *)common->priv;

    if (dvp_video->swap_byte) {
        ret = esp_video_swap_byte_process(dvp_video->swap_byte, src, src_size, dst, dst_size, dst_out_size);
    } else {
        *dst_out_size = src_size;
    }

    return ret;
}

static esp_err_t dvp_video_stop(esp_video_device_common_t *common)
{
    struct dvp_video *dvp_video = (struct dvp_video *)common->priv;

    if (dvp_video->swap_byte) {
        esp_video_swap_byte_free(dvp_video->swap_byte);
        dvp_video->swap_byte = NULL;
    }

    return ESP_OK;
}
#endif

static esp_err_t dvp_video_start(esp_video_device_common_t *common, esp_cam_ctlr_handle_t *cam_ctrl_handle_ret)
{
    assert(common);
    assert(cam_ctrl_handle_ret);

    esp_err_t ret;
    esp_cam_ctlr_handle_t cam_ctrl_handle = NULL;

#if CONFIG_ESP_VIDEO_ENABLE_SWAP_BYTE
    struct dvp_video *dvp_video = (struct dvp_video *)common->priv;
    bool need_swap_byte = false;

    if (common->in_color == CAM_CTLR_COLOR_RGB565) {
        uint32_t v4l2_format = CAPTURE_VIDEO_GET_FORMAT_PIXEL_FORMAT(common->video);
        uint32_t sensor_pf = common->sensor_format->format;

        if ((sensor_pf == ESP_CAM_SENSOR_PIXFORMAT_RGB565_BE && v4l2_format == V4L2_PIX_FMT_RGB565) ||
                (sensor_pf == ESP_CAM_SENSOR_PIXFORMAT_RGB565_LE && v4l2_format == V4L2_PIX_FMT_RGB565X)) {
            need_swap_byte = true;
        }
    }

    if (need_swap_byte) {
        dvp_video->swap_byte = esp_video_swap_byte_create();
        ESP_RETURN_ON_FALSE(dvp_video->swap_byte, ESP_FAIL, TAG, "failed to create swap byte");
        ret = esp_video_swap_byte_start(dvp_video->swap_byte);
        if (ret != ESP_OK) {
            esp_video_swap_byte_free(dvp_video->swap_byte);
            dvp_video->swap_byte = NULL;
            ESP_LOGE(TAG, "Failed to start swap byte: %d", ret);
            return ret;
        }

        ESP_LOGI(TAG, "swap byte enabled");
    } else {
        dvp_video->swap_byte = NULL;
        ESP_LOGI(TAG, "swap byte disabled");
    }
#endif

    esp_cam_ctlr_dvp_config_t dvp_config = {
        .ctlr_id = DVP_CTLR_ID,
        .clk_src = CAM_CLK_SRC_DEFAULT,
        .h_res = CAPTURE_VIDEO_GET_FORMAT_WIDTH(common->video),
        .v_res = CAPTURE_VIDEO_GET_FORMAT_HEIGHT(common->video),
        .dma_burst_size = common->buf_alignment,
        .input_data_color_type = common->in_color,
#if ESP_VIDEO_DVP_DEVICE_OUTPUT_COLOR
        .output_data_color_type = common->in_color,
#endif
        .pin_dont_init = true,
        .pic_format_jpeg = CAPTURE_VIDEO_GET_FORMAT_PIXEL_FORMAT(common->video) == V4L2_PIX_FMT_JPEG,
#if DVP_DRIVER_HAS_EXTERNAL_XTAL
        .external_xtal = true,
#endif
    };
    ret = esp_cam_new_dvp_ctlr_ext(&dvp_config, &cam_ctrl_handle);
    if (ret != ESP_OK) {
#if CONFIG_ESP_VIDEO_ENABLE_SWAP_BYTE
        if (dvp_video->swap_byte) {
            esp_video_swap_byte_free(dvp_video->swap_byte);
            dvp_video->swap_byte = NULL;
        }
#endif
        ESP_LOGE(TAG, "failed to create DVP: %d", ret);
        return ret;
    }

    *cam_ctrl_handle_ret = cam_ctrl_handle;

    return ESP_OK;
}

static const esp_video_device_intf_t s_dvp_device_intf = {
    .start             = dvp_video_start,
#if CONFIG_ESP_VIDEO_ENABLE_SWAP_BYTE
    .stop              = dvp_video_stop,
    .reprocess         = dvp_video_reprocess,
    .check_set_format  = dvp_check_set_format,
    .prepare_on_get_new_trans = dvp_prepare_on_get_new_trans,
#endif
};

/**
 * @brief Create DVP video device
 *
 * @param sensor camera sensor device
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_create_dvp_video_device(esp_cam_sensor_device_t *sensor)
{
    esp_err_t ret;
    struct dvp_video *dvp_video;

    dvp_video = heap_caps_calloc(1, sizeof(struct dvp_video), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!dvp_video) {
        return ESP_ERR_NO_MEM;
    }

    esp_video_device_common_config_t common_config = {
        .name = DVP_NAME,
        .id = ESP_VIDEO_DVP_DEVICE_ID,
        .priv = dvp_video,
        .intf = &s_dvp_device_intf,
        .cam = {
            .sensor = sensor,
        },
        .mem_caps = DVP_MEM_CAPS,
        .use_backup_element = false,
    };

    ret = esp_video_device_common_create(&common_config, &dvp_video->common);
    if (ret != ESP_OK) {
        heap_caps_free(dvp_video);
        return ret;
    }

    return ESP_OK;
}

/**
 * @brief Destroy DVP video device
 *
 * @param None
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_destroy_dvp_video_device(void)
{
    struct dvp_video *dvp_video;

    ESP_RETURN_ON_ERROR(esp_video_device_common_get_priv(DVP_NAME, (void **)&dvp_video), TAG, "failed to get private data");
    ESP_RETURN_ON_ERROR(esp_video_device_common_free(dvp_video->common), TAG, "failed to free common video device");
    heap_caps_free(dvp_video);

    return ESP_OK;
}
