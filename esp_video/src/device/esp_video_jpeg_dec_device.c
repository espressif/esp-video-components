/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
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
#include "driver/jpeg_decode.h"

#include "esp_video.h"
#include "esp_video_ioctl.h"
#include "esp_video_device_internal.h"

#define JPEG_DEC_NAME                   "JPEG_DEC"

#if CONFIG_SPIRAM
#define JPEG_DEC_MEM_CAPS               (MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM | MALLOC_CAP_CACHE_ALIGNED)
#else
#define JPEG_DEC_MEM_CAPS               (MALLOC_CAP_8BIT | MALLOC_CAP_DMA)
#endif

#define JPEG_DEC_VIDEO_MIN_WIDTH        64
#define JPEG_DEC_VIDEO_MIN_HEIGHT       64

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x)                   (sizeof(x) / sizeof((x)[0]))
#endif

struct jpeg_dec_video {
    bool jpeg_inited;
    jpeg_decoder_handle_t dec_handle;

    jpeg_dec_output_format_t dst_type;
    jpeg_yuv_rgb_conv_std_t conv_std;
};

static const char *TAG = "jpeg_dec_video";

static const uint32_t s_jpeg_dec_capture_format[] = {
    V4L2_PIX_FMT_RGB565,
    V4L2_PIX_FMT_BGR565,
    V4L2_PIX_FMT_RGB24,
    V4L2_PIX_FMT_BGR24,
    V4L2_PIX_FMT_UYVY,
#if ESP_VIDEO_JPEG_DEVICE_YUV420
    V4L2_PIX_FMT_YUV420,
#endif
    V4L2_PIX_FMT_YUV444,
    V4L2_PIX_FMT_GREY,
};

static esp_err_t jpeg_dec_get_output_format_from_v4l2(uint32_t v4l2_format, jpeg_dec_output_format_t *dst_type, uint8_t *dst_bpp)
{
    esp_err_t ret = ESP_OK;

    switch (v4l2_format) {
    case V4L2_PIX_FMT_RGB565:
    case V4L2_PIX_FMT_BGR565:
        *dst_type = JPEG_DECODE_OUT_FORMAT_RGB565;
        *dst_bpp = 16;
        break;
    case V4L2_PIX_FMT_RGB24:
    case V4L2_PIX_FMT_BGR24:
        *dst_type = JPEG_DECODE_OUT_FORMAT_RGB888;
        *dst_bpp = 24;
        break;
    case V4L2_PIX_FMT_UYVY:
        *dst_type = JPEG_DECODE_OUT_FORMAT_YUV422;
        *dst_bpp = 16;
        break;
#if ESP_VIDEO_JPEG_DEVICE_YUV420
    case V4L2_PIX_FMT_YUV420:
        *dst_type = JPEG_DECODE_OUT_FORMAT_YUV420;
        *dst_bpp = 12;
        break;
#endif
    case V4L2_PIX_FMT_YUV444:
        *dst_type = JPEG_DECODE_OUT_FORMAT_YUV444;
        *dst_bpp = 24;
        break;
    case V4L2_PIX_FMT_GREY:
        *dst_type = JPEG_DECODE_OUT_FORMAT_GRAY;
        *dst_bpp = 8;
        break;
    default:
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }

    return ret;
}

static esp_err_t jpeg_dec_video_m2m_process(struct esp_video *video, uint8_t *src, uint32_t src_size, uint8_t *dst, uint32_t dst_size, uint32_t *dst_out_size)
{
    esp_err_t ret;
    uint32_t jpeg_decoded_size;
    struct jpeg_dec_video *jpeg_dec_video = VIDEO_PRIV_DATA(struct jpeg_dec_video *, video);

    if ((M2M_VIDEO_GET_CAPTURE_FORMAT_WIDTH(video) != M2M_VIDEO_GET_OUTPUT_FORMAT_WIDTH(video)) ||
            (M2M_VIDEO_GET_CAPTURE_FORMAT_HEIGHT(video) != M2M_VIDEO_GET_OUTPUT_FORMAT_HEIGHT(video))) {
        ESP_LOGE(TAG, "capture and output width or height is invalid");
        return ESP_ERR_INVALID_ARG;
    }

    jpeg_decode_cfg_t dec_config = {
        .output_format = jpeg_dec_video->dst_type,
        .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_RGB,
        .conv_std = jpeg_dec_video->conv_std,
    };

    uint32_t capture_format = M2M_VIDEO_GET_CAPTURE_FORMAT_PIXEL_FORMAT(video);
    if (capture_format == V4L2_PIX_FMT_BGR565 || capture_format == V4L2_PIX_FMT_BGR24) {
        dec_config.rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR;
    }

    ret = jpeg_decoder_process(jpeg_dec_video->dec_handle,
                               &dec_config,
                               src,
                               src_size,
                               dst,
                               dst_size,
                               &jpeg_decoded_size);
    if (ret == ESP_OK) {
        *dst_out_size = jpeg_decoded_size;
    }

    return ret;
}

static esp_err_t jpeg_dec_video_init(struct esp_video *video)
{
    esp_err_t ret;
    struct jpeg_dec_video *jpeg_dec_video = VIDEO_PRIV_DATA(struct jpeg_dec_video *, video);

    if (!jpeg_dec_video->jpeg_inited) {
        jpeg_decode_engine_cfg_t decode_eng_cfg = {
            .intr_priority = 0,
            .timeout_ms = 40,
        };

        ret = jpeg_new_decoder_engine(&decode_eng_cfg, &jpeg_dec_video->dec_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "failed to create JPEG decoder");
            return ret;
        }
    }

    M2M_VIDEO_SET_OUTPUT_FORMAT(video, JPEG_DEC_VIDEO_MIN_WIDTH, JPEG_DEC_VIDEO_MIN_HEIGHT, V4L2_PIX_FMT_JPEG);
    M2M_VIDEO_SET_CAPTURE_FORMAT(video, JPEG_DEC_VIDEO_MIN_WIDTH, JPEG_DEC_VIDEO_MIN_HEIGHT, V4L2_PIX_FMT_RGB565);

    return ESP_OK;
}

static esp_err_t jpeg_dec_video_deinit(struct esp_video *video)
{
    esp_err_t ret;
    struct jpeg_dec_video *jpeg_dec_video = VIDEO_PRIV_DATA(struct jpeg_dec_video *, video);

    if (!jpeg_dec_video->jpeg_inited) {
        ret = jpeg_del_decoder_engine(jpeg_dec_video->dec_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "failed to delete JPEG decoder");
            return ret;
        }

        jpeg_dec_video->dec_handle = NULL;
    }

    return ESP_OK;
}

static esp_err_t jpeg_dec_video_start(struct esp_video *video, uint32_t type)
{
    if ((M2M_VIDEO_GET_CAPTURE_FORMAT_WIDTH(video) != M2M_VIDEO_GET_OUTPUT_FORMAT_WIDTH(video)) ||
            (M2M_VIDEO_GET_CAPTURE_FORMAT_HEIGHT(video) != M2M_VIDEO_GET_OUTPUT_FORMAT_HEIGHT(video))) {
        ESP_LOGE(TAG, "width or height is invalid");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t jpeg_dec_video_stop(struct esp_video *video, uint32_t type)
{
    return ESP_OK;
}

static esp_err_t jpeg_dec_video_enum_format(struct esp_video *video, uint32_t type, uint32_t index, uint32_t *pixel_format)
{
    if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
        static const uint32_t jpeg_dec_output_format[] = {
            V4L2_PIX_FMT_JPEG,
        };

        if (index >= ARRAY_SIZE(jpeg_dec_output_format)) {
            return ESP_ERR_INVALID_ARG;
        }

        *pixel_format = jpeg_dec_output_format[index];
    } else if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        if (index >= ARRAY_SIZE(s_jpeg_dec_capture_format)) {
            return ESP_ERR_INVALID_ARG;
        }

        *pixel_format = s_jpeg_dec_capture_format[index];
    } else {
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}

static esp_err_t jpeg_dec_video_set_format(struct esp_video *video, const struct v4l2_format *format)
{
    esp_err_t ret;
    const struct v4l2_pix_format *pix = &format->fmt.pix;
    struct jpeg_dec_video *jpeg_dec_video = VIDEO_PRIV_DATA(struct jpeg_dec_video *, video);

    if (format->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
        /**
         * Output data is JPEG image, so width and height are limited by capture image.
         */
        if ((pix->pixelformat != V4L2_PIX_FMT_JPEG) ||
                (pix->width < JPEG_DEC_VIDEO_MIN_WIDTH) ||
                (pix->height < JPEG_DEC_VIDEO_MIN_HEIGHT)) {
            ESP_LOGE(TAG, "pixel format or width or height is invalid");
            return ESP_ERR_INVALID_ARG;
        }
    } else if (format->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        uint8_t output_bpp;
        bool found = false;

        /**
         * Check pixel format is supported
         */
        for (size_t i = 0; i < ARRAY_SIZE(s_jpeg_dec_capture_format); ++i) {
            if (pix->pixelformat == s_jpeg_dec_capture_format[i]) {
                found = true;
                break;
            }
        }
        if (!found) {
            ESP_LOGE(TAG, "unsupported pixel format for capture: " V4L2_FMT_STR, V4L2_FMT_STR_ARG(pix->pixelformat));
            return ESP_ERR_INVALID_ARG;
        }

        /**
         * Capture data is decoded image, so width and height are not limited by output image.
         */
        if ((pix->width < JPEG_DEC_VIDEO_MIN_WIDTH) || (pix->height < JPEG_DEC_VIDEO_MIN_HEIGHT)) {
            ESP_LOGE(TAG, "width or height is invalid");
            return ESP_ERR_INVALID_ARG;
        }

        // Check format->fmt.pix.ycbcr_enc is supported
        if (pix->ycbcr_enc == V4L2_YCBCR_ENC_DEFAULT || pix->ycbcr_enc == V4L2_YCBCR_ENC_601) {
            jpeg_dec_video->conv_std = COLOR_CONV_STD_RGB_YUV_BT601;
        } else if (pix->ycbcr_enc == V4L2_YCBCR_ENC_709) {
            jpeg_dec_video->conv_std = COLOR_CONV_STD_RGB_YUV_BT709;
        } else {
            ESP_LOGE(TAG, "Unsupported ycbcr_enc (%" PRIu32 ")", pix->ycbcr_enc);
            return ESP_ERR_INVALID_ARG;
        }

        ret = jpeg_dec_get_output_format_from_v4l2(pix->pixelformat, &jpeg_dec_video->dst_type, &output_bpp);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "pixel format is invalid");
            return ret;
        }


    } else {
        return ESP_ERR_NOT_SUPPORTED;
    }

    ESP_RETURN_ON_ERROR(esp_video_config_buffer(video, format, JPEG_DEC_MEM_CAPS), TAG, "failed to configure stream buffer");

    return ESP_OK;
}

static esp_err_t jpeg_dec_video_notify(struct esp_video *video, enum esp_video_event event, void *arg)
{
    esp_err_t ret;

    if (event == ESP_VIDEO_M2M_TRIGGER) {
        uint32_t type = *(uint32_t *)arg;

        if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
            ret = esp_video_m2m_process(video,
                                        V4L2_BUF_TYPE_VIDEO_OUTPUT,
                                        V4L2_BUF_TYPE_VIDEO_CAPTURE,
                                        jpeg_dec_video_m2m_process);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "failed to process M2M device data");
                return ret;
            }
        }
    }

    return ESP_OK;
}

static const struct esp_video_ops s_jpeg_dec_video_ops = {
    .init           = jpeg_dec_video_init,
    .deinit         = jpeg_dec_video_deinit,
    .start          = jpeg_dec_video_start,
    .stop           = jpeg_dec_video_stop,
    .enum_format    = jpeg_dec_video_enum_format,
    .set_format     = jpeg_dec_video_set_format,
    .notify         = jpeg_dec_video_notify,
};

/**
 * @brief Create JPEG decode video device
 *
 * @param dec_handle JPEG decoder driver handle,
 *      - NULL, JPEG decode video device will create JPEG decoder driver handle by itself
 *      - Not null, JPEG decode video device will use this handle instead of creating JPEG decoder driver handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_create_jpeg_dec_video_device(jpeg_decoder_handle_t dec_handle)
{
    struct esp_video *video;
    struct jpeg_dec_video *jpeg_dec_video;
    uint32_t device_caps = V4L2_CAP_VIDEO_M2M | V4L2_CAP_EXT_PIX_FORMAT | V4L2_CAP_STREAMING;
    uint32_t caps = device_caps | V4L2_CAP_DEVICE_CAPS;

    jpeg_dec_video = heap_caps_calloc(1, sizeof(struct jpeg_dec_video), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!jpeg_dec_video) {
        return ESP_ERR_NO_MEM;
    }

    if (dec_handle) {
        jpeg_dec_video->jpeg_inited = true;
        jpeg_dec_video->dec_handle = dec_handle;
    }
    jpeg_dec_video->conv_std = COLOR_CONV_STD_RGB_YUV_BT601;
    jpeg_dec_video->dst_type = JPEG_DECODE_OUT_FORMAT_RGB565;

    video = esp_video_create(JPEG_DEC_NAME, ESP_VIDEO_JPEG_DEC_DEVICE_ID, &s_jpeg_dec_video_ops, jpeg_dec_video, caps, device_caps);
    if (!video) {
        heap_caps_free(jpeg_dec_video);
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief Destroy JPEG decode video device
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_destroy_jpeg_dec_video_device(void)
{
    esp_err_t ret;
    struct esp_video *video;
    struct jpeg_dec_video *jpeg_dec_video;

    video = esp_video_device_get_object(JPEG_DEC_NAME);
    if (!video) {
        return ESP_ERR_NOT_FOUND;
    }

    jpeg_dec_video = VIDEO_PRIV_DATA(struct jpeg_dec_video *, video);

    ret = esp_video_destroy(video);
    if (ret != ESP_OK) {
        return ret;
    }

    heap_caps_free(jpeg_dec_video);

    return ESP_OK;
}
