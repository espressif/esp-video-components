/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VIDEO_PRIV_DATA(t, v)               ((t)(v)->priv)

#define STREAM_FORMAT(s)                    (&(s)->format)
#define STREAM_BUF_INFO(s)                  (&(s)->buf_info)

#define STREAM_BUFFER_SIZE(s)               (STREAM_BUF_INFO(s)->size)

#define SET_BUF_INFO(bi, s, a, c)           \
{                                           \
    (bi)->size = (s);                       \
    (bi)->align_size = (a);                 \
    (bi)->caps = (c);                       \
}

#define SET_FORMAT_WIDTH(fmt, _width)                                   \
{                                                                       \
    (fmt)->width = (_width);                                            \
}

#define SET_FORMAT_HEIGHT(fmt, _height)                                 \
{                                                                       \
    (fmt)->height = (_height);                                          \
}

#define SET_FORMAT_PIXEL_FORMAT(fmt, _pixel_format)                     \
{                                                                       \
    (fmt)->pixel_format = (_pixel_format);                              \
}

#define GET_FORMAT_WIDTH(fmt)                                           \
    ((fmt)->width)

#define GET_FORMAT_HEIGHT(fmt)                                          \
    ((fmt)->height)

#define GET_FORMAT_PIXEL_FORMAT(fmt)                                    \
    ((fmt)->pixel_format)

#define SET_STREAM_BUF_INFO(st, s, a, c)                                \
    SET_BUF_INFO(STREAM_BUF_INFO(st), s, a, c)

#define SET_STREAM_FORMAT_WIDTH(st, w)                                  \
    SET_FORMAT_WIDTH(STREAM_FORMAT(st), w)

#define SET_STREAM_FORMAT_HEIGHT(st, h)                                 \
    SET_FORMAT_HEIGHT(STREAM_FORMAT(st), h)

#define SET_STREAM_FORMAT_PIXEL_FORMAT(st, f)                           \
    SET_FORMAT_PIXEL_FORMAT(STREAM_FORMAT(st), f)

#define GET_STREAM_FORMAT_WIDTH(st)                                     \
    GET_FORMAT_WIDTH(STREAM_FORMAT(st))

#define GET_STREAM_FORMAT_HEIGHT(st)                                    \
    GET_FORMAT_HEIGHT(STREAM_FORMAT(st))

#define GET_STREAM_FORMAT_PIXEL_FORMAT(st)                              \
    GET_FORMAT_PIXEL_FORMAT(STREAM_FORMAT(st))

#define CAPTURE_VIDEO_STREAM(v)             ((v)->stream)
#define CAPTURE_VIDEO_BUF_SIZE(v)           STREAM_BUFFER_SIZE(CAPTURE_VIDEO_STREAM(v))

#define CAPTURE_VIDEO_DONE_BUF(v, b, n)     esp_video_done_buffer(v, V4L2_BUF_TYPE_VIDEO_CAPTURE, b, n)

#define CAPTURE_VIDEO_SET_FORMAT_WIDTH(v, w)                            \
    SET_STREAM_FORMAT_WIDTH(CAPTURE_VIDEO_STREAM(v), w)

#define CAPTURE_VIDEO_SET_FORMAT_HEIGHT(v, h)                           \
    SET_STREAM_FORMAT_HEIGHT(CAPTURE_VIDEO_STREAM(v), h)

#define CAPTURE_VIDEO_SET_FORMAT_PIXEL_FORMAT(v, f)                     \
    SET_STREAM_FORMAT_PIXEL_FORMAT(CAPTURE_VIDEO_STREAM(v), f)

#define CAPTURE_VIDEO_GET_FORMAT_WIDTH(v)                               \
    GET_STREAM_FORMAT_WIDTH(CAPTURE_VIDEO_STREAM(v))

#define CAPTURE_VIDEO_GET_FORMAT_HEIGHT(v)                              \
    GET_STREAM_FORMAT_HEIGHT(CAPTURE_VIDEO_STREAM(v))

#define CAPTURE_VIDEO_GET_FORMAT_PIXEL_FORMAT(v)                        \
    GET_STREAM_FORMAT_PIXEL_FORMAT(CAPTURE_VIDEO_STREAM(v))


#define CAPTURE_VIDEO_SET_FORMAT(v, w, h, f)                            \
{                                                                       \
    CAPTURE_VIDEO_SET_FORMAT_WIDTH(v, w);                               \
    CAPTURE_VIDEO_SET_FORMAT_HEIGHT(v, h);                              \
    CAPTURE_VIDEO_SET_FORMAT_PIXEL_FORMAT(v, f);                        \
}

#define CAPTURE_VIDEO_SET_BUF_INFO(v, s, a, c)                          \
    SET_STREAM_BUF_INFO(CAPTURE_VIDEO_STREAM(v), s, a, c)

#define CAPTURE_VIDEO_GET_QUEUED_BUF(v)                                 \
    esp_video_get_queued_buffer(v, V4L2_BUF_TYPE_VIDEO_CAPTURE);

#define CAPTURE_VIDEO_QUEUE_ELEMENT(v, e)                               \
    esp_video_queue_element(v, V4L2_BUF_TYPE_VIDEO_CAPTURE, e)

#define CAPTURE_VIDEO_GET_QUEUED_ELEMENT(v)                             \
    esp_video_get_queued_element(v, V4L2_BUF_TYPE_VIDEO_CAPTURE)

#define M2M_VIDEO_CAPTURE_STREAM(v)         (&(v)->stream[0])
#define M2M_VIDEO_OUTPUT_STREAM(v)          (&(v)->stream[1])

/**
 * @brief Video event.
 */
enum esp_video_event {
    ESP_VIDEO_BUFFER_VALID = 0,     /*!< Video buffer is freed and it can be allocated by video device */
    ESP_VIDEO_M2M_TRIGGER,          /*!< Trigger M2M video device transforming event */
};

struct esp_video;
struct esp_video_format;

/**
 * @brief Video operations object.
 */
struct esp_video_ops {

    /*!< Initializa video hardware and allocate software resource, and must set buffer information and video format */

    esp_err_t (*init)(struct esp_video *video);

    /*!< De-initializa video hardware and free software resource */

    esp_err_t (*deinit)(struct esp_video *video);

    /*!< Start data stream */

    esp_err_t (*start)(struct esp_video *video, uint32_t type);

    /*!< Start data stream */

    esp_err_t (*stop)(struct esp_video *video, uint32_t type);

    /*!< Enumerate video format description */

    esp_err_t (*enum_format)(struct esp_video *video, uint32_t type, uint32_t index, uint32_t *pixel_format);

    /*!< Set video format configuration */

    esp_err_t (*set_format)(struct esp_video *video, uint32_t type, const struct esp_video_format *format);

    /*!< Notify driver event triggers */

    esp_err_t (*notify)(struct esp_video *video, enum esp_video_event event, void *arg);

    /*!< Set external control value */

    esp_err_t (*set_ext_ctrl)(struct esp_video *video, const struct v4l2_ext_controls *ctrls);

    /*!< Get external control value */

    esp_err_t (*get_ext_ctrl)(struct esp_video *video, struct v4l2_ext_controls *ctrls);

    /*!< Query external control descrption */

    esp_err_t (*query_ext_ctrl)(struct esp_video *video, struct v4l2_query_ext_ctrl *qctrl);
};

#ifdef __cplusplus
}
#endif
