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
#include "img_converters.h"

#define SW_JPEG_NAME                    "JPEG codec"
#define DEFAULT_QUALITY                 16

struct sw_jpeg_video {
    uint8_t quality;
};

static esp_err_t sw_jpeg_video_init(struct esp_video *video)
{
    struct sw_jpeg_video *sw_jpeg_video = VIDEO_PRIV_DATA(struct sw_jpeg_video *, video);

    sw_jpeg_video->quality = DEFAULT_QUALITY;

    return ESP_OK;
}

static esp_err_t sw_jpeg_video_deinit(struct esp_video *video)
{
    return ESP_OK;
}

static esp_err_t sw_jpeg_video_start(struct esp_video *video, uint32_t type)
{
    return ESP_OK;
}

static esp_err_t sw_jpeg_video_stop(struct esp_video *video, uint32_t type)
{
    return ESP_OK;
}

static esp_err_t sw_jpeg_video_set_format(struct esp_video *video, uint32_t type, const struct esp_video_format *format)
{
    uint32_t buf_size;
    struct esp_video_stream *stream = esp_video_get_stream(video, type);

    buf_size = format->width * format->height * format->bpp / 8;

    SET_STREAM_BUF_INFO(stream, buf_size, 1, MALLOC_CAP_8BIT);

    return ESP_OK;
}

static esp_err_t sw_jpeg_video_capability(struct esp_video *video, struct esp_video_capability *capability)
{
    return ESP_OK;
}

static esp_err_t sw_jpeg_video_description(struct esp_video *video, char *buffer, uint32_t size)
{
    int ret;

    ret = snprintf(buffer, size, "Software JPEG codec\n");
    if (ret <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t sw_jpeg_video_notify(struct esp_video *video, enum esp_video_event event, void *arg)
{
    if (event == ESP_VIDEO_M2M_TRIGGER) {
        uint32_t type = *(uint32_t *)arg;

        if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
            struct sw_jpeg_video *sw_jpeg_video = VIDEO_PRIV_DATA(struct sw_jpeg_video *, video);
            struct esp_video_stream *cap_stream = M2M_VIDEO_CAPTURE_STREAM(video);
            struct esp_video_format *cap_format = STREAM_FORMAT(cap_stream);
            struct esp_video_stream *out_stream = M2M_VIDEO_OUTPUT_STREAM(video);
            struct esp_video_format *out_format = STREAM_FORMAT(out_stream);

            if ((cap_format->width != out_format->width) || (cap_format->height != out_format->height)) {
                return ESP_ERR_INVALID_ARG;
            }

            if ((out_format->pixel_format == V4L2_PIX_FMT_RGB565) && (cap_format->pixel_format == V4L2_PIX_FMT_JPEG)) {
                struct esp_video_buffer_element *cap_element;
                struct esp_video_buffer_element *out_element;
                bool codec_ret;
                size_t out_len;

                cap_element = esp_video_get_queued_element(video, V4L2_BUF_TYPE_VIDEO_CAPTURE);
                if (!cap_element) {
                    return ESP_ERR_NO_MEM;
                }

                out_element = esp_video_get_queued_element(video, V4L2_BUF_TYPE_VIDEO_OUTPUT);
                if (!out_element) {
                    esp_video_buffer_queue(cap_stream->buffer, cap_element);
                    return ESP_ERR_NO_MEM;
                }

                codec_ret = fmt2jpg_c(ELEMENT_BUFFER(out_element),
                                      ELEMENT_SIZE(out_element),
                                      out_format->width,
                                      out_format->height,
                                      PIXFORMAT_RGB565,
                                      sw_jpeg_video->quality,
                                      ELEMENT_BUFFER(cap_element),
                                      ELEMENT_SIZE(cap_element),
                                      &out_len);
                if (codec_ret) {
                    cap_element->valid_size = out_len;
                    esp_video_done_element(video, V4L2_BUF_TYPE_VIDEO_CAPTURE, cap_element);
                    esp_video_done_element(video, V4L2_BUF_TYPE_VIDEO_OUTPUT, out_element);
                } else {
                    esp_video_queue_element(video, V4L2_BUF_TYPE_VIDEO_CAPTURE, cap_element);
                    esp_video_queue_element(video, V4L2_BUF_TYPE_VIDEO_OUTPUT, out_element);
                    return ESP_FAIL;
                }
            } else {
                return ESP_ERR_INVALID_ARG;
            }
        }
    }

    return ESP_OK;
}

static const struct esp_video_ops s_sw_jpeg_video_ops = {
    .init          = sw_jpeg_video_init,
    .deinit        = sw_jpeg_video_deinit,
    .start         = sw_jpeg_video_start,
    .stop          = sw_jpeg_video_stop,
    .set_format    = sw_jpeg_video_set_format,
    .capability    = sw_jpeg_video_capability,
    .description   = sw_jpeg_video_description,
    .notify        = sw_jpeg_video_notify,
};

/**
 * @brief Create software JPEG codec video device
 *
 * @param None
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_sw_jpeg_create_video_device(void)
{
    struct esp_video *video;
    struct sw_jpeg_video *sw_jpeg_video;
    uint32_t device_caps = V4L2_CAP_VIDEO_M2M | V4L2_CAP_READWRITE | V4L2_CAP_EXT_PIX_FORMAT |
                           V4L2_CAP_STREAMING;
    uint32_t caps = device_caps | V4L2_CAP_DEVICE_CAPS;

    sw_jpeg_video = heap_caps_malloc(sizeof(struct sw_jpeg_video), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!sw_jpeg_video) {
        return ESP_ERR_NO_MEM;
    }

    video = esp_video_create(SW_JPEG_NAME, NULL, &s_sw_jpeg_video_ops, sw_jpeg_video, caps, device_caps);
    if (!video) {
        heap_caps_free(sw_jpeg_video);
        return ESP_FAIL;
    }

    return ESP_OK;
}
