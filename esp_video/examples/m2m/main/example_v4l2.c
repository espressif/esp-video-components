/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "esp_log.h"
#include "esp_check.h"
#include "linux/videodev2.h"
#include "example_video_common.h"
#include "example_v4l2.h"

#if CONFIG_EXAMPLE_M2M_MODE_ENCODE || CONFIG_EXAMPLE_M2M_MODE_DECODE || CONFIG_EXAMPLE_M2M_MODE_PIPELINE
#include "esp_video_device.h"
#endif

#define M2M_USE_JPEG_DECODER      (CONFIG_EXAMPLE_M2M_MODE_DECODE || CONFIG_EXAMPLE_M2M_MODE_PIPELINE)
#define M2M_USE_JPEG_ENCODER      (CONFIG_EXAMPLE_M2M_MODE_ENCODE && CONFIG_ESP_VIDEO_ENABLE_JPEG_ENC_VIDEO_DEVICE)
#define M2M_USE_H264_ENCODER      ((CONFIG_EXAMPLE_M2M_MODE_ENCODE && CONFIG_ESP_VIDEO_ENABLE_H264_VIDEO_DEVICE) || CONFIG_EXAMPLE_M2M_MODE_PIPELINE)
#define M2M_USE_ENCODE_FRAME        (M2M_USE_JPEG_ENCODER || M2M_USE_H264_ENCODER)
#define M2M_USE_V4L2_CAPTURE_FORMAT (CONFIG_EXAMPLE_M2M_MODE_DECODE || CONFIG_EXAMPLE_M2M_MODE_PIPELINE)

#define CAP_BUFFER_COUNT         2
#define M2M_BUFFER_COUNT         1
#define SKIP_STARTUP_FRAME_COUNT 2
#define FORMAT_DESC_SIZE         32
#define MAX_FORMATS              16
#define V4L2_STREAM_MAX_BUFFERS  2

static const char *TAG = "example_v4l2";

typedef struct {
    uint32_t pixelformat;
    uint32_t width;
    uint32_t height;
    uint32_t fps_numerator;
    uint32_t fps_denominator;
    char description[FORMAT_DESC_SIZE];
} format_info_t;

typedef struct {
    int fd;
    uint32_t buf_type;
    uint32_t memory;
    uint32_t buffer_count;
    uint8_t *buffers[V4L2_STREAM_MAX_BUFFERS];
    uint32_t buffer_lengths[V4L2_STREAM_MAX_BUFFERS];
    bool mmap_buffer[V4L2_STREAM_MAX_BUFFERS];
    bool streaming;
} v4l2_stream_t;

typedef struct example_camera_ctx {
    int fd;
    v4l2_stream_t stream;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    bool opened;
} example_camera_ctx;

typedef struct example_jpeg_ctx {
    int fd;
    v4l2_stream_t output;
    v4l2_stream_t capture;
    uint32_t width;
    uint32_t height;
    uint32_t input_format;
    uint32_t output_format;
    bool opened;
    bool connected;
    bool is_decoder;
} example_jpeg_ctx;

typedef struct example_h264_ctx {
    int fd;
    v4l2_stream_t output;
    v4l2_stream_t capture;
    uint32_t width;
    uint32_t height;
    uint32_t input_format;
    bool opened;
    bool connected;
} example_h264_ctx;

static example_camera_ctx s_camera;
#if M2M_USE_JPEG_DECODER
static example_jpeg_ctx s_jpeg_decoder;
#endif
#if M2M_USE_JPEG_ENCODER
static example_jpeg_ctx s_jpeg_encoder;
#endif
#if M2M_USE_H264_ENCODER
static example_h264_ctx s_h264;
#endif
static bool s_video_inited;

static bool frame_size_is_discrete(const struct v4l2_frmsizeenum *frmsize)
{
    if (frmsize->type != V4L2_FRMSIZE_TYPE_DISCRETE) {
        ESP_LOGW(TAG, "unsupported frame size enum type=%" PRIu32, frmsize->type);
        return false;
    }

    return true;
}

static bool frame_interval_is_discrete(const struct v4l2_frmivalenum *frmival)
{
    if (frmival->type != V4L2_FRMIVAL_TYPE_DISCRETE) {
        ESP_LOGW(TAG, "unsupported frame interval enum type=%" PRIu32, frmival->type);
        return false;
    }

    return true;
}

static esp_err_t add_capture_format(format_info_t *formats, int max_count, int *count,
                                    const struct v4l2_fmtdesc *fmtdesc,
                                    uint32_t width, uint32_t height,
                                    uint32_t numerator, uint32_t denominator)
{
    if (*count >= max_count) {
        ESP_LOGW(TAG, "capture format list is full");
        return ESP_OK;
    }

    format_info_t *format = &formats[(*count)++];

    format->pixelformat = fmtdesc->pixelformat;
    format->width = width;
    format->height = height;
    format->fps_numerator = numerator;
    format->fps_denominator = denominator;
    strncpy(format->description, (const char *)fmtdesc->description, sizeof(format->description) - 1);

    return ESP_OK;
}

static esp_err_t v4l2_open_device(const char *path, int flags, int *fd)
{
    ESP_RETURN_ON_FALSE(path && fd, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    *fd = open(path, flags);
    ESP_RETURN_ON_FALSE(*fd >= 0, ESP_FAIL, TAG, "failed to open %s", path);

    return ESP_OK;
}

static void v4l2_close_device(int *fd)
{
    if (fd && *fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}

static esp_err_t v4l2_query_and_log(int fd, const char *label)
{
    struct v4l2_capability capability;

    ESP_RETURN_ON_FALSE(ioctl(fd, VIDIOC_QUERYCAP, &capability) == 0, ESP_FAIL, TAG, "%s QUERYCAP failed", label);
    ESP_LOGI(TAG, "%s:", label);
    ESP_LOGI(TAG, "version: %d.%d.%d", (uint16_t)(capability.version >> 16),
             (uint8_t)(capability.version >> 8),
             (uint8_t)capability.version);
    ESP_LOGI(TAG, "driver:  %s", capability.driver);
    ESP_LOGI(TAG, "card:    %s", capability.card);
    ESP_LOGI(TAG, "bus:     %s", capability.bus_info);

    return ESP_OK;
}

#if M2M_USE_JPEG_ENCODER || M2M_USE_H264_ENCODER
static esp_err_t v4l2_set_ext_ctrl(int fd, uint32_t ctrl_class, uint32_t id, int32_t value)
{
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];

    controls.ctrl_class = ctrl_class;
    controls.count = 1;
    controls.controls = control;
    control[0].id = id;
    control[0].value = value;

    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG, "failed to set control: %" PRIu32, id);
        return ESP_FAIL;
    }

    return ESP_OK;
}
#endif

static esp_err_t v4l2_enum_capture_formats(int fd, format_info_t *formats, int max_count, int *out_count)
{
    const int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ESP_RETURN_ON_FALSE(formats && out_count, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    *out_count = 0;

    for (int fmt_index = 0; ; fmt_index++) {
        struct v4l2_fmtdesc fmtdesc = {
            .index = fmt_index,
            .type = type,
        };

        if (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) != 0) {
            break;
        }

        struct v4l2_frmsizeenum frmsize = {
            .index = 0,
            .pixel_format = fmtdesc.pixelformat,
        };
        if (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0) {
            for (int fmt_frmsize_index = 0; ; fmt_frmsize_index++) {
                memset(&frmsize, 0, sizeof(frmsize));
                frmsize.index = fmt_frmsize_index;
                frmsize.pixel_format = fmtdesc.pixelformat;
                if (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) != 0) {
                    break;
                }
                if (!frame_size_is_discrete(&frmsize)) {
                    break;
                }

                struct v4l2_frmivalenum frmival = {
                    .index = 0,
                    .pixel_format = fmtdesc.pixelformat,
                    .width = frmsize.discrete.width,
                    .height = frmsize.discrete.height,
                };
                if (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) == 0) {
                    for (int fmt_frmival_index = 0; ; fmt_frmival_index++) {
                        memset(&frmival, 0, sizeof(frmival));
                        frmival.index = fmt_frmival_index;
                        frmival.pixel_format = fmtdesc.pixelformat;
                        frmival.width = frmsize.discrete.width;
                        frmival.height = frmsize.discrete.height;
                        if (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) != 0) {
                            break;
                        }
                        if (!frame_interval_is_discrete(&frmival)) {
                            break;
                        }

                        ESP_RETURN_ON_ERROR(add_capture_format(formats, max_count, out_count, &fmtdesc,
                                                               frmsize.discrete.width, frmsize.discrete.height,
                                                               frmival.discrete.numerator,
                                                               frmival.discrete.denominator),
                                            TAG, "failed to add capture format");
                    }
                } else {
                    struct v4l2_streamparm sparm = {
                        .type = type,
                        .parm.capture.capability = V4L2_CAP_TIMEPERFRAME,
                    };
                    struct v4l2_captureparm *cparam = &sparm.parm.capture;

                    ESP_RETURN_ON_FALSE(ioctl(fd, VIDIOC_G_PARM, &sparm) == 0, ESP_FAIL, TAG,
                                        "failed to get stream parameter");

                    ESP_RETURN_ON_ERROR(add_capture_format(formats, max_count, out_count, &fmtdesc,
                                                           frmsize.discrete.width, frmsize.discrete.height,
                                                           cparam->timeperframe.numerator,
                                                           cparam->timeperframe.denominator),
                                        TAG, "failed to add capture format");
                }
            }
        } else {
            struct v4l2_format format = {
                .type = type,
            };
            struct v4l2_streamparm sparm = {
                .type = type,
                .parm.capture.capability = V4L2_CAP_TIMEPERFRAME,
            };
            struct v4l2_captureparm *cparam = &sparm.parm.capture;

            ESP_RETURN_ON_FALSE(ioctl(fd, VIDIOC_G_PARM, &sparm) == 0, ESP_FAIL, TAG,
                                "failed to get stream parameter");
            ESP_RETURN_ON_FALSE(ioctl(fd, VIDIOC_G_FMT, &format) == 0, ESP_FAIL, TAG,
                                "failed to get format");

            ESP_RETURN_ON_ERROR(add_capture_format(formats, max_count, out_count, &fmtdesc,
                                                   format.fmt.pix.width, format.fmt.pix.height,
                                                   cparam->timeperframe.numerator,
                                                   cparam->timeperframe.denominator),
                                TAG, "failed to add capture format");
        }
    }

    ESP_RETURN_ON_FALSE(*out_count > 0, ESP_FAIL, TAG, "no supported format found");

    return ESP_OK;
}

#if CONFIG_EXAMPLE_M2M_MODE_ENCODE
static void v4l2_log_capture_formats(const format_info_t *formats, int count)
{
    for (int i = 0; i < count; i++) {
        const format_info_t *format = &formats[i];

        ESP_LOGI(TAG, "format: %s, frame size: %" PRIu32 "x%" PRIu32 ", FPS: %0.1f",
                 format->description, format->width, format->height,
                 (double)format->fps_denominator / (double)format->fps_numerator);
    }
}
#endif

#if M2M_USE_V4L2_CAPTURE_FORMAT
static esp_err_t v4l2_set_capture_format(int fd, const format_info_t *fmt)
{
    struct v4l2_format format = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .fmt.pix.width = fmt->width,
        .fmt.pix.height = fmt->height,
        .fmt.pix.pixelformat = fmt->pixelformat,
    };
    struct v4l2_streamparm sparm = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .parm.capture.capability = V4L2_CAP_TIMEPERFRAME,
        .parm.capture.timeperframe.numerator = fmt->fps_numerator,
        .parm.capture.timeperframe.denominator = fmt->fps_denominator,
    };

    ESP_RETURN_ON_FALSE(ioctl(fd, VIDIOC_S_FMT, &format) == 0, ESP_FAIL, TAG,
                        "failed to set capture format");
    ESP_RETURN_ON_FALSE(ioctl(fd, VIDIOC_S_PARM, &sparm) == 0, ESP_FAIL, TAG,
                        "failed to set capture stream parameter");

    return ESP_OK;
}
#endif

static esp_err_t v4l2_set_pix_format(int fd, uint32_t buf_type, uint32_t width,
                                     uint32_t height, uint32_t pixelformat)
{
    struct v4l2_format format = {
        .type = buf_type,
        .fmt.pix.width = width,
        .fmt.pix.height = height,
        .fmt.pix.pixelformat = pixelformat,
    };

    ESP_RETURN_ON_FALSE(ioctl(fd, VIDIOC_S_FMT, &format) == 0, ESP_FAIL, TAG,
                        "failed to set format type=%" PRIu32, buf_type);

    return ESP_OK;
}

static esp_err_t alloc_stream_buffers(v4l2_stream_t *stream)
{
    struct v4l2_requestbuffers req = {
        .count = stream->buffer_count,
        .type = stream->buf_type,
        .memory = stream->memory,
    };

    ESP_RETURN_ON_FALSE(stream->buffer_count <= V4L2_STREAM_MAX_BUFFERS, ESP_ERR_INVALID_ARG, TAG,
                        "too many stream buffers");
    ESP_RETURN_ON_FALSE(ioctl(stream->fd, VIDIOC_REQBUFS, &req) == 0, ESP_FAIL, TAG,
                        "failed to request buffers");

    for (uint32_t i = 0; i < stream->buffer_count; i++) {
        struct v4l2_buffer buf = {
            .type = stream->buf_type,
            .memory = stream->memory,
            .index = i,
        };

        ESP_RETURN_ON_FALSE(ioctl(stream->fd, VIDIOC_QUERYBUF, &buf) == 0, ESP_FAIL, TAG,
                            "failed to query buffer");
        stream->buffer_lengths[i] = buf.length;

        if (stream->memory == V4L2_MEMORY_MMAP) {
            stream->buffers[i] = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                                 MAP_SHARED, stream->fd, buf.m.offset);
            ESP_RETURN_ON_FALSE(stream->buffers[i] != MAP_FAILED, ESP_ERR_NO_MEM, TAG, "failed to mmap buffer");
            stream->mmap_buffer[i] = true;
        }
    }

    return ESP_OK;
}

static esp_err_t v4l2_stream_init(v4l2_stream_t *stream, int fd, uint32_t buf_type,
                                  uint32_t memory, uint32_t buffer_count)
{
    ESP_RETURN_ON_FALSE(stream, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    memset(stream, 0, sizeof(*stream));
    stream->fd = fd;
    stream->buf_type = buf_type;
    stream->memory = memory;
    stream->buffer_count = buffer_count;

    return alloc_stream_buffers(stream);
}

static void free_stream_buffer(v4l2_stream_t *stream, uint32_t index)
{
    if (!stream->buffers[index]) {
        return;
    }

    if (stream->mmap_buffer[index]) {
        struct v4l2_buffer buf = {
            .type = stream->buf_type,
            .memory = stream->memory,
            .index = index,
        };

        if (ioctl(stream->fd, VIDIOC_QUERYBUF, &buf) == 0) {
            ESP_LOGE(TAG, "failed to query buffer");
        }
        munmap(stream->buffers[index], buf.length);
        stream->mmap_buffer[index] = false;
    }

    stream->buffers[index] = NULL;
    stream->buffer_lengths[index] = 0;
}

static void v4l2_stream_deinit(v4l2_stream_t *stream)
{
    if (!stream) {
        return;
    }

    if (stream->streaming) {
        int type = stream->buf_type;
        ioctl(stream->fd, VIDIOC_STREAMOFF, &type);
        stream->streaming = false;
    }

    for (uint32_t i = 0; i < stream->buffer_count; i++) {
        free_stream_buffer(stream, i);
    }

    stream->buffer_count = 0;
    stream->fd = -1;
}

static void prepare_buffer_for_queue(v4l2_stream_t *stream, struct v4l2_buffer *buf)
{
    if (stream->memory == V4L2_MEMORY_USERPTR) {
        buf->m.userptr = (unsigned long)stream->buffers[buf->index];
        buf->length = stream->buffer_lengths[buf->index];
    }
}

static esp_err_t v4l2_stream_queue_all(v4l2_stream_t *stream)
{
    ESP_RETURN_ON_FALSE(stream, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    for (uint32_t i = 0; i < stream->buffer_count; i++) {
        struct v4l2_buffer buf = {
            .type = stream->buf_type,
            .memory = stream->memory,
            .index = i,
        };

        prepare_buffer_for_queue(stream, &buf);
        ESP_RETURN_ON_FALSE(ioctl(stream->fd, VIDIOC_QBUF, &buf) == 0, ESP_FAIL, TAG,
                            "failed to queue buffer");
    }

    return ESP_OK;
}

static esp_err_t v4l2_stream_dequeue(v4l2_stream_t *stream, struct v4l2_buffer *buf)
{
    ESP_RETURN_ON_FALSE(stream && buf, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    memset(buf, 0, sizeof(*buf));
    buf->type = stream->buf_type;
    buf->memory = stream->memory;

    ESP_RETURN_ON_FALSE(ioctl(stream->fd, VIDIOC_DQBUF, buf) == 0, ESP_FAIL, TAG,
                        "failed to dequeue buffer");

    return ESP_OK;
}

static esp_err_t v4l2_stream_queue_buffer(v4l2_stream_t *stream, struct v4l2_buffer *buf)
{
    ESP_RETURN_ON_FALSE(stream && buf, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    prepare_buffer_for_queue(stream, buf);
    ESP_RETURN_ON_FALSE(ioctl(stream->fd, VIDIOC_QBUF, buf) == 0, ESP_FAIL, TAG,
                        "failed to queue buffer");

    return ESP_OK;
}

static esp_err_t v4l2_stream_start(v4l2_stream_t *stream)
{
    ESP_RETURN_ON_FALSE(stream, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    int type = stream->buf_type;
    ESP_RETURN_ON_FALSE(ioctl(stream->fd, VIDIOC_STREAMON, &type) == 0, ESP_FAIL, TAG,
                        "failed to start stream");
    stream->streaming = true;

    return ESP_OK;
}

static esp_err_t v4l2_stream_stop(v4l2_stream_t *stream)
{
    int type;

    if (!stream || !stream->streaming) {
        return ESP_OK;
    }

    type = stream->buf_type;
    ioctl(stream->fd, VIDIOC_STREAMOFF, &type);
    stream->streaming = false;

    return ESP_OK;
}

static esp_err_t v4l2_stream_warmup(v4l2_stream_t *stream, int frame_count)
{
    ESP_RETURN_ON_FALSE(stream, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    for (int i = 0; i < frame_count; i++) {
        struct v4l2_buffer buf;

        ESP_RETURN_ON_ERROR(v4l2_stream_dequeue(stream, &buf), TAG, "warmup dequeue failed");
        ESP_RETURN_ON_ERROR(v4l2_stream_queue_buffer(stream, &buf), TAG, "warmup queue failed");
    }

    return ESP_OK;
}

static uint8_t *v4l2_stream_buffer_ptr(const v4l2_stream_t *stream, uint32_t index)
{
    if (!stream || index >= stream->buffer_count) {
        return NULL;
    }

    return stream->buffers[index];
}

#if CONFIG_EXAMPLE_M2M_MODE_ENCODE
#if CONFIG_ESP_VIDEO_ENABLE_JPEG_ENC_VIDEO_DEVICE
#define M2M_OUTPUT_FORMAT V4L2_PIX_FMT_JPEG
#else
#define M2M_OUTPUT_FORMAT V4L2_PIX_FMT_H264
#endif

static esp_err_t detect_encoder_capture_format(int fd, uint32_t m2m_output_format,
        uint32_t *width, uint32_t *height, uint32_t *capture_format)
{
    struct v4l2_format init_format = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
    };

    ESP_RETURN_ON_FALSE(ioctl(fd, VIDIOC_G_FMT, &init_format) == 0, ESP_FAIL, TAG,
                        "failed to get capture format");

    *width = init_format.fmt.pix.width;
    *height = init_format.fmt.pix.height;
    *capture_format = 0;

    if (m2m_output_format == V4L2_PIX_FMT_JPEG) {
        const uint32_t jpeg_input_formats[] = {
            V4L2_PIX_FMT_RGB565,
            V4L2_PIX_FMT_UYVY,
            V4L2_PIX_FMT_RGB24,
            V4L2_PIX_FMT_GREY,
        };

        for (int fmt_index = 0; !*capture_format; fmt_index++) {
            struct v4l2_fmtdesc fmtdesc = {
                .index = fmt_index,
                .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            };

            if (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) != 0) {
                break;
            }

            for (int i = 0; i < sizeof(jpeg_input_formats) / sizeof(jpeg_input_formats[0]); i++) {
                if (jpeg_input_formats[i] == fmtdesc.pixelformat) {
                    *capture_format = jpeg_input_formats[i];
                    break;
                }
            }
        }

        ESP_RETURN_ON_FALSE(*capture_format, ESP_ERR_NOT_SUPPORTED, TAG,
                            "no JPEG encoder compatible capture format");
    } else {
        *capture_format = V4L2_PIX_FMT_YUV420;
    }

    return ESP_OK;
}
#endif

static esp_err_t setup_camera_format(const example_camera_config_t *config, format_info_t *selected_format)
{
    format_info_t formats[MAX_FORMATS];
    int format_count = 0;

    ESP_RETURN_ON_ERROR(v4l2_enum_capture_formats(s_camera.fd, formats, MAX_FORMATS, &format_count),
                        TAG, "enum capture formats failed");

#if CONFIG_EXAMPLE_M2M_MODE_DECODE || CONFIG_EXAMPLE_M2M_MODE_PIPELINE
    *selected_format = formats[0];
    s_camera.width = selected_format->width;
    s_camera.height = selected_format->height;
    s_camera.format = selected_format->pixelformat;

    ESP_RETURN_ON_FALSE(s_camera.format == V4L2_PIX_FMT_JPEG, ESP_ERR_NOT_SUPPORTED, TAG,
                        "capture format is not JPEG, decode test requires MJPEG input");

    if (config->width && config->height) {
        s_camera.width = config->width;
        s_camera.height = config->height;
        selected_format->width = config->width;
        selected_format->height = config->height;
    }
    if (config->format) {
        s_camera.format = config->format;
        selected_format->pixelformat = config->format;
    }

    ESP_RETURN_ON_ERROR(v4l2_set_capture_format(s_camera.fd, selected_format), TAG,
                        "set capture format failed");
#elif CONFIG_EXAMPLE_M2M_MODE_ENCODE
    (void)selected_format;

    ESP_LOGI(TAG, "Capture device formats:");
    v4l2_log_capture_formats(formats, format_count);

    ESP_RETURN_ON_ERROR(detect_encoder_capture_format(s_camera.fd, M2M_OUTPUT_FORMAT,
                        &s_camera.width, &s_camera.height,
                        &s_camera.format),
                        TAG, "detect capture format failed");

    if (config->width && config->height) {
        s_camera.width = config->width;
        s_camera.height = config->height;
    }
    if (config->format) {
        s_camera.format = config->format;
    }

    ESP_RETURN_ON_ERROR(v4l2_set_pix_format(s_camera.fd, V4L2_BUF_TYPE_VIDEO_CAPTURE,
                                            s_camera.width, s_camera.height, s_camera.format),
                        TAG, "set capture format failed");
#endif

    return ESP_OK;
}

static esp_err_t setup_camera_stream(void)
{
    ESP_RETURN_ON_ERROR(v4l2_stream_init(&s_camera.stream, s_camera.fd, V4L2_BUF_TYPE_VIDEO_CAPTURE,
                                         V4L2_MEMORY_MMAP, CAP_BUFFER_COUNT),
                        TAG, "init capture stream failed");
    ESP_RETURN_ON_ERROR(v4l2_stream_queue_all(&s_camera.stream), TAG, "queue capture buffers failed");
    ESP_RETURN_ON_ERROR(v4l2_stream_start(&s_camera.stream), TAG, "start capture failed");
    return v4l2_stream_warmup(&s_camera.stream, SKIP_STARTUP_FRAME_COUNT);
}

#if M2M_USE_ENCODE_FRAME
static esp_err_t m2m_encode_frame(int fd, v4l2_stream_t *output, v4l2_stream_t *capture,
                                  const example_image_t *input, example_image_t *output_image,
                                  uint32_t output_format)
{
    struct v4l2_buffer out_buf = {
        .index = 0,
        .type = V4L2_BUF_TYPE_VIDEO_OUTPUT,
        .memory = V4L2_MEMORY_USERPTR,
        .m.userptr = (unsigned long)input->data,
        .length = input->size,
    };
    struct v4l2_buffer cap_buf;

    ESP_RETURN_ON_FALSE(ioctl(fd, VIDIOC_QBUF, &out_buf) == 0, ESP_FAIL, TAG,
                        "queue codec output buffer failed");
    ESP_RETURN_ON_ERROR(v4l2_stream_dequeue(capture, &cap_buf), TAG, "dequeue encoded frame failed");
    ESP_RETURN_ON_ERROR(v4l2_stream_dequeue(output, &out_buf), TAG, "dequeue codec output buffer failed");

    if (!(cap_buf.flags & V4L2_BUF_FLAG_DONE) || cap_buf.bytesused == 0) {
        cap_buf.index = 0;
        cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        cap_buf.memory = V4L2_MEMORY_MMAP;
        ESP_RETURN_ON_ERROR(v4l2_stream_queue_buffer(capture, &cap_buf), TAG, "requeue encoded frame failed");
        return ESP_ERR_INVALID_STATE;
    }

    output_image->width = input->width;
    output_image->height = input->height;
    output_image->format = output_format;
    output_image->data = v4l2_stream_buffer_ptr(capture, cap_buf.index);
    output_image->size = cap_buf.bytesused;
    output_image->buf_index = cap_buf.index;

    return ESP_OK;
}
#endif

#if M2M_USE_JPEG_DECODER
static bool is_valid_jpeg_frame(const uint8_t *data, uint32_t size)
{
    return data && size >= 2 && data[0] == 0xff && data[1] == 0xd8;
}

static esp_err_t m2m_decode_frame(example_jpeg_ctx *ctx, const example_image_t *input, example_image_t *output_image)
{
    struct v4l2_buffer out_buf = {
        .index = 0,
        .type = V4L2_BUF_TYPE_VIDEO_OUTPUT,
        .memory = V4L2_MEMORY_USERPTR,
        .m.userptr = (unsigned long)input->data,
        .length = input->size,
    };
    struct v4l2_buffer cap_buf;

    ESP_RETURN_ON_FALSE(is_valid_jpeg_frame(input->data, input->size), ESP_ERR_INVALID_STATE, TAG,
                        "invalid JPEG frame");

    ESP_RETURN_ON_FALSE(ioctl(ctx->fd, VIDIOC_QBUF, &out_buf) == 0, ESP_FAIL, TAG,
                        "queue decode input buffer failed");
    ESP_RETURN_ON_ERROR(v4l2_stream_dequeue(&ctx->capture, &cap_buf), TAG, "dequeue decoded frame failed");
    ESP_RETURN_ON_ERROR(v4l2_stream_dequeue(&ctx->output, &out_buf), TAG, "dequeue decode input buffer failed");

    if (!(cap_buf.flags & V4L2_BUF_FLAG_DONE) || cap_buf.bytesused == 0) {
        cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        cap_buf.memory = V4L2_MEMORY_MMAP;
        ESP_RETURN_ON_ERROR(v4l2_stream_queue_buffer(&ctx->capture, &cap_buf), TAG,
                            "requeue decode output buffer failed");
        return ESP_ERR_INVALID_STATE;
    }

    output_image->width = ctx->width;
    output_image->height = ctx->height;
    output_image->format = ctx->output_format;
    output_image->data = v4l2_stream_buffer_ptr(&ctx->capture, cap_buf.index);
    output_image->size = cap_buf.bytesused;
    output_image->buf_index = cap_buf.index;
    output_image->buf_owner = EXAMPLE_BUF_OWNER_M2M_JPEG_DEC;

    return ESP_OK;
}
#endif

#if M2M_USE_JPEG_DECODER
static esp_err_t setup_jpeg_decoder_streams(example_jpeg_ctx *jpeg)
{
    ESP_RETURN_ON_ERROR(v4l2_set_pix_format(jpeg->fd, V4L2_BUF_TYPE_VIDEO_OUTPUT,
                                            jpeg->width, jpeg->height, jpeg->input_format),
                        TAG, "set decode input format failed");
    ESP_RETURN_ON_ERROR(v4l2_stream_init(&jpeg->output, jpeg->fd, V4L2_BUF_TYPE_VIDEO_OUTPUT,
                                         V4L2_MEMORY_USERPTR, M2M_BUFFER_COUNT),
                        TAG, "init decode input stream failed");
    ESP_RETURN_ON_ERROR(v4l2_set_pix_format(jpeg->fd, V4L2_BUF_TYPE_VIDEO_CAPTURE,
                                            jpeg->width, jpeg->height, jpeg->output_format),
                        TAG, "set decode output format failed");
    ESP_RETURN_ON_ERROR(v4l2_stream_init(&jpeg->capture, jpeg->fd, V4L2_BUF_TYPE_VIDEO_CAPTURE,
                                         V4L2_MEMORY_MMAP, M2M_BUFFER_COUNT),
                        TAG, "init decode output stream failed");
    ESP_RETURN_ON_ERROR(v4l2_stream_queue_all(&jpeg->capture), TAG, "queue decode output buffer failed");
    ESP_RETURN_ON_ERROR(v4l2_stream_start(&jpeg->capture), TAG, "start decode output failed");
    ESP_RETURN_ON_ERROR(v4l2_stream_start(&jpeg->output), TAG, "start decode input failed");

    return ESP_OK;
}
#endif

#if M2M_USE_JPEG_ENCODER
static esp_err_t setup_jpeg_encoder_streams(example_jpeg_ctx *jpeg)
{
    ESP_RETURN_ON_ERROR(v4l2_set_pix_format(jpeg->fd, V4L2_BUF_TYPE_VIDEO_OUTPUT,
                                            jpeg->width, jpeg->height, jpeg->input_format),
                        TAG, "set encoder input format failed");
    ESP_RETURN_ON_ERROR(v4l2_stream_init(&jpeg->output, jpeg->fd, V4L2_BUF_TYPE_VIDEO_OUTPUT,
                                         V4L2_MEMORY_USERPTR, M2M_BUFFER_COUNT),
                        TAG, "init encoder input stream failed");
    ESP_RETURN_ON_ERROR(v4l2_set_pix_format(jpeg->fd, V4L2_BUF_TYPE_VIDEO_CAPTURE,
                                            jpeg->width, jpeg->height, jpeg->output_format),
                        TAG, "set encoder output format failed");
    ESP_RETURN_ON_ERROR(v4l2_stream_init(&jpeg->capture, jpeg->fd, V4L2_BUF_TYPE_VIDEO_CAPTURE,
                                         V4L2_MEMORY_MMAP, M2M_BUFFER_COUNT),
                        TAG, "init encoder output stream failed");
    ESP_RETURN_ON_ERROR(v4l2_stream_queue_all(&jpeg->capture), TAG, "queue encoder output buffer failed");
    ESP_RETURN_ON_ERROR(v4l2_stream_start(&jpeg->capture), TAG, "start encoder output failed");
    ESP_RETURN_ON_ERROR(v4l2_stream_start(&jpeg->output), TAG, "start encoder input failed");

    return ESP_OK;
}
#endif

#if M2M_USE_H264_ENCODER
static esp_err_t setup_h264_encoder_streams(example_h264_ctx *h264)
{
    ESP_RETURN_ON_ERROR(v4l2_set_pix_format(h264->fd, V4L2_BUF_TYPE_VIDEO_OUTPUT,
                                            h264->width, h264->height, h264->input_format),
                        TAG, "set H.264 input format failed");
    ESP_RETURN_ON_ERROR(v4l2_stream_init(&h264->output, h264->fd, V4L2_BUF_TYPE_VIDEO_OUTPUT,
                                         V4L2_MEMORY_USERPTR, M2M_BUFFER_COUNT),
                        TAG, "init H.264 input stream failed");
    ESP_RETURN_ON_ERROR(v4l2_set_pix_format(h264->fd, V4L2_BUF_TYPE_VIDEO_CAPTURE,
                                            h264->width, h264->height, V4L2_PIX_FMT_H264),
                        TAG, "set H.264 output format failed");
    ESP_RETURN_ON_ERROR(v4l2_stream_init(&h264->capture, h264->fd, V4L2_BUF_TYPE_VIDEO_CAPTURE,
                                         V4L2_MEMORY_MMAP, M2M_BUFFER_COUNT),
                        TAG, "init H.264 output stream failed");
    ESP_RETURN_ON_ERROR(v4l2_stream_queue_all(&h264->capture), TAG, "queue H.264 output buffer failed");
    ESP_RETURN_ON_ERROR(v4l2_stream_start(&h264->capture), TAG, "start H.264 output failed");
    ESP_RETURN_ON_ERROR(v4l2_stream_start(&h264->output), TAG, "start H.264 input failed");

    return ESP_OK;
}
#endif

static v4l2_stream_t *buffer_owner_stream(uint8_t owner)
{
    switch (owner) {
    case EXAMPLE_BUF_OWNER_CAMERA:
        return &s_camera.stream;
#if M2M_USE_JPEG_DECODER
    case EXAMPLE_BUF_OWNER_M2M_JPEG_DEC:
        return &s_jpeg_decoder.capture;
#endif
#if M2M_USE_JPEG_ENCODER
    case EXAMPLE_BUF_OWNER_M2M_JPEG_ENC:
        return &s_jpeg_encoder.capture;
#endif
#if M2M_USE_H264_ENCODER
    case EXAMPLE_BUF_OWNER_M2M_H264_ENC:
        return &s_h264.capture;
#endif
    default:
        return NULL;
    }
}

void buffer_free(example_image_t *image)
{
    v4l2_stream_t *stream;

    if (!image || !image->data) {
        return;
    }

    stream = buffer_owner_stream(image->buf_owner);
    if (stream && image->buf_index >= 0) {
        struct v4l2_buffer buf = {
            .index = (uint32_t)image->buf_index,
            .type = stream->buf_type,
            .memory = stream->memory,
        };

        if (v4l2_stream_queue_buffer(stream, &buf) != ESP_OK) {
            ESP_LOGW(TAG, "failed to requeue buffer owner=%u index=%d", image->buf_owner, image->buf_index);
        }
    }

    image->data = NULL;
    image->size = 0;
    image->buf_index = -1;
    image->buf_owner = EXAMPLE_BUF_OWNER_HEAP;
}

esp_err_t open_camera(const example_camera_config_t *config, example_camera_handle_t *camera)
{
#if M2M_USE_V4L2_CAPTURE_FORMAT
    format_info_t selected_format;
#else
    format_info_t *selected_format = NULL;
#endif

    ESP_RETURN_ON_FALSE(config && camera, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(!s_camera.opened, ESP_ERR_INVALID_STATE, TAG, "camera already opened");

    if (!s_video_inited) {
        ESP_RETURN_ON_ERROR(example_video_init(), TAG, "video init failed");
        s_video_inited = true;
    }

    memset(&s_camera, 0, sizeof(s_camera));
    s_camera.fd = -1;

    ESP_RETURN_ON_ERROR(v4l2_open_device(EXAMPLE_CAM_DEV_PATH, O_RDONLY, &s_camera.fd), TAG,
                        "open capture device failed");
    ESP_RETURN_ON_ERROR(v4l2_query_and_log(s_camera.fd, "Capture device"), TAG,
                        "query capture device failed");
    ESP_RETURN_ON_ERROR(setup_camera_format(config,
#if M2M_USE_V4L2_CAPTURE_FORMAT
                                            &selected_format
#else
                                            selected_format
#endif
                                           ), TAG, "setup camera format failed");
    ESP_RETURN_ON_ERROR(setup_camera_stream(), TAG, "setup camera stream failed");

    s_camera.opened = true;
    *camera = &s_camera;
    ESP_LOGI(TAG, "Camera opened: %" PRIu32 "x%" PRIu32 ", format=" V4L2_FMT_STR,
             s_camera.width, s_camera.height, V4L2_FMT_STR_ARG(s_camera.format));

    return ESP_OK;
}

void close_camera(void)
{
    if (!s_camera.opened) {
        return;
    }

    v4l2_stream_stop(&s_camera.stream);
    v4l2_stream_deinit(&s_camera.stream);
    v4l2_close_device(&s_camera.fd);
    memset(&s_camera, 0, sizeof(s_camera));
    s_camera.fd = -1;

    if (s_video_inited) {
        example_video_deinit();
        s_video_inited = false;
    }
}

esp_err_t camera_capture_image(example_image_t *image)
{
    struct v4l2_buffer buf;

    ESP_RETURN_ON_FALSE(s_camera.opened, ESP_ERR_INVALID_STATE, TAG, "camera not opened");
    ESP_RETURN_ON_FALSE(image, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    ESP_RETURN_ON_ERROR(v4l2_stream_dequeue(&s_camera.stream, &buf), TAG, "dequeue capture frame failed");

    if (!(buf.flags & V4L2_BUF_FLAG_DONE) || buf.bytesused == 0) {
        if (v4l2_stream_queue_buffer(&s_camera.stream, &buf) != ESP_OK) {
            ESP_LOGE(TAG, "failed to queue capture frame");
        }
        return ESP_ERR_INVALID_STATE;
    }

    image->width = s_camera.width;
    image->height = s_camera.height;
    image->format = s_camera.format;
    image->data = v4l2_stream_buffer_ptr(&s_camera.stream, buf.index);
    image->size = buf.bytesused;
    image->buf_index = buf.index;
    image->buf_owner = EXAMPLE_BUF_OWNER_CAMERA;

    return ESP_OK;
}

#if M2M_USE_JPEG_DECODER
esp_err_t open_jpeg_decoder(const example_jpeg_config_t *config, example_jpeg_decoder_handle_t *jpeg_decoder)
{
    ESP_RETURN_ON_FALSE(config && jpeg_decoder, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(!s_jpeg_decoder.opened, ESP_ERR_INVALID_STATE, TAG, "jpeg decoder already opened");

    memset(&s_jpeg_decoder, 0, sizeof(s_jpeg_decoder));
    s_jpeg_decoder.fd = -1;
    s_jpeg_decoder.is_decoder = true;
    s_jpeg_decoder.input_format = V4L2_PIX_FMT_JPEG;
    s_jpeg_decoder.output_format = V4L2_PIX_FMT_YUV420;

    ESP_RETURN_ON_ERROR(v4l2_open_device(ESP_VIDEO_JPEG_DEC_DEVICE_NAME, O_RDWR, &s_jpeg_decoder.fd), TAG,
                        "open JPEG decoder failed");
    ESP_RETURN_ON_ERROR(v4l2_query_and_log(s_jpeg_decoder.fd, "JPEG decoder"), TAG, "query decoder failed");

    s_jpeg_decoder.opened = true;
    *jpeg_decoder = &s_jpeg_decoder;
    ESP_LOGI(TAG, "JPEG decoder opened");

    return ESP_OK;
}
#endif

#if M2M_USE_JPEG_ENCODER
esp_err_t open_jpeg_encoder(const example_jpeg_config_t *config, example_jpeg_encoder_handle_t *jpeg_encoder)
{
    ESP_RETURN_ON_FALSE(config && jpeg_encoder, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(!s_jpeg_encoder.opened, ESP_ERR_INVALID_STATE, TAG, "jpeg encoder already opened");

    memset(&s_jpeg_encoder, 0, sizeof(s_jpeg_encoder));
    s_jpeg_encoder.fd = -1;
    s_jpeg_encoder.is_decoder = false;
    s_jpeg_encoder.output_format = V4L2_PIX_FMT_JPEG;

    ESP_RETURN_ON_ERROR(v4l2_open_device(ESP_VIDEO_JPEG_DEVICE_NAME, O_RDWR, &s_jpeg_encoder.fd), TAG,
                        "open JPEG encoder failed");
    ESP_RETURN_ON_ERROR(v4l2_query_and_log(s_jpeg_encoder.fd, "JPEG encoder"), TAG, "query encoder failed");
    v4l2_set_ext_ctrl(s_jpeg_encoder.fd, V4L2_CID_JPEG_CLASS, V4L2_CID_JPEG_COMPRESSION_QUALITY,
                      config->quality ? config->quality :
#ifdef CONFIG_EXAMPLE_JPEG_COMPRESSION_QUALITY
                      CONFIG_EXAMPLE_JPEG_COMPRESSION_QUALITY
#else
                      80
#endif
                     );

    s_jpeg_encoder.opened = true;
    *jpeg_encoder = &s_jpeg_encoder;
    ESP_LOGI(TAG, "JPEG encoder opened");

    return ESP_OK;
}
#endif

#if M2M_USE_JPEG_DECODER
void close_jpeg_decoder(example_jpeg_decoder_handle_t jpeg_decoder)
{
    example_jpeg_ctx *jpeg = jpeg_decoder;

    if (!jpeg || !jpeg->opened || !jpeg->is_decoder) {
        return;
    }

    v4l2_stream_stop(&jpeg->output);
    v4l2_stream_stop(&jpeg->capture);
    v4l2_stream_deinit(&jpeg->output);
    v4l2_stream_deinit(&jpeg->capture);

    v4l2_close_device(&jpeg->fd);
    memset(jpeg, 0, sizeof(*jpeg));
    jpeg->fd = -1;
}
#endif

#if M2M_USE_JPEG_ENCODER
void close_jpeg_encoder(example_jpeg_encoder_handle_t jpeg_encoder)
{
    example_jpeg_ctx *jpeg = jpeg_encoder;

    if (!jpeg || !jpeg->opened || jpeg->is_decoder) {
        return;
    }

    v4l2_stream_stop(&jpeg->output);
    v4l2_stream_stop(&jpeg->capture);
    v4l2_stream_deinit(&jpeg->output);
    v4l2_stream_deinit(&jpeg->capture);
    v4l2_close_device(&jpeg->fd);
    memset(jpeg, 0, sizeof(*jpeg));
    jpeg->fd = -1;
}
#endif

#if M2M_USE_JPEG_DECODER
esp_err_t camera_connect(example_camera_handle_t camera, example_jpeg_decoder_handle_t jpeg_decoder)
{
    example_jpeg_ctx *jpeg = jpeg_decoder;

    ESP_RETURN_ON_FALSE(camera && camera->opened, ESP_ERR_INVALID_STATE, TAG, "camera not opened");
    ESP_RETURN_ON_FALSE(jpeg && jpeg->opened && jpeg->is_decoder, ESP_ERR_INVALID_STATE, TAG,
                        "jpeg decoder not opened");
    ESP_RETURN_ON_FALSE(!jpeg->connected, ESP_ERR_INVALID_STATE, TAG, "jpeg decoder already connected");

    jpeg->width = camera->width;
    jpeg->height = camera->height;
    jpeg->input_format = camera->format;

    ESP_RETURN_ON_ERROR(setup_jpeg_decoder_streams(jpeg), TAG, "setup jpeg decoder streams failed");

    jpeg->connected = true;
    ESP_LOGI(TAG, "Camera connected to JPEG decoder: %" PRIu32 "x%" PRIu32 ", in=" V4L2_FMT_STR,
             jpeg->width, jpeg->height, V4L2_FMT_STR_ARG(jpeg->input_format));

    return ESP_OK;
}
#endif

#if M2M_USE_JPEG_ENCODER
esp_err_t jpeg_encode_connect(example_camera_handle_t camera, example_jpeg_encoder_handle_t jpeg_encoder)
{
    example_jpeg_ctx *jpeg = jpeg_encoder;

    ESP_RETURN_ON_FALSE(camera && camera->opened, ESP_ERR_INVALID_STATE, TAG, "camera not opened");
    ESP_RETURN_ON_FALSE(jpeg && jpeg->opened && !jpeg->is_decoder, ESP_ERR_INVALID_STATE, TAG,
                        "jpeg encoder not opened");
    ESP_RETURN_ON_FALSE(!jpeg->connected, ESP_ERR_INVALID_STATE, TAG, "jpeg encoder already connected");

    jpeg->width = camera->width;
    jpeg->height = camera->height;
    jpeg->input_format = camera->format;

    ESP_RETURN_ON_ERROR(setup_jpeg_encoder_streams(jpeg), TAG, "setup jpeg encoder streams failed");

    jpeg->connected = true;
    ESP_LOGI(TAG, "Camera connected to JPEG encoder: %" PRIu32 "x%" PRIu32 ", in=" V4L2_FMT_STR ", out=" V4L2_FMT_STR,
             jpeg->width, jpeg->height, V4L2_FMT_STR_ARG(jpeg->input_format), V4L2_FMT_STR_ARG(jpeg->output_format));

    return ESP_OK;
}
#endif

#if CONFIG_EXAMPLE_M2M_MODE_PIPELINE
esp_err_t jpeg_decode_connect(example_jpeg_decoder_handle_t jpeg_decoder, example_h264_encoder_handle_t h264_encoder)
{
    example_jpeg_ctx *jpeg = jpeg_decoder;
    example_h264_ctx *h264 = h264_encoder;

    ESP_RETURN_ON_FALSE(jpeg && jpeg->opened && jpeg->is_decoder && jpeg->connected, ESP_ERR_INVALID_STATE, TAG,
                        "jpeg decoder not connected");
    ESP_RETURN_ON_FALSE(h264 && h264->opened, ESP_ERR_INVALID_STATE, TAG, "h264 encoder not opened");
    ESP_RETURN_ON_FALSE(!h264->connected, ESP_ERR_INVALID_STATE, TAG, "h264 encoder already connected");

    h264->width = jpeg->width;
    h264->height = jpeg->height;
    h264->input_format = jpeg->output_format;

    ESP_RETURN_ON_ERROR(setup_h264_encoder_streams(h264), TAG, "setup h264 encoder streams failed");

    h264->connected = true;
    ESP_LOGI(TAG, "JPEG decoder connected to H.264 encoder: %" PRIu32 "x%" PRIu32 ", in=" V4L2_FMT_STR,
             h264->width, h264->height, V4L2_FMT_STR_ARG(h264->input_format));

    return ESP_OK;
}
#endif

#if CONFIG_EXAMPLE_M2M_MODE_ENCODE && CONFIG_ESP_VIDEO_ENABLE_H264_VIDEO_DEVICE
esp_err_t h264_encode_connect(example_camera_handle_t camera, example_h264_encoder_handle_t h264_encoder)
{
    example_h264_ctx *h264 = h264_encoder;

    ESP_RETURN_ON_FALSE(camera && camera->opened, ESP_ERR_INVALID_STATE, TAG, "camera not opened");
    ESP_RETURN_ON_FALSE(h264 && h264->opened, ESP_ERR_INVALID_STATE, TAG, "h264 encoder not opened");
    ESP_RETURN_ON_FALSE(!h264->connected, ESP_ERR_INVALID_STATE, TAG, "h264 encoder already connected");

    h264->width = camera->width;
    h264->height = camera->height;
    h264->input_format = camera->format;

    ESP_RETURN_ON_ERROR(setup_h264_encoder_streams(h264), TAG, "setup h264 encoder streams failed");

    h264->connected = true;
    ESP_LOGI(TAG, "Camera connected to H.264 encoder: %" PRIu32 "x%" PRIu32 ", in=" V4L2_FMT_STR,
             h264->width, h264->height, V4L2_FMT_STR_ARG(h264->input_format));

    return ESP_OK;
}
#endif

#if M2M_USE_JPEG_ENCODER
esp_err_t jpeg_encode(const example_image_t *input, example_image_t *output)
{
    ESP_RETURN_ON_FALSE(s_jpeg_encoder.opened && !s_jpeg_encoder.is_decoder && s_jpeg_encoder.connected,
                        ESP_ERR_INVALID_STATE, TAG, "JPEG encoder not connected");
    ESP_RETURN_ON_FALSE(input && output, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    esp_err_t ret = m2m_encode_frame(s_jpeg_encoder.fd, &s_jpeg_encoder.output, &s_jpeg_encoder.capture, input, output,
                                     s_jpeg_encoder.output_format);
    if (ret == ESP_OK) {
        output->buf_owner = EXAMPLE_BUF_OWNER_M2M_JPEG_ENC;
    }

    return ret;
}
#endif

#if M2M_USE_JPEG_DECODER
esp_err_t jpeg_decode(const example_image_t *input, example_image_t *output)
{
    ESP_RETURN_ON_FALSE(s_jpeg_decoder.opened && s_jpeg_decoder.is_decoder && s_jpeg_decoder.connected,
                        ESP_ERR_INVALID_STATE, TAG, "JPEG decoder not connected");
    ESP_RETURN_ON_FALSE(input && output, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    return m2m_decode_frame(&s_jpeg_decoder, input, output);
}
#endif

#if M2M_USE_H264_ENCODER
esp_err_t open_h264_encoder(const example_h264_config_t *config, example_h264_encoder_handle_t *h264_encoder)
{
    ESP_RETURN_ON_FALSE(config && h264_encoder, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(!s_h264.opened, ESP_ERR_INVALID_STATE, TAG, "h264 already opened");

    memset(&s_h264, 0, sizeof(s_h264));
    s_h264.fd = -1;

    ESP_RETURN_ON_ERROR(v4l2_open_device(ESP_VIDEO_H264_DEVICE_NAME, O_RDWR, &s_h264.fd), TAG,
                        "open H.264 encoder failed");
    ESP_RETURN_ON_ERROR(v4l2_query_and_log(s_h264.fd, "H.264 encoder"), TAG, "query encoder failed");

    v4l2_set_ext_ctrl(s_h264.fd, V4L2_CID_CODEC_CLASS, V4L2_CID_MPEG_VIDEO_H264_I_PERIOD,
                      config->i_period ? config->i_period : CONFIG_EXAMPLE_H264_I_PERIOD);
    v4l2_set_ext_ctrl(s_h264.fd, V4L2_CID_CODEC_CLASS, V4L2_CID_MPEG_VIDEO_BITRATE,
                      config->bitrate ? config->bitrate : CONFIG_EXAMPLE_H264_BITRATE);
    v4l2_set_ext_ctrl(s_h264.fd, V4L2_CID_CODEC_CLASS, V4L2_CID_MPEG_VIDEO_H264_MIN_QP,
                      config->min_qp ? config->min_qp : CONFIG_EXAMPLE_H264_MIN_QP);
    v4l2_set_ext_ctrl(s_h264.fd, V4L2_CID_CODEC_CLASS, V4L2_CID_MPEG_VIDEO_H264_MAX_QP,
                      config->max_qp ? config->max_qp : CONFIG_EXAMPLE_H264_MAX_QP);

    s_h264.opened = true;
    *h264_encoder = &s_h264;
    ESP_LOGI(TAG, "H.264 encoder opened");

    return ESP_OK;
}

void close_h264_encoder(example_h264_encoder_handle_t h264_encoder)
{
    example_h264_ctx *h264 = h264_encoder;

    if (!h264 || !h264->opened) {
        return;
    }

    v4l2_stream_stop(&h264->output);
    v4l2_stream_stop(&h264->capture);
    v4l2_stream_deinit(&h264->output);
    v4l2_stream_deinit(&h264->capture);
    v4l2_close_device(&h264->fd);
    memset(h264, 0, sizeof(*h264));
    h264->fd = -1;
}

esp_err_t h264_encode(const example_image_t *input, example_image_t *output)
{
    ESP_RETURN_ON_FALSE(s_h264.opened && s_h264.connected, ESP_ERR_INVALID_STATE, TAG,
                        "H.264 encoder not connected");
    ESP_RETURN_ON_FALSE(input && output, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    esp_err_t ret = m2m_encode_frame(s_h264.fd, &s_h264.output, &s_h264.capture, input, output, V4L2_PIX_FMT_H264);
    if (ret == ESP_OK) {
        output->buf_owner = EXAMPLE_BUF_OWNER_M2M_H264_ENC;
    }

    return ret;
}
#endif
