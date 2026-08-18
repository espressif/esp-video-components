/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "unity.h"

#include "example_video_common.h"
#include "esp_video_ioctl.h"
#include "esp_video_isp_ioctl.h"
#if CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER
#include "esp_video_isp_pipeline.h"
#include "esp_video_pipeline_isp.h"
#endif

static void fill_isp_window(isp_window_t *window, uint32_t left, uint32_t top,
                            uint32_t right, uint32_t bottom)
{
    window->top_left.x = left;
    window->top_left.y = top;
    window->btm_right.x = right;
    window->btm_right.y = bottom;
}

static void assert_isp_window_equal(const isp_window_t *expected, const isp_window_t *actual)
{
    TEST_ASSERT_EQUAL_UINT32(expected->top_left.x, actual->top_left.x);
    TEST_ASSERT_EQUAL_UINT32(expected->top_left.y, actual->top_left.y);
    TEST_ASSERT_EQUAL_UINT32(expected->btm_right.x, actual->btm_right.x);
    TEST_ASSERT_EQUAL_UINT32(expected->btm_right.y, actual->btm_right.y);
}

#if CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER
static void assert_isp_pipeline_stats_windows(int isp_fd, uint32_t left, uint32_t top,
        uint32_t right, uint32_t bottom)
{
    int ret;
    struct v4l2_ext_controls ctrls;
    struct v4l2_ext_control ctrl[1];

    memset(&ctrls, 0, sizeof(ctrls));
    ctrls.ctrl_class = V4L2_CID_USER_CLASS;
    ctrls.count = 1;
    ctrls.controls = ctrl;

    esp_video_isp_awb_t awb;
    ctrl[0].id = V4L2_CID_USER_ESP_ISP_AWB;
    ctrl[0].size = sizeof(awb);
    ctrl[0].p_u8 = (uint8_t *)&awb;
    ret = ioctl(isp_fd, VIDIOC_G_EXT_CTRLS, &ctrls);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL_UINT32(left, awb.windows[0].top_left.x);
    TEST_ASSERT_EQUAL_UINT32(top, awb.windows[0].top_left.y);
    TEST_ASSERT_EQUAL_UINT32(right, awb.windows[0].btm_right.x);
    TEST_ASSERT_EQUAL_UINT32(bottom, awb.windows[0].btm_right.y);

    esp_video_isp_ae_t ae;
    ctrl[0].id = V4L2_CID_USER_ESP_ISP_AE;
    ctrl[0].size = sizeof(ae);
    ctrl[0].p_u8 = (uint8_t *)&ae;
    ret = ioctl(isp_fd, VIDIOC_G_EXT_CTRLS, &ctrls);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL_UINT32(left, ae.windows[0].top_left.x);
    TEST_ASSERT_EQUAL_UINT32(top, ae.windows[0].top_left.y);
    TEST_ASSERT_EQUAL_UINT32(right, ae.windows[0].btm_right.x);
    TEST_ASSERT_EQUAL_UINT32(bottom, ae.windows[0].btm_right.y);

    esp_video_isp_hist_t hist;
    ctrl[0].id = V4L2_CID_USER_ESP_ISP_HIST;
    ctrl[0].size = sizeof(hist);
    ctrl[0].p_u8 = (uint8_t *)&hist;
    ret = ioctl(isp_fd, VIDIOC_G_EXT_CTRLS, &ctrls);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL_UINT32(left, hist.windows[0].top_left.x);
    TEST_ASSERT_EQUAL_UINT32(top, hist.windows[0].top_left.y);
    TEST_ASSERT_EQUAL_UINT32(right, hist.windows[0].btm_right.x);
    TEST_ASSERT_EQUAL_UINT32(bottom, hist.windows[0].btm_right.y);

    esp_video_isp_af_t af;
    ctrl[0].id = V4L2_CID_USER_ESP_ISP_AF;
    ctrl[0].size = sizeof(af);
    ctrl[0].p_u8 = (uint8_t *)&af;
    ret = ioctl(isp_fd, VIDIOC_G_EXT_CTRLS, &ctrls);
    TEST_ESP_OK(ret);
    for (int i = 0; i < ISP_AF_WINDOW_NUM; i++) {
        TEST_ASSERT_EQUAL_UINT32(left, af.windows[i].top_left.x);
        TEST_ASSERT_EQUAL_UINT32(top, af.windows[i].top_left.y);
        TEST_ASSERT_EQUAL_UINT32(right, af.windows[i].btm_right.x);
        TEST_ASSERT_EQUAL_UINT32(bottom, af.windows[i].btm_right.y);
    }
}

static void set_and_verify_isp_pipeline_stats_windows(int isp_fd, uint32_t target_windows,
        uint32_t left, uint32_t top,
        uint32_t width, uint32_t height)
{
    TEST_ESP_OK(esp_video_isp_pipeline_set_statistics_window(target_windows, left, top, width, height));
    assert_isp_pipeline_stats_windows(isp_fd, left, top, left + width - 1, top + height - 1);
}
#endif /* CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER */

TEST_CASE("V4L2 set/get GAMMA_EXT", "[video]")
{
    int fd;
    int ret;
    struct v4l2_ext_controls ctrls;
    struct v4l2_ext_control ctrl[1];
    esp_video_isp_gamma_ext_t gamma_set;
    esp_video_isp_gamma_ext_t gamma_get;

    setUp();

    TEST_ESP_OK(example_video_init());

    fd = open(ESP_VIDEO_ISP1_DEVICE_NAME, O_RDWR);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

    /* Set GAMMA_EXT with distinct R/G/B channel points */
    memset(&gamma_set, 0, sizeof(gamma_set));
    gamma_set.enable = true;
    gamma_set.flags = ESP_VIDEO_ISP_GAMMA_EXT_FLAG_RED | ESP_VIDEO_ISP_GAMMA_EXT_FLAG_GREEN | ESP_VIDEO_ISP_GAMMA_EXT_FLAG_BLUE;
    for (int i = 0; i < ISP_GAMMA_CURVE_POINTS_NUM; i++) {
        gamma_set.red_points[i].x   = (uint8_t)(i * 16);
        gamma_set.red_points[i].y   = (uint8_t)(i * 2);
        gamma_set.green_points[i].x = (uint8_t)(i * 16);
        gamma_set.green_points[i].y = (uint8_t)(i * 4);
        gamma_set.blue_points[i].x  = (uint8_t)(i * 16);
        gamma_set.blue_points[i].y  = (uint8_t)(i * 8);
    }

    memset(&ctrls, 0, sizeof(ctrls));
    ctrls.ctrl_class = V4L2_CID_USER_CLASS;
    ctrls.count      = 1;
    ctrls.controls   = ctrl;
    ctrl[0].id       = V4L2_CID_USER_ESP_ISP_GAMMA_EXT;
    ctrl[0].size     = sizeof(esp_video_isp_gamma_ext_t);
    ctrl[0].p_u8     = (uint8_t *)&gamma_set;

    ret = ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls);
    TEST_ESP_OK(ret);

    /* Get GAMMA_EXT and verify it matches what we set */
    memset(&gamma_get, 0, sizeof(gamma_get));
    ctrl[0].p_u8 = (uint8_t *)&gamma_get;

    ret = ioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls);
    TEST_ESP_OK(ret);

    TEST_ASSERT_EQUAL(gamma_set.enable, gamma_get.enable);
    TEST_ASSERT_EQUAL_INT(0, memcmp(gamma_set.red_points, gamma_get.red_points,
                                    sizeof(esp_video_isp_gamma_point_t) * ISP_GAMMA_CURVE_POINTS_NUM));
    TEST_ASSERT_EQUAL_INT(0, memcmp(gamma_set.green_points, gamma_get.green_points,
                                    sizeof(esp_video_isp_gamma_point_t) * ISP_GAMMA_CURVE_POINTS_NUM));
    TEST_ASSERT_EQUAL_INT(0, memcmp(gamma_set.blue_points, gamma_get.blue_points,
                                    sizeof(esp_video_isp_gamma_point_t) * ISP_GAMMA_CURVE_POINTS_NUM));

    /* Set enable = false and verify get */
    gamma_set.enable = false;
    ctrl[0].p_u8 = (uint8_t *)&gamma_set;
    ret = ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls);
    TEST_ESP_OK(ret);

    memset(&gamma_get, 0, sizeof(gamma_get));
    ctrl[0].p_u8 = (uint8_t *)&gamma_get;
    ret = ioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls);
    TEST_ESP_OK(ret);
    TEST_ASSERT_FALSE(gamma_get.enable);

    close(fd);

    TEST_ESP_OK(example_video_deinit());
}

TEST_CASE("V4L2 set/get AWB/AE/AF/HIST statistics windows", "[video]")
{
    int fd;
    int ret;
    struct v4l2_ext_controls ctrls;
    struct v4l2_ext_control ctrl[1];
    esp_video_isp_awb_t awb_set;
    esp_video_isp_awb_t awb_get;
    esp_video_isp_ae_t ae_set;
    esp_video_isp_ae_t ae_get;
    esp_video_isp_af_t af_set;
    esp_video_isp_af_t af_get;
    esp_video_isp_hist_t hist_set;
    esp_video_isp_hist_t hist_get;

    setUp();

    TEST_ESP_OK(example_video_init());

    fd = open(ESP_VIDEO_ISP1_DEVICE_NAME, O_RDWR);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

    memset(&ctrls, 0, sizeof(ctrls));
    ctrls.ctrl_class = V4L2_CID_USER_CLASS;
    ctrls.count      = 1;
    ctrls.controls   = ctrl;

    /* AWB: set window and ranges, then get back */
    memset(&awb_set, 0, sizeof(awb_set));
    awb_set.enable = true;
    awb_set.green_min = 0;
    awb_set.green_max = 200;
    awb_set.rg_min = 0.5f;
    awb_set.rg_max = 2.0f;
    awb_set.bg_min = 0.5f;
    awb_set.bg_max = 2.0f;
    fill_isp_window(&awb_set.windows[0], 16, 32, 320, 240);

    ctrl[0].id   = V4L2_CID_USER_ESP_ISP_AWB;
    ctrl[0].size = sizeof(esp_video_isp_awb_t);
    ctrl[0].p_u8 = (uint8_t *)&awb_set;
    ret = ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls);
    TEST_ESP_OK(ret);

    memset(&awb_get, 0, sizeof(awb_get));
    ctrl[0].p_u8 = (uint8_t *)&awb_get;
    ret = ioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls);
    TEST_ESP_OK(ret);

    TEST_ASSERT_TRUE(awb_get.enable);
    TEST_ASSERT_EQUAL_UINT8(awb_set.green_min, awb_get.green_min);
    TEST_ASSERT_EQUAL_UINT8(awb_set.green_max, awb_get.green_max);
    TEST_ASSERT_EQUAL_FLOAT(awb_set.rg_min, awb_get.rg_min);
    TEST_ASSERT_EQUAL_FLOAT(awb_set.rg_max, awb_get.rg_max);
    TEST_ASSERT_EQUAL_FLOAT(awb_set.bg_min, awb_get.bg_min);
    TEST_ASSERT_EQUAL_FLOAT(awb_set.bg_max, awb_get.bg_max);
    assert_isp_window_equal(&awb_set.windows[0], &awb_get.windows[0]);

    /* AE: set window, then get back */
    memset(&ae_set, 0, sizeof(ae_set));
    ae_set.enable = true;
    fill_isp_window(&ae_set.windows[0], 8, 16, 200, 180);

    ctrl[0].id   = V4L2_CID_USER_ESP_ISP_AE;
    ctrl[0].size = sizeof(esp_video_isp_ae_t);
    ctrl[0].p_u8 = (uint8_t *)&ae_set;
    ret = ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls);
    TEST_ESP_OK(ret);

    memset(&ae_get, 0, sizeof(ae_get));
    ctrl[0].p_u8 = (uint8_t *)&ae_get;
    ret = ioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls);
    TEST_ESP_OK(ret);

    TEST_ASSERT_TRUE(ae_get.enable);
    assert_isp_window_equal(&ae_set.windows[0], &ae_get.windows[0]);

    /* AF: set windows and edge threshold, then get back */
    memset(&af_set, 0, sizeof(af_set));
    af_set.enable = true;
    af_set.edge_thresh = 32;
    for (int i = 0; i < ISP_AF_WINDOW_NUM; i++) {
        fill_isp_window(&af_set.windows[i], 10 + i * 20, 20 + i * 10, 100 + i * 20, 120 + i * 10);
    }

    ctrl[0].id   = V4L2_CID_USER_ESP_ISP_AF;
    ctrl[0].size = sizeof(esp_video_isp_af_t);
    ctrl[0].p_u8 = (uint8_t *)&af_set;
    ret = ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls);
    TEST_ESP_OK(ret);

    memset(&af_get, 0, sizeof(af_get));
    ctrl[0].p_u8 = (uint8_t *)&af_get;
    ret = ioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls);
    TEST_ESP_OK(ret);

    TEST_ASSERT_TRUE(af_get.enable);
    TEST_ASSERT_EQUAL_UINT32(af_set.edge_thresh, af_get.edge_thresh);
    for (int i = 0; i < ISP_AF_WINDOW_NUM; i++) {
        assert_isp_window_equal(&af_set.windows[i], &af_get.windows[i]);
    }

    /* HIST: set window, then get back */
    memset(&hist_set, 0, sizeof(hist_set));
    hist_set.enable = true;
    fill_isp_window(&hist_set.windows[0], 24, 40, 280, 220);

    ctrl[0].id   = V4L2_CID_USER_ESP_ISP_HIST;
    ctrl[0].size = sizeof(esp_video_isp_hist_t);
    ctrl[0].p_u8 = (uint8_t *)&hist_set;
    ret = ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls);
    TEST_ESP_OK(ret);

    memset(&hist_get, 0, sizeof(hist_get));
    ctrl[0].p_u8 = (uint8_t *)&hist_get;
    ret = ioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls);
    TEST_ESP_OK(ret);

    TEST_ASSERT_TRUE(hist_get.enable);
    assert_isp_window_equal(&hist_set.windows[0], &hist_get.windows[0]);

    /* Disable AE/HIST/AWB/AF and verify enable flags */
    ae_set.enable = false;
    ctrl[0].id = V4L2_CID_USER_ESP_ISP_AE;
    ctrl[0].size = sizeof(esp_video_isp_ae_t);
    ctrl[0].p_u8 = (uint8_t *)&ae_set;
    TEST_ESP_OK(ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls));
    memset(&ae_get, 0, sizeof(ae_get));
    ctrl[0].p_u8 = (uint8_t *)&ae_get;
    TEST_ESP_OK(ioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls));
    TEST_ASSERT_FALSE(ae_get.enable);

    hist_set.enable = false;
    ctrl[0].id = V4L2_CID_USER_ESP_ISP_HIST;
    ctrl[0].size = sizeof(esp_video_isp_hist_t);
    ctrl[0].p_u8 = (uint8_t *)&hist_set;
    TEST_ESP_OK(ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls));
    memset(&hist_get, 0, sizeof(hist_get));
    ctrl[0].p_u8 = (uint8_t *)&hist_get;
    TEST_ESP_OK(ioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls));
    TEST_ASSERT_FALSE(hist_get.enable);

    awb_set.enable = false;
    ctrl[0].id = V4L2_CID_USER_ESP_ISP_AWB;
    ctrl[0].size = sizeof(esp_video_isp_awb_t);
    ctrl[0].p_u8 = (uint8_t *)&awb_set;
    TEST_ESP_OK(ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls));
    memset(&awb_get, 0, sizeof(awb_get));
    ctrl[0].p_u8 = (uint8_t *)&awb_get;
    TEST_ESP_OK(ioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls));
    TEST_ASSERT_FALSE(awb_get.enable);

    af_set.enable = false;
    ctrl[0].id = V4L2_CID_USER_ESP_ISP_AF;
    ctrl[0].size = sizeof(esp_video_isp_af_t);
    ctrl[0].p_u8 = (uint8_t *)&af_set;
    TEST_ESP_OK(ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls));
    memset(&af_get, 0, sizeof(af_get));
    ctrl[0].p_u8 = (uint8_t *)&af_get;
    TEST_ESP_OK(ioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls));
    TEST_ASSERT_FALSE(af_get.enable);

    close(fd);

    TEST_ESP_OK(example_video_deinit());
}

#if CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER
TEST_CASE("ISP pipeline set statistics window", "[video]")
{
    int fd;
    const uint32_t left = 12;
    const uint32_t top = 24;
    const uint32_t width = 160;
    const uint32_t height = 120;
    const uint32_t target_windows = ESP_VIDEO_ISP_AF_STATS_WIN |
                                    ESP_VIDEO_ISP_AWB_STATS_WIN |
                                    ESP_VIDEO_ISP_AE_STATS_WIN |
                                    ESP_VIDEO_ISP_HIST_STATS_WIN;

    setUp();

    /* Invalid arguments should fail without depending on pipeline state */
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG,
                 esp_video_isp_pipeline_set_statistics_window(0, left, top, width, height));
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG,
                 esp_video_isp_pipeline_set_statistics_window(target_windows, left, top, 0, height));
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG,
                 esp_video_isp_pipeline_set_statistics_window(target_windows, left, top, width, 0));
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG,
                 esp_video_isp_pipeline_set_statistics_window(0xffffffff, left, top, width, height));

    TEST_ESP_OK(example_video_init());

    if (!esp_video_isp_pipeline_is_initialized()) {
        TEST_ESP_ERR(ESP_ERR_INVALID_STATE,
                     esp_video_isp_pipeline_set_statistics_window(target_windows, left, top, width, height));
        TEST_ESP_OK(example_video_deinit());
        TEST_IGNORE_MESSAGE("ISP pipeline controller is not initialized");
    }

    fd = open(ESP_VIDEO_ISP1_DEVICE_NAME, O_RDWR);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

    set_and_verify_isp_pipeline_stats_windows(fd, target_windows, left, top, width, height);

    close(fd);

    TEST_ESP_OK(example_video_deinit());
}

#if CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE
TEST_CASE("ISP pipeline set/get statistics window around MIPI-CSI stream", "[video]")
{
    int csi_fd;
    int isp_fd;
    int val;
    struct v4l2_buffer buf;
    struct v4l2_requestbuffers req;
    const int buffer_count = 2;
    const uint32_t target_windows = ESP_VIDEO_ISP_AF_STATS_WIN |
                                    ESP_VIDEO_ISP_AWB_STATS_WIN |
                                    ESP_VIDEO_ISP_AE_STATS_WIN |
                                    ESP_VIDEO_ISP_HIST_STATS_WIN;

    setUp();

    TEST_ESP_OK(example_video_init());

    if (!esp_video_isp_pipeline_is_initialized()) {
        TEST_ESP_OK(example_video_deinit());
        TEST_IGNORE_MESSAGE("ISP pipeline controller is not initialized");
    }

    csi_fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDWR);
    TEST_ASSERT_GREATER_OR_EQUAL(0, csi_fd);

    isp_fd = open(ESP_VIDEO_ISP1_DEVICE_NAME, O_RDWR);
    TEST_ASSERT_GREATER_OR_EQUAL(0, isp_fd);

    memset(&req, 0, sizeof(req));
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    req.count = buffer_count;
    TEST_ESP_OK(ioctl(csi_fd, VIDIOC_REQBUFS, &req));

    for (int i = 0; i < buffer_count; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        TEST_ESP_OK(ioctl(csi_fd, VIDIOC_QUERYBUF, &buf));
        TEST_ESP_OK(ioctl(csi_fd, VIDIOC_QBUF, &buf));
    }

    /* Scenario 1: configure before MIPI-CSI stream_on */
    set_and_verify_isp_pipeline_stats_windows(isp_fd, target_windows, 12, 24, 160, 120);

    val = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    TEST_ESP_OK(ioctl(csi_fd, VIDIOC_STREAMON, &val));

    /* Scenario 2: configure after MIPI-CSI stream_on */
    set_and_verify_isp_pipeline_stats_windows(isp_fd, target_windows, 20, 40, 200, 150);

    TEST_ESP_OK(ioctl(csi_fd, VIDIOC_STREAMOFF, &val));

    /* Scenario 3: configure after MIPI-CSI stream_off */
    set_and_verify_isp_pipeline_stats_windows(isp_fd, target_windows, 8, 16, 128, 96);

    close(isp_fd);
    close(csi_fd);

    TEST_ESP_OK(example_video_deinit());
}
#endif /* CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE */
#endif /* CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER */
