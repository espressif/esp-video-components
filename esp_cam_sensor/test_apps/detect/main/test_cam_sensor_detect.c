/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <esp_log.h>
#include <esp_system.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_sccb_intf.h"
#include "esp_sccb_i2c.h"
#include "esp_cam_sensor.h"

#include "unity.h"
#include "unity_test_utils.h"
#include "unity_test_utils_memory.h"

#if CONFIG_CAMERA_BF20A6
#include "bf20a6.h"
#define SCCB0_CAM_DEVICE_ADDR BF20A6_SCCB_ADDR
#elif CONFIG_CAMERA_BF3901
#include "bf3901.h"
#define SCCB0_CAM_DEVICE_ADDR BF3901_SCCB_ADDR
#elif CONFIG_CAMERA_BF3925
#include "bf3925.h"
#define SCCB0_CAM_DEVICE_ADDR BF3925_SCCB_ADDR
#elif CONFIG_CAMERA_BF3A03
#include "bf3a03.h"
#define SCCB0_CAM_DEVICE_ADDR BF3A03_SCCB_ADDR
#elif CONFIG_CAMERA_GC0308
#include "gc0308.h"
#define SCCB0_CAM_DEVICE_ADDR GC0308_SCCB_ADDR
#elif CONFIG_CAMERA_GC2145
#include "gc2145.h"
#define SCCB0_CAM_DEVICE_ADDR GC2145_SCCB_ADDR
#elif CONFIG_CAMERA_MIRA220
#include "mira220.h"
#define SCCB0_CAM_DEVICE_ADDR MIRA220_SCCB_ADDR
#elif CONFIG_CAMERA_MT9D111
#include "mt9d111.h"
#define SCCB0_CAM_DEVICE_ADDR MT9D111_SCCB_ADDR
#elif CONFIG_CAMERA_OS02N10
#include "os02n10.h"
#define SCCB0_CAM_DEVICE_ADDR OS02N10_SCCB_ADDR
#elif CONFIG_CAMERA_OV2640
#include "ov2640.h"
#define SCCB0_CAM_DEVICE_ADDR OV2640_SCCB_ADDR
#elif CONFIG_CAMERA_OV2710
#include "ov2710.h"
#define SCCB0_CAM_DEVICE_ADDR OV2710_SCCB_ADDR
#elif CONFIG_CAMERA_OV3660
#include "ov3660.h"
#define SCCB0_CAM_DEVICE_ADDR OV3660_SCCB_ADDR
#elif CONFIG_CAMERA_OV5640
#include "ov5640.h"
#define SCCB0_CAM_DEVICE_ADDR OV5640_SCCB_ADDR
#elif CONFIG_CAMERA_OV5645
#include "ov5645.h"
#define SCCB0_CAM_DEVICE_ADDR OV5645_SCCB_ADDR
#elif CONFIG_CAMERA_OV5647
#include "ov5647.h"
#define SCCB0_CAM_DEVICE_ADDR OV5647_SCCB_ADDR
#elif CONFIG_CAMERA_OV9281
#include "ov9281.h"
#define SCCB0_CAM_DEVICE_ADDR OV9281_SCCB_ADDR
#elif CONFIG_CAMERA_SC030IOT
#include "sc030iot.h"
#define SCCB0_CAM_DEVICE_ADDR SC030IOT_SCCB_ADDR
#elif CONFIG_CAMERA_SC035HGS
#include "sc035hgs.h"
#define SCCB0_CAM_DEVICE_ADDR SC035HGS_SCCB_ADDR
#elif CONFIG_CAMERA_SC101IOT
#include "sc101iot.h"
#define SCCB0_CAM_DEVICE_ADDR SC101IOT_SCCB_ADDR
#elif CONFIG_CAMERA_SC202CS
#include "sc202cs.h"
#define SCCB0_CAM_DEVICE_ADDR SC202CS_SCCB_ADDR
#elif CONFIG_CAMERA_SC2336
#include "sc2336.h"
#define SCCB0_CAM_DEVICE_ADDR SC2336_SCCB_ADDR
#elif CONFIG_CAMERA_SP0A39
#include "sp0a39.h"
#define SCCB0_CAM_DEVICE_ADDR SP0A39_SCCB_ADDR
#else
#define SCCB0_CAM_DEVICE_ADDR 0x01
#endif

/* SCCB */
#define SCCB0_SCL             CONFIG_SCCB0_SCL
#define SCCB0_SDA             CONFIG_SCCB0_SDA
#define SCCB0_FREQ_HZ         CONFIG_SCCB0_FREQUENCY
#define SCCB0_PORT_NUM        I2C_NUM_0

#define TEST_MEMORY_LEAK_THRESHOLD (-100)

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
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

TEST_CASE("Camera sensor detect test", "[video]")
{
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = SCCB0_PORT_NUM,
        .scl_io_num = SCCB0_SCL,
        .sda_io_num = SCCB0_SDA,
        .glitch_ignore_cnt = 7,
    };
    i2c_master_bus_handle_t bus_handle;
    esp_sccb_io_handle_t sccb_io;

    TEST_ESP_OK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));

    sccb_i2c_config_t sccb_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SCCB0_CAM_DEVICE_ADDR,
        .scl_speed_hz = SCCB0_FREQ_HZ,
    };

    TEST_ESP_OK(sccb_new_i2c_io(bus_handle, &sccb_config, &sccb_io));

    esp_cam_sensor_config_t cam0_config = {
        .sccb_handle = sccb_io,
        .reset_pin = -1,
        .pwdn_pin = -1,
        .xclk_pin = -1,
    };

#if CONFIG_CAMERA_BF20A6
    esp_cam_sensor_device_t *cam0 = bf20a6_detect(&cam0_config);
    TEST_ASSERT_MESSAGE(cam0 != NULL, "detect fail");

    TEST_ESP_OK(esp_cam_sensor_del_dev(cam0));
#elif CONFIG_CAMERA_BF3901
    esp_cam_sensor_device_t *cam0 = bf3901_detect(&cam0_config);
    TEST_ASSERT_MESSAGE(cam0 != NULL, "detect fail");

    TEST_ESP_OK(esp_cam_sensor_del_dev(cam0));
#elif CONFIG_CAMERA_BF3925
    esp_cam_sensor_device_t *cam0 = bf3925_detect(&cam0_config);
    TEST_ASSERT_MESSAGE(cam0 != NULL, "detect fail");

    TEST_ESP_OK(esp_cam_sensor_del_dev(cam0));
#elif CONFIG_CAMERA_BF3A03
    esp_cam_sensor_device_t *cam0 = bf3a03_detect(&cam0_config);
    TEST_ASSERT_MESSAGE(cam0 != NULL, "detect fail");

    TEST_ESP_OK(esp_cam_sensor_del_dev(cam0));
#elif CONFIG_CAMERA_GC0308
    esp_cam_sensor_device_t *cam0 = gc0308_detect(&cam0_config);
    TEST_ASSERT_MESSAGE(cam0 != NULL, "detect fail");

    TEST_ESP_OK(esp_cam_sensor_del_dev(cam0));
#elif CONFIG_CAMERA_GC2145
    esp_cam_sensor_device_t *cam0 = gc2145_detect(&cam0_config);
    TEST_ASSERT_MESSAGE(cam0 != NULL, "detect fail");

    TEST_ESP_OK(esp_cam_sensor_del_dev(cam0));
#elif CONFIG_CAMERA_MIRA220
    esp_cam_sensor_device_t *cam0 = mira220_detect(&cam0_config);
    TEST_ASSERT_MESSAGE(cam0 != NULL, "detect fail");

    TEST_ESP_OK(esp_cam_sensor_del_dev(cam0));
#elif CONFIG_CAMERA_MT9D111
    esp_cam_sensor_device_t *cam0 = mt9d111_detect(&cam0_config);
    TEST_ASSERT_MESSAGE(cam0 != NULL, "detect fail");

    TEST_ESP_OK(esp_cam_sensor_del_dev(cam0));
#elif CONFIG_CAMERA_OS02N10
    esp_cam_sensor_device_t *cam0 = os02n10_detect(&cam0_config);
    TEST_ASSERT_MESSAGE(cam0 != NULL, "detect fail");

    TEST_ESP_OK(esp_cam_sensor_del_dev(cam0));
#elif CONFIG_CAMERA_OV2640
    esp_cam_sensor_device_t *cam0 = ov2640_detect(&cam0_config);
    TEST_ASSERT_MESSAGE(cam0 != NULL, "detect fail");

    TEST_ESP_OK(esp_cam_sensor_del_dev(cam0));
#elif CONFIG_CAMERA_OV2710
    esp_cam_sensor_device_t *cam0 = ov2710_detect(&cam0_config);
    TEST_ASSERT_MESSAGE(cam0 != NULL, "detect fail");

    TEST_ESP_OK(esp_cam_sensor_del_dev(cam0));
#elif CONFIG_CAMERA_OV3660
    esp_cam_sensor_device_t *cam0 = ov3660_detect(&cam0_config);
    TEST_ASSERT_MESSAGE(cam0 != NULL, "detect fail");

    TEST_ESP_OK(esp_cam_sensor_del_dev(cam0));
#elif CONFIG_CAMERA_OV5640
    esp_cam_sensor_device_t *cam0 = ov5640_detect(&cam0_config);
    TEST_ASSERT_MESSAGE(cam0 != NULL, "detect fail");

    TEST_ESP_OK(esp_cam_sensor_del_dev(cam0));
#elif CONFIG_CAMERA_OV5645
    esp_cam_sensor_device_t *cam0 = ov5645_detect(&cam0_config);
    TEST_ASSERT_MESSAGE(cam0 != NULL, "detect fail");

    TEST_ESP_OK(esp_cam_sensor_del_dev(cam0));
#elif CONFIG_CAMERA_OV5647
    esp_cam_sensor_device_t *cam0 = ov5647_detect(&cam0_config);
    TEST_ASSERT_MESSAGE(cam0 != NULL, "detect fail");

    TEST_ESP_OK(esp_cam_sensor_del_dev(cam0));
#elif CONFIG_CAMERA_OV9281
    esp_cam_sensor_device_t *cam0 = ov9281_detect(&cam0_config);
    TEST_ASSERT_MESSAGE(cam0 != NULL, "detect fail");

    TEST_ESP_OK(esp_cam_sensor_del_dev(cam0));
#elif CONFIG_CAMERA_SC030IOT
    esp_cam_sensor_device_t *cam0 = sc030iot_detect(&cam0_config);
    TEST_ASSERT_MESSAGE(cam0 != NULL, "detect fail");

    TEST_ESP_OK(esp_cam_sensor_del_dev(cam0));
#elif CONFIG_CAMERA_SC035HGS
    esp_cam_sensor_device_t *cam0 = sc035hgs_detect(&cam0_config);
    TEST_ASSERT_MESSAGE(cam0 != NULL, "detect fail");

    TEST_ESP_OK(esp_cam_sensor_del_dev(cam0));
#elif CONFIG_CAMERA_SC101IOT
    esp_cam_sensor_device_t *cam0 = sc101iot_detect(&cam0_config);
    TEST_ASSERT_MESSAGE(cam0 != NULL, "detect fail");

    TEST_ESP_OK(esp_cam_sensor_del_dev(cam0));
#elif CONFIG_CAMERA_SC202CS
    esp_cam_sensor_device_t *cam0 = sc202cs_detect(&cam0_config);
    TEST_ASSERT_MESSAGE(cam0 != NULL, "detect fail");

    TEST_ESP_OK(esp_cam_sensor_del_dev(cam0));
#elif CONFIG_CAMERA_SC2336
    esp_cam_sensor_device_t *cam0 = sc2336_detect(&cam0_config);
    TEST_ASSERT_MESSAGE(cam0 != NULL, "detect fail");

    TEST_ESP_OK(esp_cam_sensor_del_dev(cam0));
#elif CONFIG_CAMERA_SP0A39
    esp_cam_sensor_device_t *cam0 = sp0a39_detect(&cam0_config);
    TEST_ASSERT_MESSAGE(cam0 != NULL, "detect fail");

    TEST_ESP_OK(esp_cam_sensor_del_dev(cam0));
#endif
    TEST_ESP_OK(esp_sccb_del_i2c_io(sccb_io));

    TEST_ESP_OK(i2c_del_master_bus(bus_handle));
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

    unity_run_menu();
}
