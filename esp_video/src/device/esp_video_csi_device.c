/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "hal/isp_ll.h"
#include "esp_ldo_regulator.h"
#include "esp_cam_ctlr.h"
#include "esp_cam_ctlr_csi.h"

#include "esp_video.h"
#include "esp_video_cam.h"
#include "esp_video_ioctl.h"
#include "esp_video_device_internal.h"
#include "esp_video_device_common.h"
#include "esp_video_csi_format.h"
#if CONFIG_ESP_VIDEO_ENABLE_SWAP_SHORT && !ESP_VIDEO_CSI_DEVICE_SWAP_SHORT
#include "esp_video_swap_short.h"

#define ESP_VIDEO_CSI_DEVICE_SW_SWAP_SHORT 1 /* Software swap short */
#endif

#define CSI_LDO_UNIT_ID             3
#define CSI_LDO_CFG_VOL_MV          2500

#if CONFIG_SPIRAM
#define CSI_MEM_CAPS                (MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM | MALLOC_CAP_CACHE_ALIGNED)
#else
#define CSI_MEM_CAPS                (MALLOC_CAP_8BIT | MALLOC_CAP_DMA)
#endif

#define CSI_CTRL_ID                 0 // ESP32-P4 only supports one CSI controller, so the controller ID is fixed to 0
#define CSI_QUEUE_ITEMS             1 // CSI queue items is fixed to 1 to avoid buffer allocation and deallocation in the video device

#define V4L2_DEFAULT_OUT_COLOR      V4L2_PIX_FMT_RGB565 // Default output color is RGB565 for ESP32-P4 when input is RAW8

#define ISP_CROP_MIN_WIDTH          32
#define ISP_CROP_MIN_HEIGHT         32

struct csi_video {
    esp_video_device_common_t *common; /* Must be first for esp_video_device_common access */

    esp_video_csi_isp_in_out_format_t in_out_format;
    color_raw_element_order_t bayer_order;
    isp_color_range_t yuv_range;
    isp_yuv_conv_std_t yuv_std;

    esp_ldo_channel_handle_t ldo_handle;

    isp_proc_handle_t isp_proc;

#if ESP_VIDEO_CSI_DEVICE_SW_SWAP_SHORT
    esp_video_swap_short_t *swap_short;
#endif

    uint32_t dont_init_ldo      : 1;

#if ESP_VIDEO_ISP_DEVICE_CROP
    uint32_t set_crop           : 1;
#endif
};

static const char *TAG = "csi_video";

static esp_err_t csi_get_input_bayer_order(const esp_cam_sensor_isp_info_t *isp_info, color_raw_element_order_t *bayer_order)
{
    esp_err_t ret = ESP_OK;

    if (!isp_info) {
        *bayer_order = COLOR_RAW_ELEMENT_ORDER_BGGR;
    } else {
        switch (isp_info->isp_v1_info.bayer_type) {
        case ESP_CAM_SENSOR_BAYER_RGGB:
            *bayer_order = COLOR_RAW_ELEMENT_ORDER_RGGB;
            break;
        case ESP_CAM_SENSOR_BAYER_GRBG:
            *bayer_order = COLOR_RAW_ELEMENT_ORDER_GRBG;
            break;
        case ESP_CAM_SENSOR_BAYER_GBRG:
            *bayer_order = COLOR_RAW_ELEMENT_ORDER_GBRG;
            break;
        case ESP_CAM_SENSOR_BAYER_BGGR:
            *bayer_order = COLOR_RAW_ELEMENT_ORDER_BGGR;
            break;
        default:
            ret = ESP_ERR_NOT_SUPPORTED;
            break;
        }
    }

    return ret;
}

static esp_err_t csi_start_init_config(esp_video_device_common_t *common, esp_video_device_common_init_data_t *config)
{
    const esp_cam_sensor_format_t *sensor_fmt = common->sensor_format;
    struct csi_video *csi_video = (struct csi_video *)common->priv;
    uint32_t csi_output_v4l2_fmt = 0;

    if (sensor_fmt->isp_info) {
        csi_output_v4l2_fmt = V4L2_DEFAULT_OUT_COLOR;
    }

    ESP_RETURN_ON_ERROR(esp_video_csi_check_format(sensor_fmt->format, csi_output_v4l2_fmt, &csi_video->in_out_format), TAG, "failed to check CSI format");
    ESP_RETURN_ON_ERROR(csi_get_input_bayer_order(sensor_fmt->isp_info, &csi_video->bayer_order), TAG, "failed to get bayer order");

    csi_video->yuv_range = ISP_COLOR_RANGE_FULL;
    csi_video->yuv_std = ISP_YUV_CONV_STD_BT601;
#if ESP_VIDEO_ISP_DEVICE_CROP
    csi_video->set_crop = false;
#endif

    config->v4l2_format = csi_output_v4l2_fmt;
    return ESP_OK;
}

static esp_err_t csi_video_init(esp_video_device_common_t *common)
{
    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = CSI_LDO_UNIT_ID,
        .voltage_mv = CSI_LDO_CFG_VOL_MV,
    };
    struct csi_video *csi_video = (struct csi_video *)common->priv;

    if (!csi_video->dont_init_ldo) {
        ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&ldo_cfg, &csi_video->ldo_handle), TAG, "failed to init LDO");
    }

    return ESP_OK;
}

static esp_err_t start_csi_ctlr(esp_video_device_common_t *common, esp_cam_ctlr_handle_t *cam_ctrl_handle_ret, bool isp_swap_short_required)
{
    struct csi_video *csi_video = (struct csi_video *)common->priv;
    const esp_cam_sensor_format_t *sensor_format = common->sensor_format;
    esp_video_csi_isp_in_out_format_t *in_out_format = &csi_video->in_out_format;
    const esp_cam_sensor_mipi_info_t *mipi_info = &sensor_format->mipi_info;
    esp_cam_ctlr_handle_t cam_ctrl_handle;

    esp_cam_ctlr_csi_config_t csi_config = {
        .ctlr_id = CSI_CTRL_ID,
        .clk_src = MIPI_CSI_PHY_CLK_SRC_DEFAULT,
        .byte_swap_en = false, // no byte swap for CSI
        .queue_items = CSI_QUEUE_ITEMS,
        .data_lane_num = mipi_info->lane_num,
        .h_res = CAPTURE_VIDEO_GET_FORMAT_WIDTH(common->video),
        .v_res = CAPTURE_VIDEO_GET_FORMAT_HEIGHT(common->video),
        .input_data_color_type = in_out_format->csi_input_fmt,
        .output_data_color_type = in_out_format->csi_output_fmt,
        .lane_bit_rate_mbps = mipi_info->mipi_clk / (1000 * 1000),
#if CONFIG_ESP_VIDEO_DISABLE_MIPI_CSI_DRIVER_BACKUP_BUFFER
        .bk_buffer_dis = true,
#endif
    };

#if ESP_VIDEO_CSI_DEVICE_SWAP_SHORT
    /**
     * If the ISP swap short is required, don't enable 16-bit swap for input data
     * because the ISP swap short will handle the data swapping.
     */
    if (!isp_swap_short_required) {
        /**
         * If the sensor output format is YUV422, enable 16-bit swap for input data
         */
        if (sensor_format->format == ESP_CAM_SENSOR_PIXFORMAT_YUV422_YUYV ||
                sensor_format->format == ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY) {
            csi_config.input_16bit_swap_en = true;

            ESP_LOGI(TAG, "16-bit HW swap enabled");
        } else {
            ESP_LOGD(TAG, "16-bit HW not enabled");
        }
    }
#endif

    ESP_RETURN_ON_ERROR(esp_cam_new_csi_ctlr(&csi_config, &cam_ctrl_handle), TAG, "failed to new CSI");

    *cam_ctrl_handle_ret = cam_ctrl_handle;

    return ESP_OK;
}

static esp_err_t start_csi_swap_short(esp_video_device_common_t *common, bool isp_swap_short_required)
{
#if ESP_VIDEO_CSI_DEVICE_SW_SWAP_SHORT
    /**
     * If the ISP swap short is required, don't start CSI swap short
     */
    if (isp_swap_short_required) {
        ESP_LOGD(TAG, "ISP swap short is required, skipping CSI swap short");
        return ESP_OK;
    } else {
        ESP_LOGD(TAG, "ISP swap short is not required, starting CSI swap short");
    }

    const esp_cam_sensor_format_t *sensor_format = common->sensor_format;
    struct csi_video *csi_video = (struct csi_video *)common->priv;
    struct esp_video *video = common->video;

    if (sensor_format->format == ESP_CAM_SENSOR_PIXFORMAT_YUV422_YUYV ||
            sensor_format->format == ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY) {
        csi_video->swap_short = esp_video_swap_short_create(CAPTURE_VIDEO_BUF_SIZE(video));
        if (!csi_video->swap_short) {
            return ESP_ERR_NO_MEM;
        }

        ESP_LOGI(TAG, "16-bit SW swap enabled");
    } else {
        ESP_LOGD(TAG, "16-bit SW swap not enabled");
    }
#endif
    return ESP_OK;
}

static void stop_csi_swap_short(esp_video_device_common_t *common)
{
#if ESP_VIDEO_CSI_DEVICE_SW_SWAP_SHORT
    struct csi_video *csi_video = (struct csi_video *)common->priv;
    if (csi_video->swap_short) {
        esp_video_swap_short_free(csi_video->swap_short);
        csi_video->swap_short = NULL;
    }
#endif
}

static esp_err_t add_isp_proc(isp_proc_handle_t isp_proc, uint32_t width, uint32_t height,
                              bool crop_required, const struct v4l2_rect *crop_rect,
                              const esp_video_csi_isp_in_out_format_t *in_out_format)
{
    esp_err_t ret = ESP_OK;

#if CONFIG_ESP_VIDEO_ENABLE_ISP_VIDEO_DEVICE
    ret = esp_video_isp_video_device_add_isp_proc(isp_proc, width, height, crop_required, crop_rect, in_out_format);
#endif

    return ret;
}

static esp_err_t remove_isp_proc(isp_proc_handle_t isp_proc)
{
    esp_err_t ret = ESP_OK;

#if CONFIG_ESP_VIDEO_ENABLE_ISP_VIDEO_DEVICE
    ret = esp_video_isp_video_device_remove_isp_proc(isp_proc);
#endif

    return ret;
}

/**
 * @brief Start ISP process based on MIPI-CSI state
 *
 * @param state MIPI-CSI state object
 * @param state MIPI-CSI V4L2 capture format
 * @param isp_swap_short_required Whether ISP swap short is required
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t start_isp(esp_video_device_common_t *common, bool isp_swap_short_required)
{
    esp_err_t ret;
    isp_proc_handle_t isp_proc;
    struct csi_video *csi_video = (struct csi_video *)common->priv;
    struct esp_video *video = common->video;
    const esp_cam_sensor_format_t *sensor_format = common->sensor_format;
    const esp_cam_sensor_mipi_info_t *mipi_info = &sensor_format->mipi_info;
    esp_video_csi_isp_in_out_format_t *in_out_format = &csi_video->in_out_format;
    uint32_t width = sensor_format->width;
    uint32_t height = sensor_format->height;

    esp_isp_processor_cfg_t isp_config = {
        .clk_src = ISP_CLK_SRC_DEFAULT,
        .input_data_source = ISP_INPUT_DATA_SOURCE_CSI, // Force input data source to CSI
        .has_line_start_packet = mipi_info->line_sync_en,
        .has_line_end_packet = mipi_info->line_sync_en,
        .h_res = width,
        .v_res = height,
        .yuv_range = csi_video->yuv_range,
        .yuv_std = csi_video->yuv_std,
        .input_data_color_type = in_out_format->isp_input_fmt,
        .output_data_color_type = in_out_format->isp_output_fmt,
        .bayer_order = csi_video->bayer_order,
#if ESP_VIDEO_ISP_DRIVER_HAS_BYPASS
        .flags = {
            .bypass_isp = in_out_format->isp_bypass_required
        }
#endif
    };

#if ESP_VIDEO_ISP_DRIVER_HAS_BYTE_SWAP
    if (isp_swap_short_required) {
        isp_config.flags.byte_swap_en = true;
    }
#endif

    // Set clk_hz according to clk_src
    switch (isp_config.clk_src) {
    case ISP_CLK_SRC_XTAL:
        isp_config.clk_hz = 40000000; // 40MHz XTAL typical for ESP chips
        break;
    case ISP_CLK_SRC_PLL160:
        isp_config.clk_hz = 160000000; // 160MHz PLL
        break;
    case ISP_CLK_SRC_PLL240: // Default value
    default:
        isp_config.clk_hz = 240000000; // 240MHz PLL
        break;
    }
    ESP_RETURN_ON_ERROR(esp_isp_new_processor(&isp_config, &isp_proc), TAG, "failed to new ISP");

    bool crop_required = false;
#if ESP_VIDEO_ISP_DEVICE_CROP
    crop_required = csi_video->set_crop;
#endif
    ESP_GOTO_ON_ERROR(add_isp_proc(isp_proc, width, height, crop_required, CAPTURE_VIDEO_GET_RECT(video), in_out_format), fail_0, TAG, "failed to add ISP");

    if (in_out_format->isp_bypass_required) {
#ifndef ESP_VIDEO_ISP_DRIVER_HAS_BYPASS
        ISP.frame_cfg.hadr_num = ceil((float)(width * in_out_format->isp_bpp) / 32.0) - 1;
        ISP.frame_cfg.vadr_num = height - 1;
        ISP.cntl.isp_en = 0;
#endif
    } else {
        ESP_GOTO_ON_ERROR(esp_isp_enable(isp_proc), fail_1, TAG, "failed to enable ISP");
    }

    csi_video->isp_proc = isp_proc;
    return ESP_OK;

fail_1:
    remove_isp_proc(isp_proc);
fail_0:
    esp_isp_del_processor(isp_proc);
    csi_video->isp_proc = NULL;
    return ret;
}

static esp_err_t stop_isp(esp_video_device_common_t *common)
{
    struct csi_video *csi_video = (struct csi_video *)common->priv;
    isp_proc_handle_t isp_proc = csi_video->isp_proc;
    esp_video_csi_isp_in_out_format_t *in_out_format = &csi_video->in_out_format;

    if (!in_out_format->isp_bypass_required) {
        ESP_RETURN_ON_ERROR(esp_isp_disable(isp_proc), TAG, "failed to disable ISP");
    }

    ESP_RETURN_ON_ERROR(remove_isp_proc(isp_proc), TAG, "failed to remove ISP");

    ESP_RETURN_ON_ERROR(esp_isp_del_processor(isp_proc), TAG, "failed to delete ISP");
    csi_video->isp_proc = NULL;

    return ESP_OK;
}

static esp_err_t csi_video_start(esp_video_device_common_t *common, esp_cam_ctlr_handle_t *cam_ctrl_handle_ret)
{
    esp_err_t ret = ESP_OK;
    bool isp_swap_short_required = false;

#if ESP_VIDEO_ISP_DRIVER_HAS_BYTE_SWAP
    uint32_t data_seq = ESP_CAM_SENSOR_DATA_SEQ_NONE;
    if (esp_cam_sensor_get_para_value(common->cam.sensor, ESP_CAM_SENSOR_DATA_SEQ, &data_seq, sizeof(data_seq)) == ESP_OK) {
        if (data_seq == ESP_CAM_SENSOR_DATA_SEQ_WORD_INTERNAL_SWAPPED) {
            isp_swap_short_required = true;
            ESP_LOGI(TAG, "ISP swap short enabled");
        }
    }
#endif

    ESP_RETURN_ON_ERROR(start_csi_swap_short(common, isp_swap_short_required), TAG, "failed to start CSI swap short");
    ESP_GOTO_ON_ERROR(start_csi_ctlr(common, cam_ctrl_handle_ret, isp_swap_short_required), fail_0, TAG, "failed to start CSI ctlr");
    ESP_GOTO_ON_ERROR(start_isp(common, isp_swap_short_required), fail_1, TAG, "failed to start ISP");

    return ESP_OK;

fail_1:
    esp_cam_ctlr_del(*cam_ctrl_handle_ret);
    *cam_ctrl_handle_ret = NULL;
fail_0:
    stop_csi_swap_short(common);
    return ret;
}

static esp_err_t csi_video_stop(esp_video_device_common_t *common)
{
    ESP_RETURN_ON_ERROR(stop_isp(common), TAG, "failed to stop ISP");
    stop_csi_swap_short(common);

    return ESP_OK;
}

static esp_err_t csi_video_deinit(esp_video_device_common_t *common)
{
    struct csi_video *csi_video = (struct csi_video *)common->priv;

    if (!csi_video->dont_init_ldo) {
        ESP_RETURN_ON_ERROR(esp_ldo_release_channel(csi_video->ldo_handle), TAG, "failed to release LDO");
        csi_video->ldo_handle = NULL;
    }

    return ESP_OK;
}

static esp_err_t csi_video_enum_format(esp_video_device_common_t *common, uint32_t index, uint32_t *pixel_format)
{
    return esp_video_csi_enum_format(common->sensor_format->format, index, pixel_format);
}

static esp_err_t csi_check_set_format(esp_video_device_common_t *common, const struct v4l2_format *format)
{
    struct csi_video *csi_video = (struct csi_video *)common->priv;

    if ((format->fmt.pix.quantization == V4L2_QUANTIZATION_DEFAULT) ||
            (format->fmt.pix.quantization == V4L2_QUANTIZATION_FULL_RANGE)) {
        csi_video->yuv_range = ISP_COLOR_RANGE_FULL;
    } else if (format->fmt.pix.quantization == V4L2_QUANTIZATION_LIM_RANGE) {
        csi_video->yuv_range = ISP_COLOR_RANGE_LIMIT;
    } else {
        return ESP_ERR_NOT_SUPPORTED;
    }

    if ((format->fmt.pix.ycbcr_enc == V4L2_YCBCR_ENC_DEFAULT) ||
            (format->fmt.pix.ycbcr_enc == V4L2_YCBCR_ENC_601)) {
        csi_video->yuv_std = ISP_YUV_CONV_STD_BT601;
    } else if (format->fmt.pix.ycbcr_enc == V4L2_YCBCR_ENC_709) {
        csi_video->yuv_std = ISP_YUV_CONV_STD_BT709;
    } else {
        return ESP_ERR_NOT_SUPPORTED;
    }

    ESP_RETURN_ON_ERROR(esp_video_csi_check_format(common->sensor_format->format, format->fmt.pix.pixelformat, &csi_video->in_out_format), TAG, "failed to check CSI format");
    ESP_RETURN_ON_ERROR(esp_video_config_buffer(common->video, format, common->mem_caps), TAG, "failed to configure stream buffer");

    return ESP_OK;
}

static esp_err_t csi_video_reprocess(esp_video_device_common_t *common, uint8_t *src, size_t src_size, uint8_t *dst, size_t dst_size, size_t *dst_out_size)
{
    esp_err_t ret = ESP_OK;

    *dst_out_size = src_size;

#if ESP_VIDEO_CSI_DEVICE_SW_SWAP_SHORT
    struct csi_video *csi_video = (struct csi_video *)common->priv;

    if (csi_video->swap_short) {
        ret = esp_video_swap_short_process(csi_video->swap_short, src, src_size, dst, dst_size, dst_out_size);
    }
#endif

    return ret;
}

#if CONFIG_ESP_VIDEO_ENABLE_ISP_VIDEO_DEVICE && ESP_VIDEO_ISP_DEVICE_CROP
static esp_err_t csi_set_selection(esp_video_device_common_t *common, struct v4l2_selection *selection)
{
    esp_err_t ret = ESP_OK;
    struct csi_video *csi_video = (struct csi_video *)common->priv;
    const esp_cam_sensor_format_t *sensor_format = common->sensor_format;

    if (common->cam_ctrl_handle) {
        ESP_LOGE(TAG, "MIPI-CSI should be stream off");
        return ESP_ERR_INVALID_STATE;
    }

    if (selection->target == V4L2_SEL_TGT_CROP) {
        struct v4l2_rect *r = &selection->r;

        if (r->left >= sensor_format->width || (r->width + r->left) > sensor_format->width || (r->width < ISP_CROP_MIN_WIDTH) || r->left < 0 ||
                r->top >= sensor_format->height || (r->height + r->top) > sensor_format->height || (r->height < ISP_CROP_MIN_HEIGHT) || r->top < 0) {
            ESP_LOGE(TAG, "crop width or height is invalid");
            return ESP_ERR_INVALID_ARG;
        }

        struct v4l2_format format = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .fmt.pix = {
                .width = r->width,
                .height = r->height,
                .pixelformat = CAPTURE_VIDEO_GET_FORMAT_PIXEL_FORMAT(common->video),
            },
        };

        ESP_RETURN_ON_ERROR(esp_video_config_buffer(common->video, &format, CSI_MEM_CAPS), TAG, "failed to configure stream buffer");
        csi_video->set_crop = true;
    } else {
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}
#endif

static esp_err_t csi_check_enum_framesizes(esp_video_device_common_t *common, struct v4l2_frmsizeenum *frmsize)
{
    esp_video_csi_isp_in_out_format_t in_out_format;
    if (esp_video_csi_check_format(common->sensor_format->format, frmsize->pixel_format, &in_out_format) != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static const esp_video_device_intf_t s_csi_device_intf = {
    .init              = csi_video_init,
    .start_init_config = csi_start_init_config,
    .deinit            = csi_video_deinit,
    .start             = csi_video_start,
    .stop              = csi_video_stop,
    .enum_format       = csi_video_enum_format,
    .check_set_format  = csi_check_set_format,
    .reprocess         = csi_video_reprocess,
    .check_enum_framesizes = csi_check_enum_framesizes,
#if CONFIG_ESP_VIDEO_ENABLE_ISP_VIDEO_DEVICE && ESP_VIDEO_ISP_DEVICE_CROP
    .set_selection     = csi_set_selection,
#endif
};

/**
 * @brief Create MIPI CSI video device
 *
 * @param sensor camera sensor device
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_create_csi_video_device(esp_cam_sensor_device_t *sensor, const esp_video_csi_device_config_t *config)
{
    esp_err_t ret;
    struct csi_video *csi_video;

    csi_video = heap_caps_calloc(1, sizeof(struct csi_video), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!csi_video) {
        return ESP_ERR_NO_MEM;
    }
    csi_video->dont_init_ldo = config->dont_init_ldo;

    esp_video_device_common_config_t common_config = {
        .name = CSI_NAME,
        .id = ESP_VIDEO_MIPI_CSI_DEVICE_ID,
        .priv = csi_video,
        .intf = &s_csi_device_intf,
        .cam = {
            .sensor = sensor,
        },
        .mem_caps = CSI_MEM_CAPS,
#if CONFIG_ESP_VIDEO_DISABLE_MIPI_CSI_DRIVER_BACKUP_BUFFER
        .use_backup_element = true,
#else
        .use_backup_element = false,
#endif
    };
    ret = esp_video_device_common_create(&common_config, &csi_video->common);
    if (ret != ESP_OK) {
        heap_caps_free(csi_video);
        return ret;
    }

    return ESP_OK;
}

/**
 * @brief Add camera motor device to MIPI-CSI video device
 *
 * @param motor_dev camera motor device
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_csi_video_device_add_motor(esp_cam_motor_device_t *motor_dev)
{
    struct esp_video *video;
    esp_video_device_common_t *common;

    video = esp_video_device_get_object(CSI_NAME);
    if (!video) {
        return ESP_ERR_NOT_FOUND;
    }

    common = VIDEO_PRIV_DATA(esp_video_device_common_t *, video);

    if (common->cam.motor) {
        return ESP_ERR_INVALID_STATE;
    }

    common->cam.motor = motor_dev;

    return ESP_OK;
}

/**
 * @brief Destroy MIPI-CSI video device
 *
 * @param None
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_destroy_csi_video_device(void)
{
    struct csi_video *csi_video;

    ESP_RETURN_ON_ERROR(esp_video_device_common_get_priv(CSI_NAME, (void **)&csi_video), TAG, "failed to get private data");
    ESP_RETURN_ON_ERROR(esp_video_device_common_free(csi_video->common), TAG, "failed to free common video device");
    heap_caps_free(csi_video);

    return ESP_OK;
}
