/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <string.h>
#include <inttypes.h>
#include "esp_check.h"
#include "esp_log.h"
#include "esp_idf_version.h"
#include "esp_video_dvp_format.h"

static const char *TAG = "dvp_format";

#if ESP_VIDEO_DVP_DEVICE_CONV_FORMAT
static bool is_yuv422_yuyv_sensor(esp_cam_sensor_output_format_t sensor_fmt)
{
    return sensor_fmt == ESP_CAM_SENSOR_PIXFORMAT_YUV422_YUYV;
}
#endif

static uint32_t sensor_to_v4l2_format(esp_cam_sensor_output_format_t sensor_fmt)
{
    switch (sensor_fmt) {
    case ESP_CAM_SENSOR_PIXFORMAT_RGB565_LE:
        return V4L2_PIX_FMT_RGB565;
    case ESP_CAM_SENSOR_PIXFORMAT_RGB565_BE:
        return V4L2_PIX_FMT_RGB565X;
    case ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY:
        return V4L2_PIX_FMT_UYVY;
    case ESP_CAM_SENSOR_PIXFORMAT_YUV422_YUYV:
        return V4L2_PIX_FMT_YUYV;
    case ESP_CAM_SENSOR_PIXFORMAT_RGB888:
        return V4L2_PIX_FMT_RGB24;
    case ESP_CAM_SENSOR_PIXFORMAT_JPEG:
        return V4L2_PIX_FMT_JPEG;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW8:
        return V4L2_PIX_FMT_SBGGR8;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW10:
        return V4L2_PIX_FMT_SBGGR10;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW12:
        return V4L2_PIX_FMT_SBGGR12;
    case ESP_CAM_SENSOR_PIXFORMAT_YUV420:
        return V4L2_PIX_FMT_YUV420;
    case ESP_CAM_SENSOR_PIXFORMAT_GRAYSCALE:
        return V4L2_PIX_FMT_GREY;
    default:
        break;
    }

    return 0;
}

static cam_ctlr_color_t sensor_to_cam_color(esp_cam_sensor_output_format_t sensor_fmt)
{
    switch (sensor_fmt) {
    case ESP_CAM_SENSOR_PIXFORMAT_RGB565_LE:
    case ESP_CAM_SENSOR_PIXFORMAT_RGB565_BE:
        return CAM_CTLR_COLOR_RGB565;
    case ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY:
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
        return CAM_CTLR_COLOR_YUV422_UYVY;
#else
        return CAM_CTLR_COLOR_YUV422;
#endif
    case ESP_CAM_SENSOR_PIXFORMAT_YUV422_YUYV:
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
        return CAM_CTLR_COLOR_YUV422_YUYV;
#else
        return CAM_CTLR_COLOR_YUV422;
#endif
    case ESP_CAM_SENSOR_PIXFORMAT_RGB888:
        return CAM_CTLR_COLOR_RGB888;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW8:
        return CAM_CTLR_COLOR_RAW8;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW10:
        return CAM_CTLR_COLOR_RAW10;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW12:
        return CAM_CTLR_COLOR_RAW12;
    case ESP_CAM_SENSOR_PIXFORMAT_YUV420:
        return CAM_CTLR_COLOR_YUV420;
    case ESP_CAM_SENSOR_PIXFORMAT_GRAYSCALE:
        return CAM_CTLR_COLOR_GRAY8;
    default:
        break;
    }

    return 0;
}

esp_err_t esp_video_dvp_enum_format(esp_cam_sensor_output_format_t sensor_fmt, uint32_t index, uint32_t *pixel_format)
{
    ESP_RETURN_ON_FALSE(pixel_format, ESP_ERR_INVALID_ARG, TAG, "pixel_format is NULL");

    uint32_t native_fmt = sensor_to_v4l2_format(sensor_fmt);
    if (native_fmt == 0) {
        ESP_LOGE(TAG, "Unsupported sensor format: %d", sensor_fmt);
        return ESP_ERR_NOT_SUPPORTED;
    }

    uint32_t supported[2];
    uint8_t count = 0;

    supported[count++] = native_fmt;

#if ESP_VIDEO_DVP_DEVICE_CONV_FORMAT
    /**
     * DVP hardware conversion from YUV422 YUYV outputs RGB565 in big-endian byte order.
     * Advertise V4L2_PIX_FMT_RGB565X, not V4L2_PIX_FMT_RGB565 (little-endian).
     */
    if (is_yuv422_yuyv_sensor(sensor_fmt)) {
        supported[count++] = V4L2_PIX_FMT_RGB565X;
    }
#endif

    if (index >= count) {
        return ESP_ERR_INVALID_ARG;
    }

    *pixel_format = supported[index];
    return ESP_OK;
}

esp_err_t esp_video_dvp_check_format(esp_cam_sensor_output_format_t sensor_fmt, uint32_t v4l2_fmt, esp_video_dvp_in_out_format_t *in_out_format)
{
    ESP_RETURN_ON_FALSE(in_out_format, ESP_ERR_INVALID_ARG, TAG, "in_out_format is NULL");
    ESP_RETURN_ON_FALSE(v4l2_fmt, ESP_ERR_INVALID_ARG, TAG, "v4l2_fmt is 0");

    uint32_t native_fmt = sensor_to_v4l2_format(sensor_fmt);
    if (native_fmt == 0) {
        ESP_LOGE(TAG, "Unsupported sensor format: %d", sensor_fmt);
        return ESP_ERR_NOT_SUPPORTED;
    }

    cam_ctlr_color_t in_color = sensor_to_cam_color(sensor_fmt);

    if (v4l2_fmt == native_fmt) {
        in_out_format->in_color = in_color;
        in_out_format->out_color = in_color;
        return ESP_OK;
    }

#if ESP_VIDEO_DVP_DEVICE_CONV_FORMAT
    if (is_yuv422_yuyv_sensor(sensor_fmt) && (v4l2_fmt == V4L2_PIX_FMT_RGB565X)) {
        in_out_format->in_color = in_color;
        in_out_format->out_color = CAM_CTLR_COLOR_RGB565;
        return ESP_OK;
    }
#endif

    ESP_LOGD(TAG, "Requested format %" PRIx32 " is not supported for sensor format %d", v4l2_fmt, sensor_fmt);
    return ESP_ERR_NOT_SUPPORTED;
}
