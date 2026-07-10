/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "unity.h"
#include "esp_video_init.h"
#include "esp_video_device.h"
#include "esp_video_ioctl.h"

#define TEST_H264_WIDTH         128
#define TEST_H264_HEIGHT        128
#define TEST_H264_BUFFER_NUM    1

#define H264_DEFAULT_I_PERIOD   30
#define H264_DEFAULT_MIN_QP     25
#define H264_DEFAULT_MAX_QP     26
#define H264_DEFAULT_BITRATE    10000000

typedef struct {
    int32_t i_period;
    int32_t bitrate;
    int32_t min_qp;
    int32_t max_qp;
} h264_ctrl_values_t;

static int h264_set_ext_ctrl(int fd, uint32_t id, int32_t value)
{
    struct v4l2_ext_controls ctrls;
    struct v4l2_ext_control ctrl;

    memset(&ctrls, 0, sizeof(ctrls));
    memset(&ctrl, 0, sizeof(ctrl));
    ctrls.ctrl_class = V4L2_CID_CODEC_CLASS;
    ctrls.count = 1;
    ctrls.controls = &ctrl;
    ctrl.id = id;
    ctrl.value = value;

    return ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls);
}

static int h264_get_ext_ctrl(int fd, uint32_t id, int32_t *value)
{
    struct v4l2_ext_controls ctrls;
    struct v4l2_ext_control ctrl;

    memset(&ctrls, 0, sizeof(ctrls));
    memset(&ctrl, 0, sizeof(ctrl));
    ctrls.ctrl_class = V4L2_CID_CODEC_CLASS;
    ctrls.count = 1;
    ctrls.controls = &ctrl;
    ctrl.id = id;

    int ret = ioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls);
    if (ret == 0 && value) {
        *value = ctrl.value;
    }

    return ret;
}

static void h264_query_and_verify_ext_ctrl(int fd, uint32_t id, int64_t minimum,
        int64_t maximum, int64_t step, int64_t default_value)
{
    struct v4l2_query_ext_ctrl qctrl;

    memset(&qctrl, 0, sizeof(qctrl));
    qctrl.id = id;
    TEST_ESP_OK(ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &qctrl));
    TEST_ASSERT_EQUAL_INT64(minimum, qctrl.minimum);
    TEST_ASSERT_EQUAL_INT64(maximum, qctrl.maximum);
    TEST_ASSERT_EQUAL_UINT64(step, qctrl.step);
    TEST_ASSERT_EQUAL_INT64(default_value, qctrl.default_value);
}

static void h264_set_ctrl_values(int fd, const h264_ctrl_values_t *values)
{
    TEST_ESP_OK(h264_set_ext_ctrl(fd, V4L2_CID_MPEG_VIDEO_H264_I_PERIOD, values->i_period));
    TEST_ESP_OK(h264_set_ext_ctrl(fd, V4L2_CID_MPEG_VIDEO_BITRATE, values->bitrate));
    TEST_ESP_OK(h264_set_ext_ctrl(fd, V4L2_CID_MPEG_VIDEO_H264_MIN_QP, values->min_qp));
    TEST_ESP_OK(h264_set_ext_ctrl(fd, V4L2_CID_MPEG_VIDEO_H264_MAX_QP, values->max_qp));
}

static void h264_verify_ctrl_values(int fd, const h264_ctrl_values_t *values)
{
    int32_t actual;

    TEST_ESP_OK(h264_get_ext_ctrl(fd, V4L2_CID_MPEG_VIDEO_H264_I_PERIOD, &actual));
    TEST_ASSERT_EQUAL_INT32(values->i_period, actual);

    TEST_ESP_OK(h264_get_ext_ctrl(fd, V4L2_CID_MPEG_VIDEO_BITRATE, &actual));
    TEST_ASSERT_EQUAL_INT32(values->bitrate, actual);

    TEST_ESP_OK(h264_get_ext_ctrl(fd, V4L2_CID_MPEG_VIDEO_H264_MIN_QP, &actual));
    TEST_ASSERT_EQUAL_INT32(values->min_qp, actual);

    TEST_ESP_OK(h264_get_ext_ctrl(fd, V4L2_CID_MPEG_VIDEO_H264_MAX_QP, &actual));
    TEST_ASSERT_EQUAL_INT32(values->max_qp, actual);
}

static void h264_setup_m2m_stream(int fd)
{
    int ret;
    int val;
    struct v4l2_format format;
    struct v4l2_requestbuffers req;
    struct v4l2_buffer buf;

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    format.fmt.pix.width = TEST_H264_WIDTH;
    format.fmt.pix.height = TEST_H264_HEIGHT;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
    ret = ioctl(fd, VIDIOC_S_FMT, &format);
    TEST_ESP_OK(ret);

    memset(&req, 0, sizeof(req));
    req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    req.memory = V4L2_MEMORY_MMAP;
    req.count = TEST_H264_BUFFER_NUM;
    ret = ioctl(fd, VIDIOC_REQBUFS, &req);
    TEST_ESP_OK(ret);

    for (int i = 0; i < TEST_H264_BUFFER_NUM; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        ret = ioctl(fd, VIDIOC_QUERYBUF, &buf);
        TEST_ESP_OK(ret);

        void *mapped = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        TEST_ASSERT_NOT_EQUAL(MAP_FAILED, mapped);

        ret = ioctl(fd, VIDIOC_QBUF, &buf);
        TEST_ESP_OK(ret);
    }

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = TEST_H264_WIDTH;
    format.fmt.pix.height = TEST_H264_HEIGHT;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
    ret = ioctl(fd, VIDIOC_S_FMT, &format);
    TEST_ESP_OK(ret);

    memset(&req, 0, sizeof(req));
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    req.count = TEST_H264_BUFFER_NUM;
    ret = ioctl(fd, VIDIOC_REQBUFS, &req);
    TEST_ESP_OK(ret);

    for (int i = 0; i < TEST_H264_BUFFER_NUM; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        ret = ioctl(fd, VIDIOC_QUERYBUF, &buf);
        TEST_ESP_OK(ret);

        void *mapped = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        TEST_ASSERT_NOT_NULL(mapped);

        ret = ioctl(fd, VIDIOC_QBUF, &buf);
        TEST_ESP_OK(ret);
    }

    val = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd, VIDIOC_STREAMON, &val);
    TEST_ESP_OK(ret);

    val = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ret = ioctl(fd, VIDIOC_STREAMON, &val);
    TEST_ESP_OK(ret);
}

static void h264_stop_m2m_stream(int fd)
{
    int val;

    val = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    TEST_ESP_OK(ioctl(fd, VIDIOC_STREAMOFF, &val));

    val = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    TEST_ESP_OK(ioctl(fd, VIDIOC_STREAMOFF, &val));
}

TEST_CASE("H.264 video device ext controls", "[video][h264]")
{
    int fd;
    h264_ctrl_values_t defaults = {
        .i_period = H264_DEFAULT_I_PERIOD,
        .bitrate = H264_DEFAULT_BITRATE,
        .min_qp = H264_DEFAULT_MIN_QP,
        .max_qp = H264_DEFAULT_MAX_QP,
    };
    h264_ctrl_values_t before_stream = {
        .i_period = 45,
        .bitrate = 8000000,
        .min_qp = 22,
        .max_qp = 28,
    };
    h264_ctrl_values_t during_stream = {
        .i_period = 50,
        .bitrate = 6000000,
        .min_qp = 24,
        .max_qp = 32,
    };
    h264_ctrl_values_t after_stream = {
        .i_period = 40,
        .bitrate = 7500000,
        .min_qp = 26,
        .max_qp = 35,
    };

    setUp();

    esp_video_init_config_t config = { 0 };
    TEST_ESP_OK(esp_video_init_with_flags(&config, ESP_VIDEO_INIT_FLAGS_H264));

    fd = open(ESP_VIDEO_H264_DEVICE_NAME, O_RDWR);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

    h264_query_and_verify_ext_ctrl(fd, V4L2_CID_MPEG_VIDEO_H264_I_PERIOD, 1, 120, 1, H264_DEFAULT_I_PERIOD);
    h264_query_and_verify_ext_ctrl(fd, V4L2_CID_MPEG_VIDEO_H264_MIN_QP, 0, 51, 1, H264_DEFAULT_MIN_QP);
    h264_query_and_verify_ext_ctrl(fd, V4L2_CID_MPEG_VIDEO_H264_MAX_QP, 0, 51, 1, H264_DEFAULT_MAX_QP);
    h264_query_and_verify_ext_ctrl(fd, V4L2_CID_MPEG_VIDEO_BITRATE, 25000, 25000000, 25000, H264_DEFAULT_BITRATE);

    /* Before stream on: read default values, write new values, read back */
    h264_verify_ctrl_values(fd, &defaults);
    h264_set_ctrl_values(fd, &before_stream);
    h264_verify_ctrl_values(fd, &before_stream);

    /* After stream on: read/write parameters */
    h264_setup_m2m_stream(fd);
    h264_verify_ctrl_values(fd, &before_stream);
    h264_set_ctrl_values(fd, &during_stream);
    h264_verify_ctrl_values(fd, &during_stream);
    h264_set_ctrl_values(fd, &after_stream);
    h264_verify_ctrl_values(fd, &after_stream);

    /* After stream off: read/write parameters */
    h264_stop_m2m_stream(fd);
    h264_set_ctrl_values(fd, &during_stream);
    h264_verify_ctrl_values(fd, &during_stream);
    h264_set_ctrl_values(fd, &after_stream);
    h264_verify_ctrl_values(fd, &after_stream);

    close(fd);
    TEST_ESP_OK(esp_video_deinit_with_flags(ESP_VIDEO_INIT_FLAGS_H264));
}
