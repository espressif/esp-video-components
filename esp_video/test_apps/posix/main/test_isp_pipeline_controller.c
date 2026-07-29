/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "sdkconfig.h"

#include "example_video_common.h"
#include "esp_video_isp_pipeline.h"
#include "esp_video_isp_ioctl.h"

#if CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER && CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE

/**
 * Set to 1 to print dumped ISP statistics (AE luminance grid, AWB, etc.).
 * Default is off to keep CI/log output quiet.
 */
#ifndef TEST_ISP_PIPELINE_DUMP_STATS_PRINT
#define TEST_ISP_PIPELINE_DUMP_STATS_PRINT  0
#endif

#define TEST_APP_VIDEO_DEVICE           EXAMPLE_CAM_DEV_PATH
#define TEST_ISP_STATS_QUEUE_SIZE       4
#define TEST_ISP_STATS_DUMP_COUNT       30
#define TEST_ISP_STATS_DUMP_TIMEOUT_MS  1000
#define TEST_VIDEO_BUFFER_COUNT         3

void setUp(void);

#if TEST_ISP_PIPELINE_DUMP_STATS_PRINT
static void print_isp_stats(const esp_video_isp_stats_t *stats)
{
    printf("ISP stats seq=%" PRIu64 " flags=0x%" PRIx32 "\n", stats->seq, stats->flags);

    if (stats->flags & ESP_VIDEO_ISP_STATS_FLAG_AE) {
        const isp_ae_result_t *ae = &stats->ae.ae_result;

        printf("AE luminance:\n");
        for (int i = 0; i < ISP_AE_BLOCK_X_NUM; i++) {
            char print_buf[ISP_AE_BLOCK_Y_NUM * 6];
            uint32_t offset = 0;

            for (int j = 0; j < ISP_AE_BLOCK_Y_NUM; j++) {
                int ret = snprintf(print_buf + offset, sizeof(print_buf) - offset,
                                   " %3d", ae->luminance[i][j]);
                if (ret > 0) {
                    offset += ret;
                }
            }
            printf("  [%s ]\n", print_buf);
        }
    }

    if (stats->flags & ESP_VIDEO_ISP_STATS_FLAG_AWB) {
        const isp_awb_stat_result_t *awb = &stats->awb.awb_result;

        printf("AWB white_patch_num=%" PRIu32 " sum_r=%" PRIu32 " sum_g=%" PRIu32 " sum_b=%" PRIu32 "\n",
               awb->white_patch_num, awb->sum_r, awb->sum_g, awb->sum_b);
    }

    if (stats->flags & ESP_VIDEO_ISP_STATS_FLAG_HIST) {
        printf("HIST:");
        for (int i = 0; i < ISP_HIST_SEGMENT_NUMS; i++) {
            printf(" %u", (unsigned)stats->hist.hist_result.hist_value[i]);
        }
        printf("\n");
    }

    if (stats->flags & ESP_VIDEO_ISP_STATS_FLAG_AF) {
        printf("AF:\n");
        for (int i = 0; i < ISP_AF_WINDOW_NUM; i++) {
            printf("  win[%d] definition=%d luminance=%d\n",
                   i, stats->af.af_result.definition[i], stats->af.af_result.luminance[i]);
        }
    }
}
#endif /* TEST_ISP_PIPELINE_DUMP_STATS_PRINT */

TEST_CASE("ISP pipeline AGC set/get", "[video][isp_pipeline]")
{
    esp_video_isp_pipeline_agc_status_t status;

    setUp();

    TEST_ESP_OK(example_video_init());

    TEST_ESP_OK(esp_video_isp_pipeline_get_agc_status(&status));
    TEST_ASSERT_EQUAL_INT(ESP_VIDEO_ISP_PIPELINE_AGC_ENABLE, status);

    TEST_ESP_OK(esp_video_isp_pipeline_set_agc_status(ESP_VIDEO_ISP_PIPELINE_AGC_DISABLE));
    TEST_ESP_OK(esp_video_isp_pipeline_get_agc_status(&status));
    TEST_ASSERT_EQUAL_INT(ESP_VIDEO_ISP_PIPELINE_AGC_DISABLE, status);

    TEST_ESP_OK(esp_video_isp_pipeline_set_agc_status(ESP_VIDEO_ISP_PIPELINE_AGC_ENABLE));
    TEST_ESP_OK(esp_video_isp_pipeline_get_agc_status(&status));
    TEST_ASSERT_EQUAL_INT(ESP_VIDEO_ISP_PIPELINE_AGC_ENABLE, status);

    TEST_ESP_OK(example_video_deinit());
}

TEST_CASE("ISP pipeline dump stats after MIPI-CSI stream on", "[video][isp_pipeline]")
{
    int fd;
    int ret;
    int type;
    int dump_ok_count = 0;
    struct v4l2_buffer buf;
    struct v4l2_requestbuffers req;
    esp_video_isp_stats_t stats;
    uint64_t last_seq = 0;
    bool has_last_seq = false;

    setUp();

    TEST_ESP_OK(example_video_init());

    fd = open(TEST_APP_VIDEO_DEVICE, O_RDWR);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

    memset(&req, 0, sizeof(req));
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    req.count  = TEST_VIDEO_BUFFER_COUNT;
    ret = ioctl(fd, VIDIOC_REQBUFS, &req);
    TEST_ESP_OK(ret);

    for (int i = 0; i < TEST_VIDEO_BUFFER_COUNT; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        ret = ioctl(fd, VIDIOC_QUERYBUF, &buf);
        TEST_ESP_OK(ret);

        ret = ioctl(fd, VIDIOC_QBUF, &buf);
        TEST_ESP_OK(ret);
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd, VIDIOC_STREAMON, &type);
    TEST_ESP_OK(ret);

    TEST_ESP_OK(esp_video_isp_pipeline_start_dump_stats(TEST_ISP_STATS_QUEUE_SIZE));
    /* start twice should fail */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_video_isp_pipeline_start_dump_stats(TEST_ISP_STATS_QUEUE_SIZE));

    for (int i = 0; i < TEST_ISP_STATS_DUMP_COUNT; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(fd, VIDIOC_DQBUF, &buf);
        TEST_ESP_OK(ret);

        memset(&stats, 0, sizeof(stats));
        ret = esp_video_isp_pipeline_dump_stats(&stats, TEST_ISP_STATS_DUMP_TIMEOUT_MS);
        if (ret == ESP_OK) {
            dump_ok_count++;
            TEST_ASSERT_NOT_EQUAL(0, stats.flags);

            if (has_last_seq) {
                TEST_ASSERT_GREATER_THAN_UINT64(last_seq, stats.seq);
            }
            last_seq = stats.seq;
            has_last_seq = true;

#if TEST_ISP_PIPELINE_DUMP_STATS_PRINT
            print_isp_stats(&stats);
#endif
        } else {
            TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, ret);
        }

        ret = ioctl(fd, VIDIOC_QBUF, &buf);
        TEST_ESP_OK(ret);
    }

    TEST_ASSERT_GREATER_THAN(0, dump_ok_count);

    TEST_ESP_OK(esp_video_isp_pipeline_stop_dump_stats());
    /* stop twice should fail */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_video_isp_pipeline_stop_dump_stats());

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd, VIDIOC_STREAMOFF, &type);
    TEST_ESP_OK(ret);

    close(fd);

    TEST_ESP_OK(example_video_deinit());
}

TEST_CASE("ISP pipeline dump stats invalid args", "[video][isp_pipeline]")
{
    esp_video_isp_stats_t stats;

    setUp();

    TEST_ESP_OK(example_video_init());

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_video_isp_pipeline_start_dump_stats(0));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_video_isp_pipeline_stop_dump_stats());
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_video_isp_pipeline_dump_stats(&stats, 10));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_video_isp_pipeline_dump_stats(NULL, 10));

    TEST_ESP_OK(esp_video_isp_pipeline_start_dump_stats(2));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_video_isp_pipeline_dump_stats(NULL, 10));
    TEST_ESP_OK(esp_video_isp_pipeline_stop_dump_stats());

    TEST_ESP_OK(example_video_deinit());
}

#endif /* CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER && CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE */
