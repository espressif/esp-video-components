/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <sys/param.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"
#include "os04c10_settings.h"
#include "os04c10.h"

typedef struct {
    uint8_t analog_gain_h; // OS04C10_REG_ANALOG_GAIN_H: bit[5:0]
    uint8_t analog_gain_l; // OS04C10_REG_ANALOG_GAIN_L: bit[7:0]
} os04c10_gain_t;

/*
 * OS04C10 camera sensor parameter control.
 */
typedef struct {
    uint32_t exposure_val;
    uint32_t exposure_max;
    uint32_t gain_index; // current gain index

    uint32_t vflip_en : 1;
    uint32_t hmirror_en : 1;
} os04c10_para_t;

struct os04c10_cam {
    os04c10_para_t os04c10_para;
};

#define OS04C10_IO_MUX_LOCK(mux)
#define OS04C10_IO_MUX_UNLOCK(mux)
#define OS04C10_ENABLE_OUT_XCLK(pin,clk)
#define OS04C10_DISABLE_OUT_XCLK(pin)

#define OS04C10_FETCH_EXP_H(val)     (((val) >> 8) & 0xFF)
#define OS04C10_FETCH_EXP_L(val)     ((val) & 0xFF)

#define EXPOSURE_V4L2_UNIT_US                   100
#define EXPOSURE_V4L2_TO_OS04C10(v, sf)          \
    ((uint32_t)(((double)v) * EXPOSURE_V4L2_UNIT_US * 1000 / (((sf)->isp_info->isp_v1_info.tline_ns)) + 0.5))
#define EXPOSURE_OS04C10_TO_V4L2(v, sf)          \
    ((int32_t)(((double)v) * (((sf)->isp_info->isp_v1_info.tline_ns)) / EXPOSURE_V4L2_UNIT_US / 1000 + 0.5))

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))

#define OS04C10_EXPOSURE_TEST_EN 0
#define OS04C10_EXPOSURE_TEST_EN_GAIN 0

static const uint8_t s_os04c10_exp_min = 0x02;
static const uint8_t s_os04c10_exp_max_offset = 8;
static size_t s_os04c10_limited_gain_index;
static const char *TAG = "os04c10";

/* 0x3503[2] = 0, gain format is gain[12:0], where low 7bits are fraction bits.
* gain val = gain[12:0]/128
* os04c10_total_gain_val_map stores actual gain * 1000
*/
static const uint32_t os04c10_total_gain_val_map[] = {
    // 1x to 2x (16 steps)
    1000,   // gain_13bit = 128 (0x0080)
    1062,   // gain_13bit = 136 (0x0088)
    1125,   // gain_13bit = 144 (0x0090)
    1188,   // gain_13bit = 152 (0x0098)
    1250,   // gain_13bit = 160 (0x00A0)
    1312,   // gain_13bit = 168 (0x00A8)
    1375,   // gain_13bit = 176 (0x00B0)
    1438,   // gain_13bit = 184 (0x00B8)
    1500,   // gain_13bit = 192 (0x00C0)
    1562,   // gain_13bit = 200 (0x00C8)
    1625,   // gain_13bit = 208 (0x00D0)
    1688,   // gain_13bit = 216 (0x00D8)
    1750,   // gain_13bit = 224 (0x00E0)
    1812,   // gain_13bit = 232 (0x00E8)
    1875,   // gain_13bit = 240 (0x00F0)
    1938,   // gain_13bit = 248 (0x00F8)
    // 2x
    2000,   // gain_13bit = 256 (0x0100)
    2152,   // gain_13bit = 275 (0x0113)
    2250,   // gain_13bit = 288 (0x0120)
    2375,   // gain_13bit = 304 (0x0130)
    2500,   // gain_13bit = 320 (0x0140)
    2625,   // gain_13bit = 336 (0x0150)
    2750,   // gain_13bit = 352 (0x0160)
    2875,   // gain_13bit = 368 (0x0170)
    // 3x
    3000,   // gain_13bit = 384 (0x0180)
    3125,   // gain_13bit = 400 (0x0190)
    3250,   // gain_13bit = 416 (0x01A0)
    3375,   // gain_13bit = 432 (0x01B0)
    3500,   // gain_13bit = 448 (0x01C0)
    3625,   // gain_13bit = 464 (0x01D0)
    3750,   // gain_13bit = 480 (0x01E0)
    3875,   // gain_13bit = 496 (0x01F0)
    // 4x to 5x (8 steps)
    4000,   // gain_13bit = 512 (0x0200)
    4125,   // gain_13bit = 528 (0x0210)
    4250,   // gain_13bit = 544 (0x0220)
    4375,   // gain_13bit = 560 (0x0230)
    4500,   // gain_13bit = 576 (0x0240)
    4625,   // gain_13bit = 592 (0x0250)
    4750,   // gain_13bit = 608 (0x0260)
    4875,   // gain_13bit = 624 (0x0270)
    // 5x to 6x (8 steps)
    5000,   // gain_13bit = 640 (0x0280)
    5125,   // gain_13bit = 656 (0x0290)
    5250,   // gain_13bit = 672 (0x02A0)
    5375,   // gain_13bit = 688 (0x02B0)
    5500,   // gain_13bit = 704 (0x02C0)
    5625,   // gain_13bit = 720 (0x02D0)
    5750,   // gain_13bit = 736 (0x02E0)
    5875,   // gain_13bit = 752 (0x02F0)
    // 6x
    6000,   // gain_13bit = 768 (0x0300)
    6250,   // gain_13bit = 800 (0x0320)
    6500,   // gain_13bit = 832 (0x0340)
    6750,   // gain_13bit = 864 (0x0360)
    // 7x
    7000,   // gain_13bit = 896 (0x0380)
    7250,   // gain_13bit = 928 (0x03A0)
    7500,   // gain_13bit = 960 (0x03C0)
    7750,   // gain_13bit = 992 (0x03E0)
    // 8x to 9x (4 steps)
    8000,   // gain_13bit = 1024 (0x0400)
    8250,   // gain_13bit = 1056 (0x0420)
    8500,   // gain_13bit = 1088 (0x0440)
    8750,   // gain_13bit = 1120 (0x0460)
    // 9x to 10x (4 steps)
    9000,   // gain_13bit = 1152 (0x0480)
    9250,   // gain_13bit = 1184 (0x04A0)
    9500,   // gain_13bit = 1216 (0x04C0)
    9750,   // gain_13bit = 1248 (0x04E0)
    // 10x to 11x (4 steps)
    10000,  // gain_13bit = 1280 (0x0500)
    10250,  // gain_13bit = 1312 (0x0520)
    10500,  // gain_13bit = 1344 (0x0540)
    10750,  // gain_13bit = 1376 (0x0560)
    // 11x to 12x (4 steps)
    11000,  // gain_13bit = 1408 (0x0580)
    11250,  // gain_13bit = 1440 (0x05A0)
    11500,  // gain_13bit = 1472 (0x05C0)
    11750,  // gain_13bit = 1504 (0x05E0)
    // 12x to 13x (4 steps)
    12000,  // gain_13bit = 1536 (0x0600)
    12250,  // gain_13bit = 1568 (0x0620)
    12500,  // gain_13bit = 1600 (0x0640)
    12750,  // gain_13bit = 1632 (0x0660)
    // 13x to 14x (4 steps)
    13000,  // gain_13bit = 1664 (0x0680)
    13250,  // gain_13bit = 1696 (0x06A0)
    13500,  // gain_13bit = 1728 (0x06C0)
    13750,  // gain_13bit = 1760 (0x06E0)
    // 14x to 15x (4 steps)
    14000,  // gain_13bit = 1792 (0x0700)
    14250,  // gain_13bit = 1824 (0x0720)
    14500,  // gain_13bit = 1856 (0x0740)
    14750,  // gain_13bit = 1888 (0x0760)
    // 15x
    15000,  // gain_13bit = 1920 (0x0780)
};

/* 0x3503[2] = 0, gain format is gain[12:0], where low 7bits are fraction bits.
* gain val = gain[12:0]/128
* 0x3508[5:0] = gain[12:8] (high 5 bits)
* 0x3509[7:0] = gain[7:0] (low 8 bits)
* gain_13bit = (0x3508[5:0] << 8) | 0x3509[7:0]
*/
static const os04c10_gain_t os04c10_gain_map[] = {
    // 1x to 2x (16 steps)
    {0x00, 0x80},   // gain_13bit = 0x0080, gain_val = 1.0000
    {0x00, 0x88},   // gain_13bit = 0x0088, gain_val = 1.0625
    {0x00, 0x90},   // gain_13bit = 0x0090, gain_val = 1.1250
    {0x00, 0x98},   // gain_13bit = 0x0098, gain_val = 1.1875
    {0x00, 0xA0},   // gain_13bit = 0x00A0, gain_val = 1.2500
    {0x00, 0xA8},   // gain_13bit = 0x00A8, gain_val = 1.3125
    {0x00, 0xB0},   // gain_13bit = 0x00B0, gain_val = 1.3750
    {0x00, 0xB8},   // gain_13bit = 0x00B8, gain_val = 1.4375
    {0x00, 0xC0},   // gain_13bit = 0x00C0, gain_val = 1.5000
    {0x00, 0xC8},   // gain_13bit = 0x00C8, gain_val = 1.5625
    {0x00, 0xD0},   // gain_13bit = 0x00D0, gain_val = 1.6250
    {0x00, 0xD8},   // gain_13bit = 0x00D8, gain_val = 1.6875
    {0x00, 0xE0},   // gain_13bit = 0x00E0, gain_val = 1.7500
    {0x00, 0xE8},   // gain_13bit = 0x00E8, gain_val = 1.8125
    {0x00, 0xF0},   // gain_13bit = 0x00F0, gain_val = 1.8750
    {0x00, 0xF8},   // gain_13bit = 0x00F8, gain_val = 1.9375
    // 2x
    {0x01, 0x00},   // gain_13bit = 0x0100, gain_val = 2.000
    {0x01, 0x13},   // gain_13bit = 0x0113, gain_val = 2.152
    {0x01, 0x20},   // gain_13bit = 0x0120, gain_val = 2.250
    {0x01, 0x30},   // gain_13bit = 0x0130, gain_val = 2.375
    {0x01, 0x40},   // gain_13bit = 0x0140, gain_val = 2.500
    {0x01, 0x50},   // gain_13bit = 0x0150, gain_val = 2.625
    {0x01, 0x60},   // gain_13bit = 0x0160, gain_val = 2.750
    {0x01, 0x70},   // gain_13bit = 0x0170, gain_val = 2.875
    // 3x
    {0x01, 0x80},   // gain_13bit = 0x0180, gain_val = 3.000
    {0x01, 0x90},   // gain_13bit = 0x0190, gain_val = 3.125
    {0x01, 0xA0},   // gain_13bit = 0x01A0, gain_val = 3.250
    {0x01, 0xB0},   // gain_13bit = 0x01B0, gain_val = 3.375
    {0x01, 0xC0},   // gain_13bit = 0x01C0, gain_val = 3.500
    {0x01, 0xD0},   // gain_13bit = 0x01D0, gain_val = 3.625
    {0x01, 0xE0},   // gain_13bit = 0x01E0, gain_val = 3.750
    {0x01, 0xF0},   // gain_13bit = 0x01F0, gain_val = 3.875
    // 4x to 5x (8 steps)
    {0x02, 0x00},   // gain_13bit = 0x0200, gain_val = 4.000
    {0x02, 0x10},   // gain_13bit = 0x0210, gain_val = 4.125
    {0x02, 0x20},   // gain_13bit = 0x0220, gain_val = 4.250
    {0x02, 0x30},   // gain_13bit = 0x0230, gain_val = 4.375
    {0x02, 0x40},   // gain_13bit = 0x0240, gain_val = 4.500
    {0x02, 0x50},   // gain_13bit = 0x0250, gain_val = 4.625
    {0x02, 0x60},   // gain_13bit = 0x0260, gain_val = 4.750
    {0x02, 0x70},   // gain_13bit = 0x0270, gain_val = 4.875
    // 5x to 6x (8 steps)
    {0x02, 0x80},   // gain_13bit = 0x0280, gain_val = 5.000
    {0x02, 0x90},   // gain_13bit = 0x0290, gain_val = 5.125
    {0x02, 0xA0},   // gain_13bit = 0x02A0, gain_val = 5.250
    {0x02, 0xB0},   // gain_13bit = 0x02B0, gain_val = 5.375
    {0x02, 0xC0},   // gain_13bit = 0x02C0, gain_val = 5.500
    {0x02, 0xD0},   // gain_13bit = 0x02D0, gain_val = 5.625
    {0x02, 0xE0},   // gain_13bit = 0x02E0, gain_val = 5.750
    {0x02, 0xF0},   // gain_13bit = 0x02F0, gain_val = 5.875
    // 6x
    {0x03, 0x00},   // gain_13bit = 0x0300, gain_val = 6.000
    {0x03, 0x20},   // gain_13bit = 0x0320, gain_val = 6.250
    {0x03, 0x40},   // gain_13bit = 0x0340, gain_val = 6.500
    {0x03, 0x60},   // gain_13bit = 0x0360, gain_val = 6.750
    // 7x
    {0x03, 0x80},   // gain_13bit = 0x0380, gain_val = 7.000
    {0x03, 0xA0},   // gain_13bit = 0x03A0, gain_val = 7.250
    {0x03, 0xC0},   // gain_13bit = 0x03C0, gain_val = 7.500
    {0x03, 0xE0},   // gain_13bit = 0x03E0, gain_val = 7.750
    // 8x
    {0x04, 0x00},   // gain_13bit = 0x0400, gain_val = 8.000
    {0x04, 0x20},   // gain_13bit = 0x0420, gain_val = 8.2500
    {0x04, 0x40},   // gain_13bit = 0x0440, gain_val = 8.5000
    {0x04, 0x60},   // gain_13bit = 0x0460, gain_val = 8.7500
    // 9x to 10x (4 steps)
    {0x04, 0x80},   // gain_13bit = 0x0480, gain_val = 9.0000
    {0x04, 0xA0},   // gain_13bit = 0x04A0, gain_val = 9.2500
    {0x04, 0xC0},   // gain_13bit = 0x04C0, gain_val = 9.5000
    {0x04, 0xE0},   // gain_13bit = 0x04E0, gain_val = 9.7500
    // 10x to 11x (4 steps)
    {0x05, 0x00},   // gain_13bit = 0x0500, gain_val = 10.0000
    {0x05, 0x20},   // gain_13bit = 0x0520, gain_val = 10.2500
    {0x05, 0x40},   // gain_13bit = 0x0540, gain_val = 10.5000
    {0x05, 0x60},   // gain_13bit = 0x0560, gain_val = 10.7500
    // 11x to 12x (4 steps)
    {0x05, 0x80},   // gain_13bit = 0x0580, gain_val = 11.0000
    {0x05, 0xA0},   // gain_13bit = 0x05A0, gain_val = 11.2500
    {0x05, 0xC0},   // gain_13bit = 0x05C0, gain_val = 11.5000
    {0x05, 0xE0},   // gain_13bit = 0x05E0, gain_val = 11.7500
    // 12x to 13x (4 steps)
    {0x06, 0x00},   // gain_13bit = 0x0600, gain_val = 12.0000
    {0x06, 0x20},   // gain_13bit = 0x0620, gain_val = 12.2500
    {0x06, 0x40},   // gain_13bit = 0x0640, gain_val = 12.5000
    {0x06, 0x60},   // gain_13bit = 0x0660, gain_val = 12.7500
    // 13x to 14x (4 steps)
    {0x06, 0x80},   // gain_13bit = 0x0680, gain_val = 13.0000
    {0x06, 0xA0},   // gain_13bit = 0x06A0, gain_val = 13.2500
    {0x06, 0xC0},   // gain_13bit = 0x06C0, gain_val = 13.5000
    {0x06, 0xE0},   // gain_13bit = 0x06E0, gain_val = 13.7500
    // 14x to 15x (4 steps)
    {0x07, 0x00},   // gain_13bit = 0x0700, gain_val = 14.0000
    {0x07, 0x20},   // gain_13bit = 0x0720, gain_val = 14.2500
    {0x07, 0x40},   // gain_13bit = 0x0740, gain_val = 14.5000
    {0x07, 0x60},   // gain_13bit = 0x0760, gain_val = 14.7500
    // 15x
    {0x07, 0x80},   // gain_13bit = 0x0780, gain_val = 15.0000
};

// ISP information for each format
static const esp_cam_sensor_isp_info_t os04c10_isp_info[] = {
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 101000000,
            .vts = 1574, // 0x0626
            .hts = 2140, // 0x085c
            .tline_ns = 21158, // 21.15819209 us
            .gain_def = 16, // default gain index
            .exp_def = 0x313, // default exposure value
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
};

static const esp_cam_sensor_format_t os04c10_format_info[] = {
#if CONFIG_CAMERA_OS04C10_MIPI_RAW10_810X1080_30FPS
    {
        .name = "MIPI_1lane_24Minput_RAW10_810x1080_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .width = 810,
        .height = 1080,
        .xclk = 24000000,
        .regs = os04c10_mipi_1lane_24Minput_810x1080_raw10_30fps,
        .regs_size = ARRAY_SIZE(os04c10_mipi_1lane_24Minput_810x1080_raw10_30fps),
        .fps = 30,
        .isp_info = &os04c10_isp_info[0],
        .mipi_info = {
            .mipi_clk = 992000000, // 992Mbps/lane
            .lane_num = 1,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
#endif
};

#ifndef CONFIG_CAMERA_OS04C10_MIPI_IF_FORMAT_INDEX_DEFAULT
#error "Please choose at least one format in menuconfig for OS04C10"
#endif

static const uint8_t os04c10_format_default_index = CONFIG_CAMERA_OS04C10_MIPI_IF_FORMAT_INDEX_DEFAULT;

static const uint8_t os04c10_format_index[] = {
#if CONFIG_CAMERA_OS04C10_MIPI_RAW10_810X1080_30FPS
    0,
#endif
};

static inline uint8_t get_os04c10_actual_format_index(void)
{
    return os04c10_format_index[os04c10_format_default_index];
}

static esp_err_t os04c10_read(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a16v8(sccb_handle, reg, read_buf);
}

static esp_err_t os04c10_write(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t data)
{
    return esp_sccb_transmit_reg_a16v8(sccb_handle, reg, data);
}

/* write a array of registers  */
static esp_err_t os04c10_write_array(esp_sccb_io_handle_t sccb_handle, os04c10_reginfo_t *regarray)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && regarray[i].reg != OS04C10_REG_END) {
        if (regarray[i].reg != OS04C10_REG_DELAY) {
            ret = os04c10_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    ESP_LOGD(TAG, "Set array done[i=%d]", i);
    return ret;
}

static esp_err_t os04c10_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_data = 0;

    ret = os04c10_read(sccb_handle, reg, &reg_data);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (reg_data & ~mask) | ((value << offset) & mask);
    ret = os04c10_write(sccb_handle, reg, value);
    return ret;
}

static esp_err_t os04c10_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    return os04c10_set_reg_bits(dev->sccb_handle, OS04C10_REG_TEST_PATTERN, 7, 1, enable ? 0x01 : 0x00);
}

static esp_err_t os04c10_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return ESP_OK;
}

static esp_err_t os04c10_soft_reset(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = os04c10_set_reg_bits(dev->sccb_handle, OS04C10_REG_SOFT_RESET, 0, 1, 0x01);
    delay_ms(5);
    return ret;
}

static esp_err_t os04c10_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t pid_h, pid_l;

    ret = os04c10_read(dev->sccb_handle, OS04C10_REG_CHIP_ID_H, &pid_h);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = os04c10_read(dev->sccb_handle, OS04C10_REG_CHIP_ID_L, &pid_l);
    if (ret != ESP_OK) {
        return ret;
    }
    id->pid = (pid_h << 8) | pid_l;

    return ret;
}

static esp_err_t os04c10_set_exp_val(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    esp_err_t ret;
    struct os04c10_cam *cam_os04c10 = (struct os04c10_cam *)dev->priv;
    uint32_t value_buf = MAX(u32_val, s_os04c10_exp_min);
    value_buf = MIN(value_buf, cam_os04c10->os04c10_para.exposure_max);

    ESP_LOGD(TAG, "set exposure 0x%" PRIx32, value_buf);
    // According to reference code: 0x3502 is low 8 bits, 0x3501 is high 8 bits
    ret = os04c10_write(dev->sccb_handle,
                        OS04C10_REG_EXP_L,
                        OS04C10_FETCH_EXP_L(value_buf));
    ret |= os04c10_write(dev->sccb_handle,
                         OS04C10_REG_EXP_H,
                         OS04C10_FETCH_EXP_H(value_buf));
    if (ret == ESP_OK) {
        cam_os04c10->os04c10_para.exposure_val = value_buf;
    }
    return ret;
}

static esp_err_t os04c10_set_total_gain_val(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    esp_err_t ret;
    struct os04c10_cam *cam_os04c10 = (struct os04c10_cam *)dev->priv;

    // Limit gain index to valid range
    if (u32_val >= s_os04c10_limited_gain_index) {
        if (s_os04c10_limited_gain_index > 0) {
            u32_val = s_os04c10_limited_gain_index - 1;
        } else {
            u32_val = 0;  // Fallback to minimum gain if limit is 0
        }
    }

    ESP_LOGD(TAG, "set gain index %" PRIu32 ", gain_h=0x%02X, gain_l=0x%02X",
             u32_val, os04c10_gain_map[u32_val].analog_gain_h, os04c10_gain_map[u32_val].analog_gain_l);

    // Write gain registers: 0x3508[5:0] = gain[12:8], 0x3509[7:0] = gain[7:0]
    // Note: 0x3508[5:0] means bits 5 to 0, which is the low 6 bits of the register
    ret = os04c10_set_reg_bits(dev->sccb_handle,
                               OS04C10_REG_ANALOG_GAIN_H,
                               0, 6,  // offset=0, length=6, modify bits [5:0]
                               os04c10_gain_map[u32_val].analog_gain_h & 0x3F);  // Mask to 6 bits
    ret |= os04c10_write(dev->sccb_handle,
                         OS04C10_REG_ANALOG_GAIN_L,
                         os04c10_gain_map[u32_val].analog_gain_l);

    if (ret == ESP_OK) {
        cam_os04c10->os04c10_para.gain_index = u32_val;
    }
    return ret;
}

#if OS04C10_EXPOSURE_TEST_EN
static volatile uint32_t s_exp_v = 0;
static bool s_exp_add = true;
static TimerHandle_t ae_timer_handle = NULL;

static void ae_timer_callback(TimerHandle_t timer)
{
    esp_cam_sensor_device_t *dev = (esp_cam_sensor_device_t *)pvTimerGetTimerID(timer);
    if (dev == NULL) {
        return;
    }
    struct os04c10_cam *cam_os04c10 = (struct os04c10_cam *)dev->priv;
#if OS04C10_EXPOSURE_TEST_EN_GAIN
    // Gain mode: s_exp_v is gain index
    uint32_t min_gain_index = 0;
    uint32_t max_gain_index = (s_os04c10_limited_gain_index > 0) ? (s_os04c10_limited_gain_index - 1) : 0;

    // Check boundaries first and reverse direction if needed
    if (s_exp_add) {
        // Incrementing: check if reached maximum
        if (s_exp_v >= max_gain_index) {
            // Reached maximum, reverse direction to decrement
            s_exp_add = false;
            if (s_exp_v > max_gain_index) {
                s_exp_v = max_gain_index;  // Clamp to maximum if overflowed
            }
        } else {
            // Not at maximum, continue incrementing
            s_exp_v += 1;
        }
    } else {
        // Decrementing: check if reached minimum
        if (s_exp_v <= min_gain_index) {
            // Reached minimum, reverse direction to increment
            s_exp_add = true;
            if (s_exp_v < min_gain_index) {
                s_exp_v = min_gain_index;  // Clamp to minimum if underflowed
            }
        } else {
            // Not at minimum, continue decrementing
            s_exp_v -= 1;
        }
    }

    // Apply the gain value
    os04c10_set_total_gain_val(dev, s_exp_v);
    ESP_LOGI(TAG, "Gain index=%" PRIu32 " (direction=%s)", s_exp_v, s_exp_add ? "UP" : "DOWN");
#else
    // Exposure mode: s_exp_v is exposure value
    uint32_t min_exp = s_os04c10_exp_min;
    uint32_t max_exp = cam_os04c10->os04c10_para.exposure_max;
    const uint32_t exp_step = 10;  // Step size for exposure adjustment

    // Check boundaries first and reverse direction if needed
    if (s_exp_add) {
        // Incrementing: check if reached maximum
        if (s_exp_v >= max_exp) {
            // Reached maximum, reverse direction to decrement
            s_exp_add = false;
            if (s_exp_v > max_exp) {
                s_exp_v = max_exp;  // Clamp to maximum if overflowed
            }
        } else {
            // Not at maximum, continue incrementing
            s_exp_v += exp_step;
            // Clamp to maximum if step would exceed it
            if (s_exp_v > max_exp) {
                s_exp_v = max_exp;
            }
        }
    } else {
        // Decrementing: check if reached minimum
        if (s_exp_v <= min_exp) {
            // Reached minimum, reverse direction to increment
            s_exp_add = true;
            if (s_exp_v < min_exp) {
                s_exp_v = min_exp;  // Clamp to minimum if underflowed
            }
        } else {
            // Not at minimum, continue decrementing
            if (s_exp_v > exp_step) {
                s_exp_v -= exp_step;
            } else {
                s_exp_v = min_exp;  // Clamp to minimum if step would go below it
            }
        }
    }

    // Apply the exposure value
    os04c10_set_exp_val(dev, s_exp_v);
    ESP_LOGI(TAG, "Exposure=0x%" PRIx32 " (direction=%s)", s_exp_v, s_exp_add ? "UP" : "DOWN");
#endif
}
#endif

static esp_err_t os04c10_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;
    ret = os04c10_write(dev->sccb_handle, OS04C10_REG_STREAM_ON, enable ? 0x01 : 0x00);
    if (ret == ESP_OK) {
        dev->stream_status = enable;
    }
    ESP_LOGD(TAG, "Stream=%d", enable);
#if OS04C10_EXPOSURE_TEST_EN
    if (enable) {
        // Start test timer when stream is enabled
        if (ae_timer_handle == NULL) {
            // Initialize test value to minimum
            struct os04c10_cam *cam_os04c10 = (struct os04c10_cam *)dev->priv;
#if OS04C10_EXPOSURE_TEST_EN_GAIN
            s_exp_v = 0;  // Start from minimum gain index
#else
            s_exp_v = s_os04c10_exp_min;  // Start from minimum exposure
#endif
            s_exp_add = true;  // Start with increment direction

            ae_timer_handle = xTimerCreate("AE_t", 100 / portTICK_PERIOD_MS, pdTRUE,
                                           (void *)dev, ae_timer_callback);
            if (ae_timer_handle != NULL) {
                xTimerStart(ae_timer_handle, portMAX_DELAY);
            }
        }
    } else {
        // Stop and delete test timer when stream is disabled
        if (ae_timer_handle != NULL) {
            xTimerStop(ae_timer_handle, portMAX_DELAY);
            xTimerDelete(ae_timer_handle, portMAX_DELAY);
            ae_timer_handle = NULL;
        }
    }
#endif
    return ret;
}

static esp_err_t os04c10_set_mirror(esp_cam_sensor_device_t *dev, int enable)
{
    return os04c10_set_reg_bits(dev->sccb_handle, OS04C10_REG_FORMAT1, 3, 1, enable ? 0x01 : 0x00);
}

static esp_err_t os04c10_set_vflip(esp_cam_sensor_device_t *dev, int enable)
{
    return os04c10_set_reg_bits(dev->sccb_handle, OS04C10_REG_FORMAT1, 4, 1, enable ? 0x01 : 0x00);
}

static esp_err_t os04c10_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    esp_err_t ret = ESP_OK;
    switch (qdesc->id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = s_os04c10_exp_min;
        qdesc->number.maximum = dev->cur_format->isp_info->isp_v1_info.vts - s_os04c10_exp_max_offset;
        qdesc->number.step = 1;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.exp_def;
        break;
    case ESP_CAM_SENSOR_EXPOSURE_US:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = EXPOSURE_OS04C10_TO_V4L2(s_os04c10_exp_min, dev->cur_format);
        qdesc->number.maximum = EXPOSURE_OS04C10_TO_V4L2((dev->cur_format->isp_info->isp_v1_info.vts - s_os04c10_exp_max_offset), dev->cur_format);
        qdesc->number.step = MAX(EXPOSURE_OS04C10_TO_V4L2(0x01, dev->cur_format), 1);
        qdesc->default_value = EXPOSURE_OS04C10_TO_V4L2((dev->cur_format->isp_info->isp_v1_info.exp_def), dev->cur_format);
        break;
    case ESP_CAM_SENSOR_GAIN:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION;
        qdesc->enumeration.count = s_os04c10_limited_gain_index;
        qdesc->enumeration.elements = os04c10_total_gain_val_map;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.gain_def;
        break;
    case ESP_CAM_SENSOR_GROUP_EXP_GAIN:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_U8;
        qdesc->u8.size = sizeof(esp_cam_sensor_gh_exp_gain_t);
        break;
    case ESP_CAM_SENSOR_VFLIP:
    case ESP_CAM_SENSOR_HMIRROR:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = 0;
        qdesc->number.maximum = 1;
        qdesc->number.step = 1;
        qdesc->default_value = 0;
        break;
    default: {
        ESP_LOGD(TAG, "id=%"PRIx32" is not supported", qdesc->id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }
    return ret;
}

static esp_err_t os04c10_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    struct os04c10_cam *cam_os04c10 = (struct os04c10_cam *)dev->priv;
    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        *(uint32_t *)arg = cam_os04c10->os04c10_para.exposure_val;
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        *(uint32_t *)arg = cam_os04c10->os04c10_para.gain_index;
        break;
    }
    default: {
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    }
    return ret;
}

static esp_err_t os04c10_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;

    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        uint32_t u32_val = *(uint32_t *)arg;
        ret = os04c10_set_exp_val(dev, u32_val);
        break;
    }
    case ESP_CAM_SENSOR_EXPOSURE_US: {
        uint32_t u32_val = *(uint32_t *)arg;
        uint32_t ori_exp = EXPOSURE_V4L2_TO_OS04C10(u32_val, dev->cur_format);
        ret = os04c10_set_exp_val(dev, ori_exp);
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        uint32_t u32_val = *(uint32_t *)arg;
        ret = os04c10_set_total_gain_val(dev, u32_val);
        break;
    }
    case ESP_CAM_SENSOR_GROUP_EXP_GAIN: {
        esp_cam_sensor_gh_exp_gain_t *value = (esp_cam_sensor_gh_exp_gain_t *)arg;
        uint32_t ori_exp = 0;
        if (value->exposure_us != 0) {
            ori_exp = EXPOSURE_V4L2_TO_OS04C10(value->exposure_us, dev->cur_format);
        } else if (value->exposure_val != 0) {
            ori_exp = value->exposure_val;
        } else {
            ret = ESP_ERR_INVALID_ARG;
            break;
        }
        ret = os04c10_set_exp_val(dev, ori_exp);
        ret |= os04c10_set_total_gain_val(dev, value->gain_index);
        break;
    }
    case ESP_CAM_SENSOR_VFLIP: {
        int *value = (int *)arg;
        ret = os04c10_set_vflip(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        int *value = (int *)arg;
        ret = os04c10_set_mirror(dev, *value);
        break;
    }
    default: {
        ESP_LOGE(TAG, "set id=%" PRIx32 " is not supported", id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }

    return ret;
}

static esp_err_t os04c10_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);

    formats->count = ARRAY_SIZE(os04c10_format_info);
    formats->format_array = &os04c10_format_info[0];

    return ESP_OK;
}

static esp_err_t os04c10_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_raw = 1;
    return ESP_OK;
}

static esp_err_t os04c10_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    struct os04c10_cam *cam_os04c10 = (struct os04c10_cam *)dev->priv;
    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        format = &os04c10_format_info[get_os04c10_actual_format_index()];
    }

    ret = os04c10_write_array(dev->sccb_handle, (os04c10_reginfo_t *)format->regs);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set format regs fail");
        return ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT;
    }

    dev->cur_format = format;
    // init para
    cam_os04c10->os04c10_para.exposure_val = dev->cur_format->isp_info->isp_v1_info.exp_def;
    cam_os04c10->os04c10_para.gain_index = dev->cur_format->isp_info->isp_v1_info.gain_def;
    cam_os04c10->os04c10_para.exposure_max = dev->cur_format->isp_info->isp_v1_info.vts - s_os04c10_exp_max_offset;

    return ret;
}

static esp_err_t os04c10_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, format);

    esp_err_t ret = ESP_FAIL;

    if (dev->cur_format != NULL) {
        memcpy(format, dev->cur_format, sizeof(esp_cam_sensor_format_t));
        ret = ESP_OK;
    }
    return ret;
}

static esp_err_t os04c10_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    OS04C10_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = os04c10_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = os04c10_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = os04c10_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = os04c10_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = os04c10_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = os04c10_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = os04c10_get_sensor_id(dev, arg);
        break;
    default:
        ESP_LOGE(TAG, "cmd=%" PRIx32 " is not supported", cmd);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }

    OS04C10_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t os04c10_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        OS04C10_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->pwdn_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        gpio_config(&conf);

        // carefully, logic is inverted compared to reset pin
        gpio_set_level(dev->pwdn_pin, 1);
        delay_ms(10);
        gpio_set_level(dev->pwdn_pin, 0);
        delay_ms(10);
    }

    if (dev->reset_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->reset_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        gpio_config(&conf);

        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }

    return ret;
}

static esp_err_t os04c10_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        OS04C10_DISABLE_OUT_XCLK(dev->xclk_pin);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_set_level(dev->pwdn_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->pwdn_pin, 1);
        delay_ms(10);
    }

    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
    }

    return ret;
}

static esp_err_t os04c10_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del os04c10 (%p)", dev);
    if (dev) {
        if (dev->priv) {
            free(dev->priv);
            dev->priv = NULL;
        }
        free(dev);
        dev = NULL;
    }

    return ESP_OK;
}

static const esp_cam_sensor_ops_t os04c10_ops = {
    .query_para_desc = os04c10_query_para_desc,
    .get_para_value = os04c10_get_para_value,
    .set_para_value = os04c10_set_para_value,
    .query_support_formats = os04c10_query_support_formats,
    .query_support_capability = os04c10_query_support_capability,
    .set_format = os04c10_set_format,
    .get_format = os04c10_get_format,
    .priv_ioctl = os04c10_priv_ioctl,
    .del = os04c10_delete
};

esp_cam_sensor_device_t *os04c10_detect(esp_cam_sensor_config_t *config)
{
    esp_cam_sensor_device_t *dev = NULL;
    struct os04c10_cam *cam_os04c10;
    // Initialize gain limit index
    s_os04c10_limited_gain_index = ARRAY_SIZE(os04c10_total_gain_val_map);
    if (config == NULL) {
        return NULL;
    }

    dev = calloc(1, sizeof(esp_cam_sensor_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    cam_os04c10 = heap_caps_calloc(1, sizeof(struct os04c10_cam), MALLOC_CAP_DEFAULT);
    if (!cam_os04c10) {
        ESP_LOGE(TAG, "failed to calloc cam");
        free(dev);
        return NULL;
    }

    dev->name = (char *)OS04C10_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &os04c10_ops;
    dev->priv = cam_os04c10;
    dev->cur_format = &os04c10_format_info[get_os04c10_actual_format_index()];
    for (size_t i = 0; i < ARRAY_SIZE(os04c10_total_gain_val_map); i++) {
        if (os04c10_total_gain_val_map[i] > CONFIG_CAMERA_OS04C10_ABSOLUTE_GAIN_LIMIT) {
            // Ensure limited_index is at least 0 (first valid gain index)
            s_os04c10_limited_gain_index = i - 1;
            ESP_LOGD(TAG, "Limit gain index is %d", s_os04c10_limited_gain_index);
            break;
        }
    }

    // Configure sensor power, clock, and SCCB port
    if (os04c10_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (os04c10_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != OS04C10_PID) {
        ESP_LOGE(TAG, "Camera sensor is not OS04C10, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    os04c10_power_off(dev);
    free(dev->priv);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_OS04C10_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(os04c10_detect, ESP_CAM_SENSOR_MIPI_CSI, OS04C10_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return os04c10_detect(config);
}
#endif
