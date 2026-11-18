/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "unity.h"
#include "example_video_common.h"
#include "esp_cam_sensor.h"
#include "esp_video_device.h"
#include "esp_video_device_common.h"
#include "esp_video_ioctl.h"
#include "esp_video_pipeline_isp.h"

#if CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER && CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE

#define TEST_BUFFER_COUNT       3
#define TEST_CAPTURE_FRAMES     3
#define TEST_DQBUF_TIMEOUT_MS   1000
#define TEST_CSI_DEV_NAME       "MIPI-CSI"

void setUp(void);

static void set_dqbuf_timeout(int fd, uint32_t timeout_ms)
{
    struct timeval timeout;
    struct timeval got;

    memset(&timeout, 0, sizeof(timeout));
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    TEST_ESP_OK(ioctl(fd, VIDIOC_S_DQBUF_TIMEOUT, &timeout));

    memset(&got, 0, sizeof(got));
    TEST_ESP_OK(ioctl(fd, VIDIOC_G_DQBUF_TIMEOUT, &got));
    TEST_ASSERT_EQUAL_INT(timeout.tv_sec, got.tv_sec);
    TEST_ASSERT_EQUAL_INT(timeout.tv_usec, got.tv_usec);
}

static void get_capture_format(int fd, struct v4l2_format *format)
{
    memset(format, 0, sizeof(*format));
    format->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    TEST_ESP_OK(ioctl(fd, VIDIOC_G_FMT, format));
    TEST_ASSERT_GREATER_THAN_UINT32(0, format->fmt.pix.width);
    TEST_ASSERT_GREATER_THAN_UINT32(0, format->fmt.pix.height);
    TEST_ASSERT_NOT_EQUAL(0, format->fmt.pix.pixelformat);
}

static void expect_capture_format(int fd, const struct v4l2_format *expected)
{
    struct v4l2_format format;

    get_capture_format(fd, &format);
    TEST_ASSERT_EQUAL_UINT32(expected->fmt.pix.width, format.fmt.pix.width);
    TEST_ASSERT_EQUAL_UINT32(expected->fmt.pix.height, format.fmt.pix.height);
    TEST_ASSERT_EQUAL_UINT32(expected->fmt.pix.pixelformat, format.fmt.pix.pixelformat);
}

static void queue_capture_buffers(int fd)
{
    struct v4l2_requestbuffers req;
    struct v4l2_buffer buf;

    memset(&req, 0, sizeof(req));
    req.count = TEST_BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    TEST_ESP_OK(ioctl(fd, VIDIOC_REQBUFS, &req));

    for (int i = 0; i < TEST_BUFFER_COUNT; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        TEST_ESP_OK(ioctl(fd, VIDIOC_QUERYBUF, &buf));
        TEST_ESP_OK(ioctl(fd, VIDIOC_QBUF, &buf));
    }
}

static void capture_frames(int fd, int count)
{
    struct v4l2_buffer buf;

    for (int i = 0; i < count; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        TEST_ESP_OK(ioctl(fd, VIDIOC_DQBUF, &buf));
        TEST_ASSERT_GREATER_THAN_UINT32(0, buf.bytesused);
        TEST_ESP_OK(ioctl(fd, VIDIOC_QBUF, &buf));
    }
}

static void restart_isp_pipeline(void)
{
    esp_video_cam_t cam;
    esp_video_isp_config_t config;
    const esp_ipa_config_t *ipa_config;

    TEST_ESP_OK(esp_video_device_common_get_video_cam(TEST_CSI_DEV_NAME, &cam));
    TEST_ASSERT_NOT_NULL(cam.sensor);

    ipa_config = esp_ipa_pipeline_get_config(esp_cam_sensor_get_name(cam.sensor));
    TEST_ASSERT_NOT_NULL(ipa_config);

    memset(&config, 0, sizeof(config));
    config.cam_dev = ESP_VIDEO_MIPI_CSI_DEVICE_NAME;
    config.isp_dev = ESP_VIDEO_ISP1_DEVICE_NAME;
    config.ipa_config = ipa_config;
    TEST_ESP_OK(esp_video_isp_pipeline_init(&config));
    TEST_ASSERT_TRUE(esp_video_isp_pipeline_is_initialized());
}

/* Capture stays up across ISP pipeline stop and start. The camera fd stays open,
 * so deinit must not drop the device and the following init must not reset it. */
TEST_CASE("capture continues across ISP pipeline deinit and init", "[video][isp_pipeline]")
{
    int fd;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    struct v4l2_format format;

    setUp();

    TEST_ESP_OK(example_video_init());
    TEST_ASSERT_TRUE(esp_video_isp_pipeline_is_initialized());

    fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDWR);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

    set_dqbuf_timeout(fd, TEST_DQBUF_TIMEOUT_MS);
    get_capture_format(fd, &format);
    queue_capture_buffers(fd);
    TEST_ESP_OK(ioctl(fd, VIDIOC_STREAMON, &type));

    capture_frames(fd, TEST_CAPTURE_FRAMES);

    TEST_ESP_OK(esp_video_isp_pipeline_deinit());
    TEST_ASSERT_FALSE(esp_video_isp_pipeline_is_initialized());
    expect_capture_format(fd, &format);
    capture_frames(fd, TEST_CAPTURE_FRAMES);

    restart_isp_pipeline();
    expect_capture_format(fd, &format);
    capture_frames(fd, TEST_CAPTURE_FRAMES);

    TEST_ESP_OK(ioctl(fd, VIDIOC_STREAMOFF, &type));
    close(fd);
    TEST_ESP_OK(example_video_deinit());
}

#endif /* CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER && CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE */
