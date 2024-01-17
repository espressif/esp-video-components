/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "esp_heap_caps.h"
#ifdef CONFIG_HEAP_TRACING
#include "esp_heap_trace.h"
#endif

#include "memory_checks.h"
#include "unity_test_utils_memory.h"
#include "unity.h"

#include "linux/videodev2.h"
#include "esp_video.h"

struct file_list_t {
    char *config;
    char *name;
};

#define GOOD_TEST_FILES         "good_lists.h"
#define BAD_TEST_FILES          "bad_lists.h"

#ifdef GOOD_TEST_FILES
#include GOOD_TEST_FILES
#endif

#ifdef BAD_TEST_FILES
#include BAD_TEST_FILES
#endif

#define TEST_MEMORY_LEAK_THRESHOLD (-100)

void setUp(void);

TEST_CASE("Video Pipeline Load Bad Configs List", "[video]")
{
    esp_err_t ret;

#ifdef CONFIG_HEAP_TRACING
    heap_trace_record_t recs[HEAP_RES_NUM];

    heap_trace_init_standalone(recs, HEAP_RES_NUM);
    heap_trace_start(HEAP_TRACE_LEAKS);
#endif
    setUp();

#ifdef BAD_TEST_FILES
    for (int i = 0; i < sizeof(bad_file_lists) / sizeof(bad_file_lists[0]); i++) {
        if (bad_file_lists[i].config) {
            printf("Test %s config...\r\n", bad_file_lists[i].name);
            ret = esp_media_config_loader(bad_file_lists[i].config);
            TEST_ASSERT_NOT_EQUAL_INT(ESP_OK, ret);
            printf("Tested %s config successfully\r\n", bad_file_lists[i].name);
        }
    }
#endif

#ifdef CONFIG_HEAP_TRACING
    heap_trace_dump();
    heap_trace_stop();
#endif
}

#define VIDEO_COUNT             2
static void init(void)
{
    static bool s_inited;
    if (!s_inited) {
        static const esp_camera_sccb_config_t s_sccb_config[] = {
            {
                .i2c_or_i3c = CONFIG_ESP_VIDEO_CAMERA_SCCB0_I2C_I3C,
                .scl_pin    = CONFIG_ESP_VIDEO_CAMERA_SCCB0_I2C_I3C_SCL_PIN,
                .sda_pin    = CONFIG_ESP_VIDEO_CAMERA_SCCB0_I2C_I3C_SDA_PIN,
                .port       = CONFIG_ESP_VIDEO_CAMERA_SCCB0_I2C_I3C_PORT,
                .freq       = CONFIG_ESP_VIDEO_CAMERA_SCCB0_I2C_I3C_FREQ,
            }
        };
#ifdef CONFIG_MIPI_CSI_ENABLE
        static const esp_camera_csi_config_t s_csi_config[] = {
            {
                .ctrl_cfg = {
                    .sccb_config_index = CONFIG_ESP_VIDEO_CAMERA_CSI0_SCCB_INDEX,
                    .xclk_pin          = CONFIG_ESP_VIDEO_CAMERA_CSI0_XCLK_PIN,
                    .reset_pin         = CONFIG_ESP_VIDEO_CAMERA_CSI0_RESET_PIN,
                    .pwdn_pin          = CONFIG_ESP_VIDEO_CAMERA_CSI0_PWDN_PIN,
                    .xclk_freq_hz      = CONFIG_ESP_VIDEO_CAMERA_CSI0_XCLK_FREQ,
                }
            }
        };
#endif

#if CONFIG_DVP_ENABLE
        static const esp_camera_dvp_config_t s_dvp_config[] = {
            {
                .ctrl_cfg = {
                    .sccb_config_index = CONFIG_ESP_VIDEO_CAMERA_DVP0_SCCB_INDEX,
                    .reset_pin         = CONFIG_ESP_VIDEO_CAMERA_DVP0_RESET_PIN,
                    .pwdn_pin          = CONFIG_ESP_VIDEO_CAMERA_DVP0_PWDN_PIN,
                    .xclk_freq_hz      = CONFIG_ESP_VIDEO_CAMERA_DVP0_XCLK_FREQ,
#ifndef CONFIG_DVP_ENABLE_OUTPUT_CLOCK
                    .xclk_timer        = LEDC_TIMER_0,
                    .xclk_timer_channel = LEDC_CHANNEL_0,
#endif
                }
                ,
                .dvp_pin_cfg = {
                    .data_pin = {
                        CONFIG_ESP_VIDEO_CAMERA_DVP0_D0_PIN, CONFIG_ESP_VIDEO_CAMERA_DVP0_D1_PIN,
                        CONFIG_ESP_VIDEO_CAMERA_DVP0_D2_PIN, CONFIG_ESP_VIDEO_CAMERA_DVP0_D3_PIN,
                        CONFIG_ESP_VIDEO_CAMERA_DVP0_D4_PIN, CONFIG_ESP_VIDEO_CAMERA_DVP0_D5_PIN,
                        CONFIG_ESP_VIDEO_CAMERA_DVP0_D6_PIN, CONFIG_ESP_VIDEO_CAMERA_DVP0_D7_PIN,
                    },
                    .vsync_pin = CONFIG_ESP_VIDEO_CAMERA_DVP0_VSYNC_PIN,
                    .href_pin = CONFIG_ESP_VIDEO_CAMERA_DVP0_HREF_PIN,
                    .pclk_pin = CONFIG_ESP_VIDEO_CAMERA_DVP0_PCLK_PIN,
                    .xclk_pin = CONFIG_ESP_VIDEO_CAMERA_DVP0_XCLK_PIN,
                }
            }
        };
#endif
#ifdef CONFIG_SIMULATED_INTF
        const esp_camera_sim_config_t s_sim_config[2] = {
            {.id = 0},
            {.id = 1}
        };
#endif
        const esp_camera_config_t s_cam_config = {
            .sccb_num = 1,
            .sccb     = s_sccb_config,
#ifdef CONFIG_MIPI_CSI_ENABLE
            .csi      = s_csi_config,
#endif
#ifdef CONFIG_DVP_ENABLE
            .dvp_num  = sizeof(s_dvp_config) / sizeof(s_dvp_config[0]),
            .dvp      = s_dvp_config,
#endif
#ifdef CONFIG_SIMULATED_INTF
            .sim_num  = VIDEO_COUNT,
            .sim = s_sim_config,
#endif
        };

        TEST_ESP_OK(esp_camera_init(&s_cam_config));
        s_inited = true;
    }
}

TEST_CASE("Video Pipeline Load MIPI-CSI Sensor Config", "[video]")
{
    esp_err_t ret;

#ifdef CONFIG_HEAP_TRACING
    heap_trace_record_t recs[HEAP_RES_NUM];

    heap_trace_init_standalone(recs, HEAP_RES_NUM);
    heap_trace_start(HEAP_TRACE_LEAKS);
#endif

    setUp();

    ret = esp_media_config_loader(mipi_csi_sensor_test_json);
    TEST_ASSERT_EQUAL_INT(ESP_OK, ret);

    struct esp_video *video = esp_video_device_get_object("MIPI-CSI");
    esp_pad_t *pad = esp_entity_get_pad_by_index(video->entity, ESP_PAD_TYPE_SINK, 0);
    esp_pipeline_t *pipeline = esp_pad_get_pipeline(pad);
    esp_media_t *media = esp_pipeline_get_media(pipeline);
    ret = esp_media_cleanup(media);

    TEST_ASSERT_EQUAL_INT(ESP_OK, ret);
    if (ret == ESP_OK) {
        printf("Config mipi_csi_sensor_test.json tests successfully\r\n");
    }

#ifdef CONFIG_HEAP_TRACING
    heap_trace_dump();
    heap_trace_stop();
#endif
}

TEST_CASE("Video Pipeline Load DVP Sensor Config", "[video]")
{
    esp_err_t ret;

#ifdef CONFIG_HEAP_TRACING
    heap_trace_record_t recs[HEAP_RES_NUM];

    heap_trace_init_standalone(recs, HEAP_RES_NUM);
    heap_trace_start(HEAP_TRACE_LEAKS);
#endif

    setUp();

    ret = esp_media_config_loader(dvp_sensor_test_json);
    TEST_ASSERT_EQUAL_INT(ESP_OK, ret);

    struct esp_video *video = esp_video_device_get_object("DVP");
    esp_pad_t *pad = esp_entity_get_pad_by_index(video->entity, ESP_PAD_TYPE_SINK, 0);
    esp_pipeline_t *pipeline = esp_pad_get_pipeline(pad);
    esp_media_t *media = esp_pipeline_get_media(pipeline);
    ret = esp_media_cleanup(media);

    TEST_ASSERT_EQUAL_INT(ESP_OK, ret);
    if (ret == ESP_OK) {
        printf("Config dvp_sensor_test.json tests successfully\r\n");
    }

#ifdef CONFIG_HEAP_TRACING
    heap_trace_dump();
    heap_trace_stop();
#endif
}

TEST_CASE("Video Pipeline Load 2 Sim Sensors Config", "[video]")
{
    esp_err_t ret;

#ifdef CONFIG_HEAP_TRACING
    heap_trace_record_t recs[HEAP_RES_NUM];

    heap_trace_init_standalone(recs, HEAP_RES_NUM);
    heap_trace_start(HEAP_TRACE_LEAKS);
#endif

    setUp();

    ret = esp_media_config_loader(sim_2_devices_test_json);
    TEST_ASSERT_EQUAL_INT(ESP_OK, ret);

    struct esp_video *video = esp_video_device_get_object("SIM0");
    esp_pad_t *pad = esp_entity_get_pad_by_index(video->entity, ESP_PAD_TYPE_SINK, 0);
    esp_pipeline_t *pipeline = esp_pad_get_pipeline(pad);
    esp_media_t *media = esp_pipeline_get_media(pipeline);
    ret = esp_media_cleanup(media);

    TEST_ASSERT_EQUAL_INT(ESP_OK, ret);
    if (ret == ESP_OK) {
        printf("Config sim_2_devices_test.json tests successfully\r\n");
    }

#ifdef CONFIG_HEAP_TRACING
    heap_trace_dump();
    heap_trace_stop();
#endif
}

TEST_CASE("Video Pipeline Load 2 Sim Sensors and 2 User Nodes Config", "[video]")
{
    esp_err_t ret;

#ifdef CONFIG_HEAP_TRACING
    heap_trace_record_t recs[HEAP_RES_NUM];

    heap_trace_init_standalone(recs, HEAP_RES_NUM);
    heap_trace_start(HEAP_TRACE_LEAKS);
#endif

    setUp();

    ret = esp_media_config_loader(sim_2_devices_2_user_nodes_test_json);
    TEST_ASSERT_EQUAL_INT(ESP_OK, ret);

    struct esp_video *video = esp_video_device_get_object("SIM0");
    esp_pad_t *pad = esp_entity_get_pad_by_index(video->entity, ESP_PAD_TYPE_SINK, 0);
    esp_pipeline_t *pipeline = esp_pad_get_pipeline(pad);
    esp_media_t *media = esp_pipeline_get_media(pipeline);
    ret = esp_media_cleanup(media);

    TEST_ASSERT_EQUAL_INT(ESP_OK, ret);
    if (ret == ESP_OK) {
        printf("Config sim_2_devices_2_user_nodes_test.json tests successfully\r\n");
    }

#ifdef CONFIG_HEAP_TRACING
    heap_trace_dump();
    heap_trace_stop();
#endif
}

TEST_CASE("Video Pipeline Load 2 User Nodes Config and 2 Pipelines Config", "[video]")
{
    esp_err_t ret;

#ifdef CONFIG_HEAP_TRACING
    heap_trace_record_t recs[HEAP_RES_NUM];

    heap_trace_init_standalone(recs, HEAP_RES_NUM);
    heap_trace_start(HEAP_TRACE_LEAKS);
#endif

    setUp();

    ret = esp_media_config_loader(sim_2_user_nodes_2_pipelines_test_json);
    TEST_ASSERT_EQUAL_INT(ESP_OK, ret);

    struct esp_video *video = esp_video_device_get_object("SIM0");
    esp_pad_t *pad = esp_entity_get_pad_by_index(video->entity, ESP_PAD_TYPE_SINK, 0);
    esp_pipeline_t *pipeline = esp_pad_get_pipeline(pad);
    esp_media_t *media = esp_pipeline_get_media(pipeline);
    ret = esp_media_cleanup(media);

    TEST_ASSERT_EQUAL_INT(ESP_OK, ret);
    if (ret == ESP_OK) {
        printf("Config sim_2_user_nodes_2_pipelines_test.json tests successfully\r\n");
    }

#ifdef CONFIG_HEAP_TRACING
    heap_trace_dump();
    heap_trace_stop();
#endif
}

static size_t before_free_8bit;
static size_t before_free_32bit;

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, before_free, after_free, delta);
    TEST_ASSERT_MESSAGE(delta >= TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
}

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    /* some FreeRTOS stuff is cleaned up by idle task */
    vTaskDelay(5);

    /* clean up some of the newlib's lazy allocations */
    esp_reent_cleanup();

    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

void app_main(void)
{
    /**
     * \ \     /_ _| __ \  ____|  _ \
     *  \ \   /   |  |   | __|   |   |
     *   \ \ /    |  |   | |     |   |
     *    \_/   ___|____/ _____|\___/
    */

    printf("\r\n");
    printf("\\ \\     /_ _| __ \\  ____|  _ \\  \r\n");
    printf(" \\ \\   /   |  |   | __|   |   |\r\n");
    printf("  \\ \\ /    |  |   | |     |   | \r\n");
    printf("   \\_/   ___|____/ _____|\\___/  \r\n");

    init();
    unity_run_menu();
}
