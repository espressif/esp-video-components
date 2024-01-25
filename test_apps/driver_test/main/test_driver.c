/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_camera.h"
#include "driver/i2c.h"
#include "sccb.h"
#include "mipi_csi.h"
#include "camera_isp.h"

#include "unity.h"
#include "unity_test_utils.h"
#include "unity_test_utils_memory.h"

/**************************************************************************************************
 *  ESP32-P4-Function-ev-board Pinout
 **************************************************************************************************/
/* I2C */
#if CONFIG_BSP_BOARD_TYPE_FLY_LINE
#define BSP_I2C_SCL             (GPIO_NUM_22)
#define BSP_I2C_SDA             (GPIO_NUM_23)
#elif CONFIG_BSP_BOARD_TYPE_FIB
#define BSP_I2C_SCL             (GPIO_NUM_8)
#define BSP_I2C_SDA             (GPIO_NUM_7)
#elif CONFIG_BSP_BOARD_TYPE_SAMPLE
#define BSP_I2C_SCL             (GPIO_NUM_34)
#define BSP_I2C_SDA             (GPIO_NUM_31)
#endif

#define SCCB_PORT_NUM I2C_NUM_0
#define FREQ_HZ          100000   /*!< clock frequency */

#define TEST_MEMORY_LEAK_THRESHOLD (-100)

static size_t before_free_8bit;
static size_t before_free_32bit;
static int IRAM_ATTR s_buffer_count;
static const char *TAG = "video_drv";

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

static void disp_buf(uint8_t *buf, uint16_t len)
{
    int i;
    for (i = 0; i < len; i++) {
        esp_rom_printf("%02x ", buf[i]);
        if ((i + 1) % 16 == 0) {
            esp_rom_printf("\n");
        }
    }
    esp_rom_printf("\n");
}

/*Note, used in ISR, unsafe functions cannot be used.*/
static esp_err_t IRAM_ATTR camera_recvdone_vb(uint8_t *buffer, uint32_t offset, uint32_t len, void *priv)
{
    esp_rom_printf("buf=%p, offset=%u, l=%d\n", buffer, offset, len);
    disp_buf(buffer, 32);
    free(buffer);
    return ESP_OK;
}

/*Note, used in ISR or Task context, unsafe functions cannot be used.*/
static uint8_t *IRAM_ATTR camera_get_new_vb(uint32_t len, void *priv)
{
    uint32_t _caps = MALLOC_CAP_8BIT;
    _caps |= MALLOC_CAP_SPIRAM;
    if (s_buffer_count > 3) {
        return NULL;
    }
    s_buffer_count++;
    return heap_caps_malloc(len + 16, _caps); // 16bytes for DMA align
}

TEST_CASE("csi driver dump received image data", "[video]")
{
    int enable_flag = 1;
    esp_err_t ret = ESP_OK;
    esp_mipi_csi_handle_t handle = NULL;
    sensor_format_array_info_t formats = {0};
    esp_mipi_csi_ops_t ops = {
        .alloc_buffer = camera_get_new_vb,
        .recved_data = camera_recvdone_vb,
    };
    esp_camera_driver_config_t drv_config = {
        .pwdn_pin = -1,
        .reset_pin = -1,
        .sccb_port = SCCB_PORT_NUM,
        .xclk_freq_hz = FREQ_HZ,
        .xclk_pin = -1,
    };

    ret = sccb_init(SCCB_PORT_NUM, BSP_I2C_SDA, BSP_I2C_SCL, FREQ_HZ);
    TEST_ESP_OK(ret);
    // This should auto detect, not call it in manual
    extern esp_camera_device_t *ov5647_csi_detect(const esp_camera_driver_config_t *config);
    esp_camera_device_t *device = ov5647_csi_detect(&drv_config);
    TEST_ASSERT(device);
    printf("sensor is %s\r\n", esp_camera_get_name(device));

    ret = esp_camera_query_format(device, &formats);
    TEST_ESP_OK(ret);
    printf("format count = %"PRIu32"\r\n", formats.count);
    const sensor_format_t *parray = formats.format_array;
    // set sensor output format
    sensor_format_t *current_format = (sensor_format_t *) & (parray[0].index);

    // Init cam interface
    mipi_csi_port_config_t mipi_if_cfg = {
        .frame_height = current_format->height,
        .frame_width = current_format->width,
        .mipi_clk_freq_hz = current_format->mipi_info.mipi_clk,
    };

    if (current_format->format == CAM_SENSOR_PIXFORMAT_RAW10) {
        mipi_if_cfg.in_format = PIXFORMAT_RAW10;
    } else if (current_format->format == CAM_SENSOR_PIXFORMAT_RAW8) {
        mipi_if_cfg.in_format = PIXFORMAT_RAW8;
    } else if (current_format->format == CAM_SENSOR_PIXFORMAT_YUV422) {
        mipi_if_cfg.in_format = PIXFORMAT_YUV422;
    } else if (current_format->format == CAM_SENSOR_PIXFORMAT_RGB565) {
        mipi_if_cfg.in_format = PIXFORMAT_RGB565;
    }

    if (current_format->isp_info) {
        mipi_if_cfg.isp_enable = true;
        // Todo, copy ISP info to ISP processor
    } else {
        mipi_if_cfg.isp_enable = false;
    }

    if (current_format->port == MIPI_CSI_OUTPUT_LANE1) {
        mipi_if_cfg.lane_num = 1;
    } else if (current_format->port == MIPI_CSI_OUTPUT_LANE2) {
        mipi_if_cfg.lane_num = 2;
    }
    // output RGB565 default
    mipi_if_cfg.out_format = PIXFORMAT_RGB565;

    ret = esp_mipi_csi_driver_install(MIPI_CSI_PORT0, &mipi_if_cfg, 0, &handle);
    TEST_ESP_OK(ret);

    isp_init(mipi_if_cfg.frame_width, mipi_if_cfg.frame_height, mipi_if_cfg.in_format, mipi_if_cfg.out_format, mipi_if_cfg.isp_enable, NULL);

    ret = esp_camera_set_format(device, (const sensor_format_t *) & (parray[0].index));
    TEST_ESP_OK(ret);
    PRINT_CAM_SENSOR_FORMAT_INFO(&(parray[0].index));

    // Start sensor stream
    ret = esp_camera_ioctl(device, CAM_SENSOR_IOC_S_STREAM, &enable_flag);
    TEST_ESP_OK(ret);

    ret = esp_mipi_csi_ops_regist(handle, &ops);
    TEST_ESP_OK(ret);

    ret = esp_mipi_csi_start(handle);

    // Delay to wait s_buffer_count>3
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    printf("Notify new buf\r\n");

    s_buffer_count = 0;
    ret = esp_mipi_csi_new_buffer_available(handle);
    TEST_ESP_OK(ret);

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ret = esp_mipi_csi_stop(handle);
    TEST_ESP_OK(ret);
    printf("Test Done\r\n");
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
