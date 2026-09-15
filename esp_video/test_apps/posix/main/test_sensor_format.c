/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "unity.h"
#include "example_video_common.h"
#include "esp_video_ioctl.h"

#define TEST_APP_VIDEO_DEVICE   EXAMPLE_CAM_DEV_PATH
#define TEST_CAPTURE_BUF_COUNT  2
#define TEST_CAPTURE_FRAME_NUM  2

void setUp(void);

static void capture_frames(int fd)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    struct v4l2_requestbuffers req;
    struct v4l2_buffer buf;
    uint8_t *mmap_buffers[TEST_CAPTURE_BUF_COUNT] = {0};

    memset(&req, 0, sizeof(req));
    req.count  = TEST_CAPTURE_BUF_COUNT;
    req.type   = type;
    req.memory = V4L2_MEMORY_MMAP;
    TEST_ESP_OK(ioctl(fd, VIDIOC_REQBUFS, &req));
    TEST_ASSERT_EQUAL_UINT32(TEST_CAPTURE_BUF_COUNT, req.count);

    for (int i = 0; i < TEST_CAPTURE_BUF_COUNT; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type   = type;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        TEST_ESP_OK(ioctl(fd, VIDIOC_QUERYBUF, &buf));

        mmap_buffers[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        TEST_ASSERT_NOT_EQUAL(MAP_FAILED, mmap_buffers[i]);
        TEST_ESP_OK(ioctl(fd, VIDIOC_QBUF, &buf));
    }

    TEST_ESP_OK(ioctl(fd, VIDIOC_STREAMON, &type));

    for (int i = 0; i < TEST_CAPTURE_FRAME_NUM; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type   = type;
        buf.memory = V4L2_MEMORY_MMAP;
        TEST_ESP_OK(ioctl(fd, VIDIOC_DQBUF, &buf));
        TEST_ASSERT_GREATER_THAN(0, buf.bytesused);
        TEST_ESP_OK(ioctl(fd, VIDIOC_QBUF, &buf));
    }

    TEST_ESP_OK(ioctl(fd, VIDIOC_STREAMOFF, &type));

    for (int i = 0; i < TEST_CAPTURE_BUF_COUNT; i++) {
        if (mmap_buffers[i] && mmap_buffers[i] != MAP_FAILED) {
            memset(&buf, 0, sizeof(buf));
            buf.type   = type;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index  = i;
            if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == 0) {
                munmap(mmap_buffers[i], buf.length);
            }
        }
    }

    memset(&req, 0, sizeof(req));
    req.type   = type;
    req.memory = V4L2_MEMORY_MMAP;
    req.count  = 0;
    TEST_ESP_OK(ioctl(fd, VIDIOC_REQBUFS, &req));
}

TEST_CASE("V4L2 enum/set sensor format then capture", "[video][sensor_format]")
{
    int fd;
    int ret;
    uint32_t sensor_fmt_count = 0;
    struct v4l2_sensor_format_enum sensor_enum;
    esp_cam_sensor_format_t current_fmt;

    setUp();
    TEST_ESP_OK(example_video_init());

    fd = open(TEST_APP_VIDEO_DEVICE, O_RDWR);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

    /* Enumerate all sensor formats. */
    memset(&current_fmt, 0, sizeof(current_fmt));
    TEST_ESP_OK(ioctl(fd, VIDIOC_G_SENSOR_FMT, &current_fmt));
    printf("current sensor format: %s %ux%u @%ufps\n",
           current_fmt.name ? current_fmt.name : "(null)",
           current_fmt.width, current_fmt.height, current_fmt.fps);

    /* Enumerate all sensor formats. */
    for (uint32_t i = 0; ; i++) {
        memset(&sensor_enum, 0, sizeof(sensor_enum));
        sensor_enum.index = i;
        ret = ioctl(fd, VIDIOC_ENUM_SENSOR_FMT, &sensor_enum);
        if (ret != 0) {
            break;
        }

        TEST_ASSERT_NOT_NULL(sensor_enum.format.name);
        TEST_ASSERT_GREATER_THAN(0, sensor_enum.format.width);
        TEST_ASSERT_GREATER_THAN(0, sensor_enum.format.height);
        TEST_ASSERT_GREATER_THAN(0, sensor_enum.format.fps);
        printf("[sensor fmt %" PRIu32 "] %s %ux%u @%ufps format=%d\n",
               i,
               sensor_enum.format.name ? sensor_enum.format.name : "(null)",
               sensor_enum.format.width,
               sensor_enum.format.height,
               sensor_enum.format.fps,
               (int)sensor_enum.format.format);
        sensor_fmt_count++;
    }
    TEST_ASSERT_GREATER_THAN(0, sensor_fmt_count);

    /* Out-of-range index must fail. */
    memset(&sensor_enum, 0, sizeof(sensor_enum));
    sensor_enum.index = sensor_fmt_count;
    ret = ioctl(fd, VIDIOC_ENUM_SENSOR_FMT, &sensor_enum);
    TEST_ASSERT_EQUAL_INT(-1, ret);

    /* For each sensor format: set it, enum V4L2 formats, configure and capture. */
    for (uint32_t i = 0; i < sensor_fmt_count; i++) {
        struct v4l2_fmtdesc fmtdesc;
        struct v4l2_format format;
        uint32_t v4l2_fmt_count = 0;

        memset(&sensor_enum, 0, sizeof(sensor_enum));
        sensor_enum.index = i;
        TEST_ESP_OK(ioctl(fd, VIDIOC_ENUM_SENSOR_FMT, &sensor_enum));
        TEST_ESP_OK(ioctl(fd, VIDIOC_S_SENSOR_FMT, &sensor_enum.format));

        memset(&current_fmt, 0, sizeof(current_fmt));
        TEST_ESP_OK(ioctl(fd, VIDIOC_G_SENSOR_FMT, &current_fmt));
        TEST_ASSERT_EQUAL_UINT16(sensor_enum.format.width, current_fmt.width);
        TEST_ASSERT_EQUAL_UINT16(sensor_enum.format.height, current_fmt.height);
        TEST_ASSERT_EQUAL_UINT8(sensor_enum.format.fps, current_fmt.fps);
        TEST_ASSERT_EQUAL_INT(sensor_enum.format.format, current_fmt.format);

        memset(&fmtdesc, 0, sizeof(fmtdesc));
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        for (fmtdesc.index = 0; ; fmtdesc.index++) {
            ret = ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc);
            if (ret != 0) {
                break;
            }
            printf("  [v4l2 fmt %" PRIu32 "] " V4L2_FMT_STR "\n",
                   fmtdesc.index, V4L2_FMT_STR_ARG(fmtdesc.pixelformat));
            v4l2_fmt_count++;
        }
        TEST_ASSERT_GREATER_THAN(0, v4l2_fmt_count);

        memset(&fmtdesc, 0, sizeof(fmtdesc));
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmtdesc.index = 0;
        TEST_ESP_OK(ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc));

        memset(&format, 0, sizeof(format));
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        TEST_ESP_OK(ioctl(fd, VIDIOC_G_FMT, &format));
        format.fmt.pix.pixelformat = fmtdesc.pixelformat;
        format.fmt.pix.width = sensor_enum.format.width;
        format.fmt.pix.height = sensor_enum.format.height;
        TEST_ESP_OK(ioctl(fd, VIDIOC_S_FMT, &format));

        printf("  capture with sensor[%" PRIu32 "] + " V4L2_FMT_STR " %" PRIu32 "x%" PRIu32 "\n",
               i,
               V4L2_FMT_STR_ARG(format.fmt.pix.pixelformat),
               format.fmt.pix.width,
               format.fmt.pix.height);
        capture_frames(fd);
    }

    close(fd);
    TEST_ESP_OK(example_video_deinit());
}

TEST_CASE("V4L2 set sensor format fails when buffers exist", "[video][sensor_format]")
{
    int fd;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    struct v4l2_requestbuffers req;
    struct v4l2_sensor_format_enum sensor_enum;

    setUp();
    TEST_ESP_OK(example_video_init());

    fd = open(TEST_APP_VIDEO_DEVICE, O_RDWR);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

    memset(&sensor_enum, 0, sizeof(sensor_enum));
    sensor_enum.index = 0;
    TEST_ESP_OK(ioctl(fd, VIDIOC_ENUM_SENSOR_FMT, &sensor_enum));

    memset(&req, 0, sizeof(req));
    req.count  = TEST_CAPTURE_BUF_COUNT;
    req.type   = type;
    req.memory = V4L2_MEMORY_MMAP;
    TEST_ESP_OK(ioctl(fd, VIDIOC_REQBUFS, &req));
    TEST_ASSERT_GREATER_THAN(0, req.count);

    errno = 0;
    TEST_ASSERT_EQUAL_INT(-1, ioctl(fd, VIDIOC_S_SENSOR_FMT, &sensor_enum.format));
    TEST_ASSERT_EQUAL_INT(EBUSY, errno);

    memset(&req, 0, sizeof(req));
    req.count  = 0;
    req.type   = type;
    req.memory = V4L2_MEMORY_MMAP;
    TEST_ESP_OK(ioctl(fd, VIDIOC_REQBUFS, &req));

    TEST_ESP_OK(ioctl(fd, VIDIOC_S_SENSOR_FMT, &sensor_enum.format));

    close(fd);
    TEST_ESP_OK(example_video_deinit());
}
