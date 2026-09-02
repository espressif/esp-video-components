/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include <inttypes.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "unity.h"
#include "esp_log.h"
#include "esp_video_dvp_format.h"
#include "esp_video_caps.h"
#include "example_video_common.h"

#define DVP_CAPTURE_BUFFER_NUM 2

void setUp(void);

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

static const char *TAG = "dvp_format";

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
    case ESP_CAM_SENSOR_PIXFORMAT_GRAYSCALE:
        return V4L2_PIX_FMT_GREY;
    default:
        return 0;
    }
}

static void test_dvp_enum_format(esp_cam_sensor_output_format_t sensor_fmt, const uint32_t *expected_formats, int expected_count)
{
    uint32_t fmt;
    int index = 0;

    ESP_LOGI(TAG, "Enumerate DVP formats for sensor format %d", sensor_fmt);

    for (int i = 0; i < expected_count; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, esp_video_dvp_enum_format(sensor_fmt, i, &fmt));
        bool found = false;
        for (int j = 0; j < expected_count; j++) {
            if (expected_formats[j] == fmt) {
                found = true;
                break;
            }
        }
        TEST_ASSERT(found);
        index++;
    }

    TEST_ASSERT_EQUAL(expected_count, index);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_video_dvp_enum_format(sensor_fmt, index, &fmt));
}

TEST_CASE("Test enumerate formats for DVP", "[video][dvp]")
{
    const uint32_t uyvy_expected[] = {
        V4L2_PIX_FMT_UYVY,
    };
    test_dvp_enum_format(ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY, uyvy_expected, ARRAY_SIZE(uyvy_expected));

#if ESP_VIDEO_DVP_DEVICE_CONV_FORMAT
    const uint32_t yuyv_expected[] = {
        V4L2_PIX_FMT_YUYV,
        /* Converted RGB565 is big-endian, not little-endian */
        V4L2_PIX_FMT_RGB565X,
    };
#else
    const uint32_t yuyv_expected[] = {
        V4L2_PIX_FMT_YUYV,
    };
#endif
    test_dvp_enum_format(ESP_CAM_SENSOR_PIXFORMAT_YUV422_YUYV, yuyv_expected, ARRAY_SIZE(yuyv_expected));

    const esp_cam_sensor_output_format_t passthrough_sensors[] = {
        ESP_CAM_SENSOR_PIXFORMAT_RGB565_LE,
        ESP_CAM_SENSOR_PIXFORMAT_RGB565_BE,
        ESP_CAM_SENSOR_PIXFORMAT_RGB888,
        ESP_CAM_SENSOR_PIXFORMAT_JPEG,
        ESP_CAM_SENSOR_PIXFORMAT_GRAYSCALE,
    };

    for (int i = 0; i < ARRAY_SIZE(passthrough_sensors); i++) {
        const uint32_t expected[] = {
            sensor_to_v4l2_format(passthrough_sensors[i]),
        };
        test_dvp_enum_format(passthrough_sensors[i], expected, ARRAY_SIZE(expected));
    }
}

TEST_CASE("Test DVP check_format native and YUV422 to RGB565X", "[video][dvp]")
{
    esp_video_dvp_in_out_format_t in_out = {0};

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_video_dvp_check_format(ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY, 0, &in_out));
    TEST_ASSERT_EQUAL(ESP_OK, esp_video_dvp_check_format(ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY, V4L2_PIX_FMT_UYVY, &in_out));
    TEST_ASSERT_EQUAL(ESP_OK, esp_video_dvp_check_format(ESP_CAM_SENSOR_PIXFORMAT_YUV422_YUYV, V4L2_PIX_FMT_YUYV, &in_out));

    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_video_dvp_check_format(ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY, V4L2_PIX_FMT_RGB565X, &in_out));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_video_dvp_check_format(ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY, V4L2_PIX_FMT_RGB565, &in_out));

#if ESP_VIDEO_DVP_DEVICE_CONV_FORMAT
    TEST_ASSERT_EQUAL(ESP_OK, esp_video_dvp_check_format(ESP_CAM_SENSOR_PIXFORMAT_YUV422_YUYV, V4L2_PIX_FMT_RGB565X, &in_out));
    TEST_ASSERT_EQUAL(CAM_CTLR_COLOR_RGB565, in_out.out_color);
    /* Converted RGB565 is big-endian (RGB565X). Little-endian RGB565 is not a conversion target. */
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_video_dvp_check_format(ESP_CAM_SENSOR_PIXFORMAT_YUV422_YUYV, V4L2_PIX_FMT_RGB565, &in_out));
#else
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_video_dvp_check_format(ESP_CAM_SENSOR_PIXFORMAT_YUV422_YUYV, V4L2_PIX_FMT_RGB565X, &in_out));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_video_dvp_check_format(ESP_CAM_SENSOR_PIXFORMAT_YUV422_YUYV, V4L2_PIX_FMT_RGB565, &in_out));
#endif

    TEST_ASSERT_EQUAL(ESP_OK, esp_video_dvp_check_format(ESP_CAM_SENSOR_PIXFORMAT_RGB565_LE, V4L2_PIX_FMT_RGB565, &in_out));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_video_dvp_check_format(ESP_CAM_SENSOR_PIXFORMAT_RGB565_LE, V4L2_PIX_FMT_UYVY, &in_out));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_video_dvp_check_format(ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY, V4L2_PIX_FMT_UYVY, NULL));
}

#if CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR
static bool is_yuv422_yuyv_sensor_format(esp_cam_sensor_output_format_t fmt)
{
    return fmt == ESP_CAM_SENSOR_PIXFORMAT_YUV422_YUYV;
}

static void dvp_capture_one_frame(int fd, uint32_t pixelformat, uint32_t width, uint32_t height)
{
    int ret;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    struct v4l2_format format;
    struct v4l2_buffer buf;
    struct v4l2_requestbuffers req;
    uint8_t *buffers[DVP_CAPTURE_BUFFER_NUM];
    uint32_t lengths[DVP_CAPTURE_BUFFER_NUM];

    memset(&format, 0, sizeof(format));
    format.type = type;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = pixelformat;
    ret = ioctl(fd, VIDIOC_S_FMT, &format);
    TEST_ESP_OK(ret);

    memset(&format, 0, sizeof(format));
    format.type = type;
    ret = ioctl(fd, VIDIOC_G_FMT, &format);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL_UINT32(pixelformat, format.fmt.pix.pixelformat);
    TEST_ASSERT_EQUAL_UINT32(width, format.fmt.pix.width);
    TEST_ASSERT_EQUAL_UINT32(height, format.fmt.pix.height);

    memset(&req, 0, sizeof(req));
    req.type = type;
    req.memory = V4L2_MEMORY_MMAP;
    req.count = DVP_CAPTURE_BUFFER_NUM;
    ret = ioctl(fd, VIDIOC_REQBUFS, &req);
    TEST_ESP_OK(ret);

    for (int i = 0; i < DVP_CAPTURE_BUFFER_NUM; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type = type;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        ret = ioctl(fd, VIDIOC_QUERYBUF, &buf);
        TEST_ESP_OK(ret);

        lengths[i] = buf.length;
        buffers[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        TEST_ASSERT_NOT_EQUAL(MAP_FAILED, buffers[i]);

        ret = ioctl(fd, VIDIOC_QBUF, &buf);
        TEST_ESP_OK(ret);
    }

    ret = ioctl(fd, VIDIOC_STREAMON, &type);
    TEST_ESP_OK(ret);

    memset(&buf, 0, sizeof(buf));
    buf.type = type;
    buf.memory = V4L2_MEMORY_MMAP;
    ret = ioctl(fd, VIDIOC_DQBUF, &buf);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL_UINT32(width * height * 2, buf.bytesused);

    ret = ioctl(fd, VIDIOC_STREAMOFF, &type);
    TEST_ESP_OK(ret);

    for (int i = 0; i < DVP_CAPTURE_BUFFER_NUM; i++) {
        munmap(buffers[i], lengths[i]);
    }

    memset(&req, 0, sizeof(req));
    req.type = type;
    req.memory = V4L2_MEMORY_MMAP;
    req.count = 0;
    ret = ioctl(fd, VIDIOC_REQBUFS, &req);
    TEST_ESP_OK(ret);
}

TEST_CASE("V4L2 DVP capture YUV422 then RGB565X", "[video][dvp]")
{
    int fd;
    int ret;
    uint32_t width;
    uint32_t height;
    uint32_t yuv_pixelformat;
    struct v4l2_format format;
    esp_cam_sensor_format_t sensor_fmt;

    setUp();

    TEST_ESP_OK(example_video_init());

    fd = open(ESP_VIDEO_DVP_DEVICE_NAME, O_RDWR);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

    memset(&sensor_fmt, 0, sizeof(sensor_fmt));
    ret = ioctl(fd, VIDIOC_G_SENSOR_FMT, &sensor_fmt);
    TEST_ESP_OK(ret);

    if (!is_yuv422_yuyv_sensor_format(sensor_fmt.format)) {
        close(fd);
        TEST_ESP_OK(example_video_deinit());
        TEST_IGNORE_MESSAGE("DVP sensor current format is not YUV422 YUYV");
    }

    ret = ioctl(fd, VIDIOC_S_SENSOR_FMT, &sensor_fmt);
    TEST_ESP_OK(ret);

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd, VIDIOC_G_FMT, &format);
    TEST_ESP_OK(ret);

    yuv_pixelformat = format.fmt.pix.pixelformat;
    width = format.fmt.pix.width;
    height = format.fmt.pix.height;
    TEST_ASSERT_EQUAL_UINT32(V4L2_PIX_FMT_YUYV, yuv_pixelformat);
    TEST_ASSERT_GREATER_THAN(0, width);
    TEST_ASSERT_GREATER_THAN(0, height);

    ESP_LOGI(TAG, "Capture native YUV422: %" PRIu32 "x%" PRIu32, width, height);
    dvp_capture_one_frame(fd, yuv_pixelformat, width, height);

#if ESP_VIDEO_DVP_DEVICE_CONV_FORMAT
    ESP_LOGI(TAG, "Capture converted RGB565X: %" PRIu32 "x%" PRIu32, width, height);
    dvp_capture_one_frame(fd, V4L2_PIX_FMT_RGB565X, width, height);
#endif

    close(fd);
    TEST_ESP_OK(example_video_deinit());
}
#endif /* CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR */
