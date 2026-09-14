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
#include "esp_check.h"
#include "hal/color_types.h"
#include "esp_cam_ctlr.h"
#include "esp_cam_ctlr_dvp_ext.h"

#include "esp_video.h"
#include "esp_video_cam.h"
#include "esp_video_device_internal.h"
#include "esp_video_device_common.h"
#include "esp_video_dvp_format.h"
#include "esp_video_ioctl.h"
#include "esp_video_caps.h"
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

#if ESP_VIDEO_DVP_DEVICE_CONV_FORMAT
    bool out_valid;
    cam_ctlr_color_t out_color;
    color_range_t yuv_range;
    color_conv_std_rgb_yuv_t yuv_std;
#endif

#if CONFIG_ESP_VIDEO_ENABLE_SWAP_BYTE
    esp_video_swap_byte_t *swap_byte;
#endif
};

static const char *TAG = "dvp_video";

#if ESP_VIDEO_DVP_DEVICE_CONV_FORMAT
static esp_err_t dvp_start_init_config(esp_video_device_common_t *common, esp_video_device_common_init_data_t *config)
{
    struct dvp_video *dvp_video = (struct dvp_video *)common->priv;

    /*
     * update_format_config() / VIDIOC_S_SENSOR_FMT reconfigures capture to the
     * sensor native format. Drop any previously committed conversion state so
     * STREAMON cannot reuse a stale RGB565X conversion against a new input.
     */
    dvp_video->out_valid = false;
    (void)config;
    return ESP_OK;
}
#endif

static esp_err_t dvp_enum_format(esp_video_device_common_t *common, uint32_t index, uint32_t *pixel_format)
{
    return esp_video_dvp_enum_format(common->sensor_format->format, index, pixel_format);
}

static esp_err_t dvp_check_enum_framesizes(esp_video_device_common_t *common, struct v4l2_frmsizeenum *frmsize)
{
    esp_video_dvp_in_out_format_t in_out_format;

    if (esp_video_dvp_check_format(common->sensor_format->format, frmsize->pixel_format, &in_out_format) != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t dvp_check_set_format(esp_video_device_common_t *common, const struct v4l2_format *format)
{
    const struct v4l2_pix_format *pix = &format->fmt.pix;
    struct dvp_video *dvp_video = (struct dvp_video *)common->priv;
    esp_video_dvp_in_out_format_t in_out_format = {0};

    /* Reject zero FOURCC before committing any DVP private conversion state. */
    if (!pix->pixelformat) {
        ESP_LOGE(TAG, "pixel format is 0");
        return ESP_ERR_INVALID_ARG;
    }

    if (esp_video_dvp_check_format(common->sensor_format->format, pix->pixelformat, &in_out_format) == ESP_OK) {
#if ESP_VIDEO_DVP_DEVICE_CONV_FORMAT
        color_range_t yuv_range;
        color_conv_std_rgb_yuv_t yuv_std;

        if ((pix->quantization == V4L2_QUANTIZATION_DEFAULT) ||
                (pix->quantization == V4L2_QUANTIZATION_FULL_RANGE)) {
            yuv_range = COLOR_RANGE_FULL;
        } else if (pix->quantization == V4L2_QUANTIZATION_LIM_RANGE) {
            yuv_range = COLOR_RANGE_LIMIT;
        } else {
            ESP_LOGE(TAG, "unsupported color YUV color range");
            return ESP_ERR_NOT_SUPPORTED;
        }

        if ((pix->ycbcr_enc == V4L2_YCBCR_ENC_DEFAULT) ||
                (pix->ycbcr_enc == V4L2_YCBCR_ENC_601)) {
            yuv_std = COLOR_CONV_STD_RGB_YUV_BT601;
        } else if (pix->ycbcr_enc == V4L2_YCBCR_ENC_709) {
            yuv_std = COLOR_CONV_STD_RGB_YUV_BT709;
        } else {
            ESP_LOGE(TAG, "unsupported color YUV conversion standard");
            return ESP_ERR_NOT_SUPPORTED;
        }

        /* Configure the committed capture buffer first, then publish conversion state. */
        ESP_RETURN_ON_ERROR(esp_video_config_buffer(common->video, format, common->mem_caps), TAG, "failed to configure stream buffer");
        dvp_video->out_valid = true;
        dvp_video->out_color = in_out_format.out_color;
        dvp_video->yuv_range = yuv_range;
        dvp_video->yuv_std = yuv_std;
#else
        (void)dvp_video;
#endif
        return ESP_OK;
    }

#if CONFIG_ESP_VIDEO_ENABLE_SWAP_BYTE
    if (common->in_color == CAM_CTLR_COLOR_RGB565) {
        if (pix->pixelformat == V4L2_PIX_FMT_RGB565 || pix->pixelformat == V4L2_PIX_FMT_RGB565X) {
            return ESP_OK;
        }
    }
#endif

    ESP_LOGE(TAG, "format=" V4L2_FMT_STR " is not supported", V4L2_FMT_STR_ARG(pix->pixelformat));
    return ESP_ERR_INVALID_ARG;
}

#if CONFIG_ESP_VIDEO_ENABLE_SWAP_BYTE
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
    struct dvp_video *dvp_video = (struct dvp_video *)common->priv;
#if ESP_VIDEO_DVP_DEVICE_CONV_FORMAT || ESP_VIDEO_DVP_DEVICE_OUTPUT_COLOR
#if ESP_VIDEO_DVP_DEVICE_CONV_FORMAT
    cam_ctlr_color_t out_color = common->in_color;
    uint32_t capture_fmt = CAPTURE_VIDEO_GET_FORMAT_PIXEL_FORMAT(common->video);
    esp_video_dvp_in_out_format_t capture_in_out = {0};

    /*
     * Derive output color from the currently committed capture FOURCC so
     * conversion state stays atomic with G_FMT / S_SENSOR_FMT updates.
     */
    if (capture_fmt &&
            esp_video_dvp_check_format(common->sensor_format->format, capture_fmt, &capture_in_out) == ESP_OK) {
        out_color = capture_in_out.out_color;
        if (out_color == common->in_color) {
            dvp_video->out_valid = false;
        }
    } else {
        dvp_video->out_valid = false;
    }
#else
    cam_ctlr_color_t out_color = common->in_color;
#endif
#endif
#if !CONFIG_ESP_VIDEO_ENABLE_SWAP_BYTE && !ESP_VIDEO_DVP_DEVICE_CONV_FORMAT
    (void)dvp_video;
#endif

#if CONFIG_ESP_VIDEO_ENABLE_SWAP_BYTE
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
        .output_data_color_type = out_color,
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

#if ESP_VIDEO_DVP_DEVICE_CONV_FORMAT
    if (out_color != common->in_color) {
        cam_ctlr_format_conv_config_t conv_config = {
            .src_format = common->in_color,
            .dst_format = out_color,
            .conv_std = dvp_video->out_valid ? dvp_video->yuv_std : COLOR_CONV_STD_RGB_YUV_BT601,
            .data_width = 8,
            .input_range = COLOR_RANGE_FULL,
            .output_range = dvp_video->out_valid ? dvp_video->yuv_range : COLOR_RANGE_FULL,
        };

        ret = esp_cam_ctlr_format_conversion(cam_ctrl_handle, &conv_config);
        if (ret != ESP_OK) {
#if CONFIG_ESP_VIDEO_ENABLE_SWAP_BYTE
            if (dvp_video->swap_byte) {
                esp_video_swap_byte_free(dvp_video->swap_byte);
                dvp_video->swap_byte = NULL;
            }
#endif
            esp_cam_ctlr_del(cam_ctrl_handle);
            ESP_LOGE(TAG, "failed to set format conversion");
            return ret;
        }
    }
#endif

    *cam_ctrl_handle_ret = cam_ctrl_handle;

    return ESP_OK;
}

static const esp_video_device_intf_t s_dvp_device_intf = {
#if ESP_VIDEO_DVP_DEVICE_CONV_FORMAT
    .start_init_config = dvp_start_init_config,
#endif
    .start             = dvp_video_start,
    .enum_format       = dvp_enum_format,
    .check_set_format  = dvp_check_set_format,
    .check_enum_framesizes = dvp_check_enum_framesizes,
#if CONFIG_ESP_VIDEO_ENABLE_SWAP_BYTE
    .stop              = dvp_video_stop,
    .reprocess         = dvp_video_reprocess,
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
