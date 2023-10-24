/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "memory_checks.h"
#include "unity_test_utils_memory.h"
#include "esp_video.h"

#define VIDEO_DEVICE_NAME       "sim_camera"
#define VIDEO_BUFFER_NUM        4
#define VIDEO_BUFFER_SIZE       6078
#define BUFFER_SIZE             128
#define FPS_DEFAULT             20

static bool s_inited;
static char buffer[BUFFER_SIZE];

static void init(void)
{
    if (!s_inited) {
        TEST_ESP_OK(esp_video_init());
        unity_utils_record_free_mem();
        test_utils_record_free_mem();
        s_inited = true;
    }
}

TEST_CASE("video basic operation", "[video]")
{
    struct esp_video *video;
    struct esp_video_capability capability;
    int fps;
    int count;
    TickType_t tick;
    size_t recv_size;

    /* Initialize esp-video system */

    init();

    video = esp_video_open(VIDEO_DEVICE_NAME);
    TEST_ASSERT_NOT_NULL(video);

    TEST_ESP_OK(esp_video_get_capability(video, &capability));
    TEST_ASSERT_TRUE(capability.fmt_jpeg);

    TEST_ESP_OK(esp_video_get_description(video, buffer, BUFFER_SIZE));

    TEST_ESP_OK(esp_video_get_attr(video, ESP_VIDEO_CTRL_FPS, &fps));
    TEST_ASSERT_EQUAL_INT(FPS_DEFAULT, fps);

    /* Test receiving picture from video device in default FPS(20)  */

    TEST_ESP_OK(esp_video_start_capture(video));

    count = 0;
    tick = xTaskGetTickCount();
    while (xTaskGetTickCount() - tick < (1000 / portTICK_PERIOD_MS)) {
        uint8_t *buffer = esp_video_recv_buffer(video, &recv_size, 100);
        if (buffer) {
            count++;
            esp_video_free_buffer(video, buffer);
            TEST_ASSERT_EQUAL_INT(VIDEO_BUFFER_SIZE, recv_size);
        } else {
            break;
        }
    }
    TEST_ASSERT_GREATER_OR_EQUAL(fps - 1, count);

    TEST_ESP_OK(esp_video_stop_capture(video));

    /* Test receiving picture from video device in FPS = 30 */

    fps = 30;
    TEST_ESP_OK(esp_video_set_attr(video, ESP_VIDEO_CTRL_FPS, &fps));
    TEST_ESP_OK(esp_video_get_attr(video, ESP_VIDEO_CTRL_FPS, &fps));
    TEST_ASSERT_EQUAL_INT(30, fps);

    TEST_ESP_OK(esp_video_start_capture(video));

    count = 0;
    tick = xTaskGetTickCount();
    while (xTaskGetTickCount() - tick < (1000 / portTICK_PERIOD_MS)) {
        uint8_t *buffer = esp_video_recv_buffer(video, &recv_size, 100);
        if (buffer) {
            count++;
            esp_video_free_buffer(video, buffer);
            TEST_ASSERT_EQUAL_INT(VIDEO_BUFFER_SIZE, recv_size);
        } else {
            break;
        }
    }
    TEST_ASSERT_GREATER_OR_EQUAL(fps - 1, count);

    TEST_ESP_OK(esp_video_stop_capture(video));

    TEST_ESP_OK(esp_video_close(video));

    /* After closing video device, all buffer elements should be free */

    TEST_ASSERT_EQUAL_INT(VIDEO_BUFFER_NUM, esp_video_buffer_get_element_num(video->buffer));
}
