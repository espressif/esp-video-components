/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_attr.h"

/**
 * IDF-9706
 */
#include "soc/isp_struct.h"

#include "esp_ldo_regulator.h"
#include "esp_cam_ctlr.h"
#include "esp_cam_ctlr_csi.h"
#include "driver/isp.h"

#include "esp_video.h"
#include "esp_video_sensor.h"
#include "esp_video_device_internal.h"

#define CSI_NAME                    "MIPI-CSI"

#define CSI_LDO_UNIT_ID             3
#define CSI_LDO_CFG_VOL_MV          2500

#define CSI_DMA_ALIGN_BYTES         64
#define CSI_MEM_CAPS                (MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM)

#define CSI_CTRL_ID                 0
#define CSI_CLK_SRC                 MIPI_CSI_PHY_CLK_SRC_DEFAULT
#define CSI_QUEUE_ITEMS             1

/* AEG-1488 */
#define CSI_BYTE_SWAP_EN            false

#define ISP_INPUT_DATA_SRC          ISP_INPUT_DATA_SOURCE_CSI

/* AEG-1489 */
#define ISP_CLK_SRC                 ISP_CLK_SRC_DEFAULT
#define ISP_CLK_FREQ_HZ             (80 * 1000 * 1000)

#define ISP_DEFAULT_OUT_COLOR       ISP_COLOR_RGB565
#define CSI_DEFAULT_OUT_COLOR       CAM_CTLR_COLOR_RGB565
#define CSI_DEFAULT_OUT_BPP         16
#define V4L2_DEFAULT_OUT_COLOR      V4L2_PIX_FMT_RGB565

#define ISP_BYPASS_IN_COLOR         ISP_COLOR_RAW8

struct csi_video {
    uint32_t lane_bit_rate_mbps;
    uint8_t csi_data_lane_num;
    cam_ctlr_color_t csi_in_color;
    cam_ctlr_color_t csi_out_color;
    uint8_t csi_out_bpp;
    bool csi_line_sync;

    bool isp_enable;
    isp_color_t isp_in_color;
    isp_color_t isp_out_color;

    esp_cam_ctlr_handle_t cam_ctrl_handle;
    esp_ldo_channel_handle_t ldo_handle;
    isp_proc_handle_t isp_processor;

    esp_cam_sensor_device_t *cam_dev;
#if CONFIG_ESP_VIDEO_DISABLE_MIPI_CSI_DRIVER_BACKUP_BUFFER
    struct esp_video_buffer_element *element;
#endif
};

static const char *TAG = "csi_video";

static esp_err_t csi_get_input_frame_type_from_sensor(uint32_t sensor_fmt, cam_ctlr_color_t *csi_color, uint8_t *csi_in_bpp)
{
    esp_err_t ret = ESP_OK;

    switch (sensor_fmt) {
    case ESP_CAM_SENSOR_PIXFORMAT_RAW8:
        *csi_color = CAM_CTLR_COLOR_RAW8;
        *csi_in_bpp = 8;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW10:
        *csi_color = CAM_CTLR_COLOR_RAW10;
        *csi_in_bpp = 10;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW12:
        *csi_color = CAM_CTLR_COLOR_RAW12;
        *csi_in_bpp = 12;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_RGB565:
        *csi_color = CAM_CTLR_COLOR_RGB565;
        *csi_in_bpp = 16;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_RGB888:
        *csi_color = CAM_CTLR_COLOR_RGB888;
        *csi_in_bpp = 24;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_YUV420:
        *csi_color = CAM_CTLR_COLOR_YUV420;
        *csi_in_bpp = 12;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_YUV422:
        *csi_color = CAM_CTLR_COLOR_YUV422;
        *csi_in_bpp = 16;
        break;
    default:
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }

    return ret;
}

static esp_err_t csi_get_output_frame_type_from_v4l2(uint32_t output_fmt, cam_ctlr_color_t *csi_color, uint8_t *out_bpp)
{
    esp_err_t ret = ESP_OK;

    switch (output_fmt) {
    case V4L2_PIX_FMT_SBGGR8:
        *csi_color = CAM_CTLR_COLOR_RAW8;
        *out_bpp = 8;
        break;
    case V4L2_PIX_FMT_RGB565:
        *csi_color = CAM_CTLR_COLOR_RGB565;
        *out_bpp = 16;
        break;
    case V4L2_PIX_FMT_RGB24:
        *csi_color = CAM_CTLR_COLOR_RGB888;
        *out_bpp = 24;
        break;
    case V4L2_PIX_FMT_YUV420:
        *csi_color = CAM_CTLR_COLOR_YUV420;
        *out_bpp = 12;
        break;
    case V4L2_PIX_FMT_YUV422P:
        *csi_color = CAM_CTLR_COLOR_YUV422;
        *out_bpp = 16;
        break;
    default:
        ret = ESP_ERR_NOT_SUPPORTED;
        *out_bpp = 0;
        break;
    }

    return ret;
}

static esp_err_t isp_get_input_frame_type_from_sensor(uint32_t sensor_fmt, isp_color_t *isp_color)
{
    esp_err_t ret = ESP_OK;

    switch (sensor_fmt) {
    case ESP_CAM_SENSOR_PIXFORMAT_RAW8:
        *isp_color = ISP_COLOR_RAW8;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW10:
        *isp_color = ISP_COLOR_RAW10;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW12:
        *isp_color = ISP_COLOR_RAW12;
        break;
    default:
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }

    return ret;
}

static esp_err_t isp_get_output_frame_type_from_sensor(uint32_t output_fmt, isp_color_t *isp_color)
{
    esp_err_t ret = ESP_OK;

    switch (output_fmt) {
    case ESP_CAM_SENSOR_PIXFORMAT_RAW8:
        *isp_color = ISP_COLOR_RAW8;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_RGB565:
        *isp_color = ISP_COLOR_RGB565;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_RGB888:
        *isp_color = ISP_COLOR_RGB888;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_YUV420:
        *isp_color = ISP_COLOR_YUV420;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_YUV422:
        *isp_color = ISP_COLOR_YUV422;
        break;
    default:
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }

    return ret;
}

static esp_err_t isp_get_output_frame_type_from_v4l2(uint32_t output_fmt, isp_color_t *isp_color)
{
    esp_err_t ret = ESP_OK;

    switch (output_fmt) {
    case V4L2_PIX_FMT_SBGGR8:
        *isp_color = ISP_COLOR_RAW8;
        break;
    case V4L2_PIX_FMT_RGB565:
        *isp_color = ISP_COLOR_RGB565;
        break;
    case V4L2_PIX_FMT_RGB24:
        *isp_color = ISP_COLOR_RGB888;
        break;
    case V4L2_PIX_FMT_YUV420:
        *isp_color = ISP_COLOR_YUV420;
        break;
    case V4L2_PIX_FMT_YUV422P:
        *isp_color = ISP_COLOR_YUV422;
        break;
    default:
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }

    return ret;
}

static esp_err_t csi_get_data_lane(uint32_t port, uint8_t *data_lane_num)
{
    esp_err_t ret = ESP_OK;

    switch (port) {
    case 1:
        *data_lane_num = 1;
        break;
    case 2:
        *data_lane_num = 2;
        break;
    default:
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }

    return ret;
}

static esp_err_t v4l2_get_input_frame_type_from_sensor(uint32_t sensor_fmt, uint32_t *v4l2_format)
{
    esp_err_t ret = ESP_OK;

    switch (sensor_fmt) {
    case ESP_CAM_SENSOR_PIXFORMAT_RAW8:
        *v4l2_format = V4L2_PIX_FMT_SBGGR8;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW10:
        *v4l2_format = V4L2_PIX_FMT_SBGGR10;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW12:
        *v4l2_format = V4L2_PIX_FMT_SBGGR12;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_RGB565:
        *v4l2_format = V4L2_PIX_FMT_RGB565;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_RGB888:
        *v4l2_format = V4L2_PIX_FMT_RGB24;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_YUV420:
        *v4l2_format = V4L2_PIX_FMT_YUV420;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_YUV422:
        *v4l2_format = V4L2_PIX_FMT_YUV422P;
        break;
    default:
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }

    return ret;
}

static bool IRAM_ATTR csi_video_on_trans_finished(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data)
{
    struct esp_video *video = (struct esp_video *)user_data;

    ESP_LOGD(TAG, "size=%zu", trans->received_size);

#if CONFIG_ESP_VIDEO_DISABLE_MIPI_CSI_DRIVER_BACKUP_BUFFER
    struct csi_video *csi_video = VIDEO_PRIV_DATA(struct csi_video *, video);
    if (trans->buffer != csi_video->element->buffer) {
        CAPTURE_VIDEO_DONE_BUF(video, trans->buffer, trans->received_size);
    }
#else
    CAPTURE_VIDEO_DONE_BUF(video, trans->buffer, trans->received_size);
#endif

    return true;
}

static bool IRAM_ATTR csi_video_on_get_new_trans(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data)
{
    struct esp_video_buffer_element *element;
    struct esp_video *video = (struct esp_video *)user_data;

    element = CAPTURE_VIDEO_GET_QUEUED_ELEMENT(video);
#if CONFIG_ESP_VIDEO_DISABLE_MIPI_CSI_DRIVER_BACKUP_BUFFER
    struct csi_video *csi_video = VIDEO_PRIV_DATA(struct csi_video *, video);

    if (!element) {
        element = csi_video->element;
    } else {
        csi_video->element = element;
    }
#else
    if (!element) {
        return false;
    }
#endif

    trans->buffer = element->buffer;
    trans->buflen = ELEMENT_SIZE(element);

    return true;
}

static esp_err_t csi_video_init(struct esp_video *video)
{
    esp_err_t ret;
    uint8_t csi_in_bpp;
    uint32_t v4l2_format;
    esp_cam_sensor_format_t sensor_format;
    struct csi_video *csi_video = VIDEO_PRIV_DATA(struct csi_video *, video);
    esp_cam_sensor_device_t *cam_dev = csi_video->cam_dev;

    ret = esp_cam_sensor_set_format(cam_dev, NULL);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_cam_sensor_get_format(cam_dev, &sensor_format);
    if (ret != ESP_OK) {
        return ret;
    }

    if (!sensor_format.mipi_info.mipi_clk) {
        ESP_LOGE(TAG, "camera sensor mipi_clk is 0");
        return ESP_ERR_NOT_SUPPORTED;
    } else {
        csi_video->lane_bit_rate_mbps = sensor_format.mipi_info.mipi_clk / (1000 * 1000);
    }

    ret = csi_get_data_lane(sensor_format.mipi_info.lane_num, &csi_video->csi_data_lane_num);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to get CSI data lane number");
        return ret;
    }

    ret = csi_get_input_frame_type_from_sensor(sensor_format.format, &csi_video->csi_in_color, &csi_in_bpp);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to get CSI input frame type");
        return ret;
    }

    if (sensor_format.isp_info) {
        ret = isp_get_input_frame_type_from_sensor(sensor_format.format, &csi_video->isp_in_color);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "failed to get CSI input frame type");
            return ret;
        }

        csi_video->isp_enable = 1;
        csi_video->isp_out_color = ISP_DEFAULT_OUT_COLOR;

        csi_video->csi_out_color = CSI_DEFAULT_OUT_COLOR;
        csi_video->csi_out_bpp = CSI_DEFAULT_OUT_BPP;

        v4l2_format = V4L2_DEFAULT_OUT_COLOR;
    } else {
        ret = isp_get_output_frame_type_from_sensor(sensor_format.format, &csi_video->isp_out_color);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "failed to get ISP output frame type");
            return ret;
        }

        ret = v4l2_get_input_frame_type_from_sensor(sensor_format.format, &v4l2_format);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "failed to get V4L2 input frame type");
            return ret;
        }

        csi_video->isp_enable = 0;
        csi_video->isp_in_color = ISP_BYPASS_IN_COLOR;

        csi_video->csi_out_color = csi_video->csi_in_color;
        csi_video->csi_out_bpp = csi_in_bpp;
    }

    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = CSI_LDO_UNIT_ID,
        .voltage_mv = CSI_LDO_CFG_VOL_MV,
    };
    ret = esp_ldo_acquire_channel(&ldo_cfg, &csi_video->ldo_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to init LDO");
        return ret;
    }

    csi_video->csi_line_sync = sensor_format.mipi_info.line_sync_en;

    CAPTURE_VIDEO_SET_FORMAT(video,
                             sensor_format.width,
                             sensor_format.height,
                             v4l2_format);

    uint32_t buf_size = CAPTURE_VIDEO_GET_FORMAT_WIDTH(video) * CAPTURE_VIDEO_GET_FORMAT_HEIGHT(video) * csi_video->csi_out_bpp / 8;

    ESP_LOGD(TAG, "buffer size=%" PRIu32, buf_size);

    CAPTURE_VIDEO_SET_BUF_INFO(video, buf_size, CSI_DMA_ALIGN_BYTES, CSI_MEM_CAPS);

    return ESP_OK;
}

static esp_err_t csi_video_start(struct esp_video *video, uint32_t type)
{
    esp_err_t ret;
    struct csi_video *csi_video = VIDEO_PRIV_DATA(struct csi_video *, video);
    esp_cam_sensor_device_t *cam_dev = csi_video->cam_dev;

    esp_cam_ctlr_csi_config_t csi_config = {
        .ctlr_id = CSI_CTRL_ID,
        .clk_src = CSI_CLK_SRC,
        .byte_swap_en = CSI_BYTE_SWAP_EN,
        .queue_items = CSI_QUEUE_ITEMS,
        .h_res = CAPTURE_VIDEO_GET_FORMAT_WIDTH(video),
        .v_res = CAPTURE_VIDEO_GET_FORMAT_HEIGHT(video),
        .data_lane_num = csi_video->csi_data_lane_num,
        .input_data_color_type = csi_video->csi_in_color,
        .output_data_color_type = csi_video->csi_out_color,
        .lane_bit_rate_mbps = csi_video->lane_bit_rate_mbps,
#if CONFIG_ESP_VIDEO_DISABLE_MIPI_CSI_DRIVER_BACKUP_BUFFER
        .bk_buffer_dis = true,
#endif
    };
    ret = esp_cam_new_csi_ctlr(&csi_config, &csi_video->cam_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to create CSI");
        goto exit_0;
    }

    esp_cam_ctlr_evt_cbs_t cam_ctrl_cbs = {
        .on_get_new_trans = csi_video_on_get_new_trans,
        .on_trans_finished = csi_video_on_trans_finished
    };
    ret = esp_cam_ctlr_register_event_callbacks(csi_video->cam_ctrl_handle, &cam_ctrl_cbs, video);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to register CAM ctlr event callback");
        goto exit_1;
    }

    esp_isp_processor_cfg_t isp_config = {
        .clk_src = ISP_CLK_SRC,
        .input_data_source = ISP_INPUT_DATA_SRC,
        .has_line_start_packet = csi_video->csi_line_sync,
        .has_line_end_packet = csi_video->csi_line_sync,
        .h_res = CAPTURE_VIDEO_GET_FORMAT_WIDTH(video),
        .v_res = CAPTURE_VIDEO_GET_FORMAT_HEIGHT(video),
        .clk_hz = ISP_CLK_FREQ_HZ,
        .input_data_color_type = csi_video->isp_in_color,
        .output_data_color_type = csi_video->isp_out_color,
    };
    ret = esp_isp_new_processor(&isp_config, &csi_video->isp_processor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to create ISP");
        goto exit_1;
    }

    if (csi_video->isp_enable) {
        ret = esp_isp_enable(csi_video->isp_processor);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "failed to enable ISP");
            goto exit_2;
        }
    } else {
        /**
         * IDF-9706
         */

        ISP.frame_cfg.hadr_num = ceil((float)(isp_config.h_res * 16) / 32.0) - 1;
        ISP.frame_cfg.vadr_num = isp_config.v_res - 1;
        ISP.cntl.isp_en = 0;
    }

    ret = esp_cam_ctlr_enable(csi_video->cam_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to enable CAM ctlr");
        goto exit_3;
    }

    ret = esp_cam_ctlr_start(csi_video->cam_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to start CAM ctlr");
        goto exit_4;
    }

    int flags = 1;
    ret = esp_cam_sensor_ioctl(cam_dev, ESP_CAM_SENSOR_IOC_S_STREAM, &flags);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to start sensor");
        goto exit_5;
    }

    return ESP_OK;

exit_5:
    esp_cam_ctlr_stop(csi_video->cam_ctrl_handle);
exit_4:
    esp_cam_ctlr_disable(csi_video->cam_ctrl_handle);
exit_3:
    if (csi_video->isp_enable) {
        esp_isp_disable(csi_video->isp_processor);
    }
exit_2:
    esp_isp_del_processor(csi_video->isp_processor);
    csi_video->isp_processor = NULL;
exit_1:
    esp_cam_ctlr_del(csi_video->cam_ctrl_handle);
    csi_video->cam_ctrl_handle = NULL;
exit_0:
    return ret;
}

static esp_err_t csi_video_stop(struct esp_video *video, uint32_t type)
{
    esp_err_t ret;
    int flags = 0;
    struct csi_video *csi_video = VIDEO_PRIV_DATA(struct csi_video *, video);
    esp_cam_sensor_device_t *cam_dev = csi_video->cam_dev;

    ret = esp_cam_sensor_ioctl(cam_dev, ESP_CAM_SENSOR_IOC_S_STREAM, &flags);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to disable sensor");
        return ret;
    }

    ret = esp_cam_ctlr_stop(csi_video->cam_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to disable CAM ctlr");
        return ret;
    }

    ret = esp_cam_ctlr_disable(csi_video->cam_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to disable CAM ctlr");
        return ret;
    }

    if (csi_video->isp_enable) {
        ret = esp_isp_disable(csi_video->isp_processor);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "failed to disable ISP");
            return ret;
        }
    }

    ret = esp_isp_del_processor(csi_video->isp_processor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to delete ISP");
        return ret;
    }

    csi_video->isp_processor = NULL;

    ret = esp_cam_ctlr_del(csi_video->cam_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to delete cam ctlr");
        return ret;
    }

    csi_video->cam_ctrl_handle = NULL;

    return ret;
}

static esp_err_t csi_video_deinit(struct esp_video *video)
{
    struct csi_video *csi_video = VIDEO_PRIV_DATA(struct csi_video *, video);

    esp_ldo_release_channel(csi_video->ldo_handle);
    csi_video->ldo_handle = NULL;

    return ESP_OK;
}

static esp_err_t csi_video_enum_format(struct esp_video *video, uint32_t type, uint32_t index, uint32_t *pixel_format)
{
    struct csi_video *csi_video = VIDEO_PRIV_DATA(struct csi_video *, video);

    if (csi_video->isp_enable) {
        static const uint32_t csi_isp_format[] = {
            V4L2_PIX_FMT_SBGGR8,
            V4L2_PIX_FMT_RGB565,
            V4L2_PIX_FMT_RGB24,
            V4L2_PIX_FMT_YUV420,
            V4L2_PIX_FMT_YUV422P,
        };

        if (index >= ARRAY_SIZE(csi_isp_format)) {
            return ESP_ERR_INVALID_ARG;
        }

        *pixel_format = csi_isp_format[index];
    } else {
        if (index > 0) {
            return ESP_ERR_INVALID_ARG;
        }

        *pixel_format = CAPTURE_VIDEO_GET_FORMAT_PIXEL_FORMAT(video);
    }

    return ESP_OK;
}

static esp_err_t csi_video_set_format(struct esp_video *video, uint32_t type, const struct esp_video_format *format)
{
    esp_err_t ret;
    struct csi_video *csi_video = VIDEO_PRIV_DATA(struct csi_video *, video);

    if (csi_video->isp_enable) {
        if (format->width != CAPTURE_VIDEO_GET_FORMAT_WIDTH(video) ||
                format->height != CAPTURE_VIDEO_GET_FORMAT_HEIGHT(video)) {
            ESP_LOGE(TAG, "width or height is not supported");
            return ESP_ERR_INVALID_ARG;
        }

        ret = csi_get_output_frame_type_from_v4l2(format->pixel_format, &csi_video->csi_out_color, &csi_video->csi_out_bpp);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "CSI does not support format=%" PRIx32, format->pixel_format);
            return ret;
        }

        ret = isp_get_output_frame_type_from_v4l2(format->pixel_format, &csi_video->isp_out_color);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ISP does not support format=%" PRIx32, format->pixel_format);
            return ret;
        }

        uint32_t buf_size = CAPTURE_VIDEO_GET_FORMAT_WIDTH(video) * CAPTURE_VIDEO_GET_FORMAT_HEIGHT(video) * csi_video->csi_out_bpp / 8;

        ESP_LOGD(TAG, "buffer size=%" PRIu32, buf_size);

        CAPTURE_VIDEO_SET_BUF_INFO(video, buf_size, CSI_DMA_ALIGN_BYTES, CSI_MEM_CAPS);
    } else {
        if (format->width != CAPTURE_VIDEO_GET_FORMAT_WIDTH(video) ||
                format->height != CAPTURE_VIDEO_GET_FORMAT_HEIGHT(video) ||
                format->pixel_format != CAPTURE_VIDEO_GET_FORMAT_PIXEL_FORMAT(video)) {
            ESP_LOGE(TAG, "width or height or format is not supported");
            return ESP_ERR_INVALID_ARG;
        }
    }

    return ESP_OK;
}

static esp_err_t csi_video_notify(struct esp_video *video, enum esp_video_event event, void *arg)
{
    return ESP_OK;
}

static esp_err_t csi_video_set_ext_ctrl(struct esp_video *video, const struct v4l2_ext_controls *ctrls)
{
    struct csi_video *csi_video = VIDEO_PRIV_DATA(struct csi_video *, video);

    return esp_video_set_ext_ctrls_to_sensor(csi_video->cam_dev, ctrls);
}

static esp_err_t csi_video_get_ext_ctrl(struct esp_video *video, struct v4l2_ext_controls *ctrls)
{
    struct csi_video *csi_video = VIDEO_PRIV_DATA(struct csi_video *, video);

    return esp_video_get_ext_ctrls_from_sensor(csi_video->cam_dev, ctrls);
}

static esp_err_t csi_video_query_ext_ctrl(struct esp_video *video, struct v4l2_query_ext_ctrl *qctrl)
{
    struct csi_video *csi_video = VIDEO_PRIV_DATA(struct csi_video *, video);

    return esp_video_query_ext_ctrls_from_sensor(csi_video->cam_dev, qctrl);
}

static const struct esp_video_ops s_csi_video_ops = {
    .init          = csi_video_init,
    .deinit        = csi_video_deinit,
    .start         = csi_video_start,
    .stop          = csi_video_stop,
    .enum_format   = csi_video_enum_format,
    .set_format    = csi_video_set_format,
    .notify        = csi_video_notify,
    .set_ext_ctrl  = csi_video_set_ext_ctrl,
    .get_ext_ctrl  = csi_video_get_ext_ctrl,
    .query_ext_ctrl = csi_video_query_ext_ctrl,
};

/**
 * @brief Create MIPI CSI video device
 *
 * @param cam_dev camera sensor device
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_create_csi_video_device(esp_cam_sensor_device_t *cam_dev)
{
    struct esp_video *video;
    struct csi_video *csi_video;
    uint32_t device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_READWRITE | V4L2_CAP_EXT_PIX_FORMAT |
                           V4L2_CAP_STREAMING;
    uint32_t caps = device_caps | V4L2_CAP_DEVICE_CAPS;

    csi_video = heap_caps_calloc(1, sizeof(struct csi_video), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!csi_video) {
        return ESP_ERR_NO_MEM;
    }

    csi_video->cam_dev = cam_dev;

    video = esp_video_create(CSI_NAME, ESP_VIDEO_MIPI_CSI_DEVICE_ID, &s_csi_video_ops, csi_video, caps, device_caps);
    if (!video) {
        heap_caps_free(csi_video);
        return ESP_FAIL;
    }

    return ESP_OK;
}
