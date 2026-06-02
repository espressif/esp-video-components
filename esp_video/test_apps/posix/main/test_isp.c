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
#include "unity.h"

#include "example_video_common.h"
#include "esp_video_ioctl.h"
#include "esp_video_isp_ioctl.h"

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
