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
#include "gc2053_settings.h"
#include "gc2053.h"

/*
 * GC2053 camera sensor gain control.
 */
typedef struct {
    uint8_t reg_b4;
    uint8_t reg_b3;
    uint8_t reg_b8;
    uint8_t reg_b9;
    uint8_t reg_b1;
    uint8_t reg_b2;
} gc2053_gain_t;

typedef struct {
    uint32_t exposure_val;
    uint32_t exposure_max;
    uint32_t gain_index; // current gain index
    size_t limited_abs_gain_index; // max valid gain index (inclusive)
    bool stream_en;

    uint32_t vflip_en : 1;
    uint32_t hmirror_en : 1;
} gc2053_para_t;

struct gc2053_cam {
    gc2053_para_t gc2053_para;
};

#define GC2053_IO_MUX_LOCK(mux)
#define GC2053_IO_MUX_UNLOCK(mux)
#define GC2053_ENABLE_OUT_XCLK(pin,clk)
#define GC2053_DISABLE_OUT_XCLK(pin)

#define EXPOSURE_V4L2_UNIT_US                   100
#define EXPOSURE_V4L2_TO_GC2053(v, sf)          \
    ((uint32_t)(((double)v) * EXPOSURE_V4L2_UNIT_US * 1000 / (((sf)->isp_info->isp_v1_info.tline_ns)) + 0.5))
#define EXPOSURE_GC2053_TO_V4L2(v, sf)          \
    ((int32_t)(((double)v) * (((sf)->isp_info->isp_v1_info.tline_ns)) / EXPOSURE_V4L2_UNIT_US / 1000 + 0.5))

#define GC2053_FETCH_EXP_H(val)     (((val) >> 8) & 0x3F)
#define GC2053_FETCH_EXP_L(val)     ((val) & 0xFF)

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))

static const uint32_t s_limited_gain = CONFIG_CAMERA_GC2053_ABSOLUTE_GAIN_LIMIT;
static const uint8_t s_gc2053_exp_max_offset = 0x20; // min:1, max:VTS-32
static const uint8_t s_gc2053_exp_min = 0x02;
static const char *TAG = "gc2053";
#define GC2053_MODE_SW_STANDBY_RAW10  0x11
#define GC2053_MODE_SW_STANDBY_RAW8   0x15
#define GC2053_MODE_STREAMING_RAW10   0x91
#define GC2053_MODE_STREAMING_RAW8    0x95

#define GC2053_EXPOSURE_TEST_EN 0
#define GC2053_EXPOSURE_TEST_EN_GAIN 0

static const uint32_t gc2053_total_gain_val_map[] = {
    1000, 1031, 1063, 1094, 1125, 1156, 1188, 1219,
    1250, 1281, 1313, 1344, 1375, 1406, 1438, 1469,
    1516, 1547, 1578, 1609, 1641, 1672, 1703, 1734,
    1766, 1797, 1828, 1859, 1891, 1922, 1953, 1984,

    2000, 2063, 2125, 2188, 2250, 2313, 2391, 2453,
    2516, 2578, 2641, 2703, 2766, 2828, 2891, 2953,
    3031, 3094, 3156, 3219, 3281, 3344, 3406, 3469,
    3531, 3594, 3672, 3734, 3797, 3859, 3922, 3984,

    4000, 4125, 4250, 4391, 4516, 4641, 4766, 4906,
    5031, 5156, 5281, 5406, 5547, 5672, 5797, 5922,
    6063, 6188, 6313, 6438, 6578, 6703, 6828, 6953,
    7078, 7219, 7344, 7469, 7594, 7734, 7859, 7984,

    8000, 8250, 8516, 8766, 9031, 9281, 9547, 9797,
    10063, 10313, 10578, 10828, 11094, 11344, 11609, 11859,
    12125, 12375, 12641, 12891, 13156, 13406, 13672, 13922,
    14188, 14438, 14703, 14953, 15219, 15469, 15734, 15984,

    16000, 16516, 17031, 17547, 18063, 18578, 19094, 19609,
    20125, 20641, 21156, 21672, 22188, 22703, 23219, 23734,
    24250, 24766, 25281, 25797, 26313, 26828, 27344, 27859,
    28375, 28891, 29406, 29922, 30438, 30953, 31469, 31984,

    32000, 34125, 36266, 38391, 40531, 42656, 44797, 46922,
    49063, 51188, 53328, 55453, 57594, 59719, 61859, 63984,
};

static const gc2053_gain_t gc2053_gain_map[] = {

    {0x00, 0x00, 0x01, 0x00, 0x01, 0x00}, {0x00, 0x00, 0x01, 0x00, 0x01, 0x08}, {0x00, 0x00, 0x01, 0x00, 0x01, 0x10}, {0x00, 0x00, 0x01, 0x00, 0x01, 0x18},
    {0x00, 0x00, 0x01, 0x00, 0x01, 0x20}, {0x00, 0x10, 0x01, 0x0c, 0x01, 0x00}, {0x00, 0x10, 0x01, 0x0c, 0x01, 0x04}, {0x00, 0x10, 0x01, 0x0c, 0x01, 0x0c},
    {0x00, 0x10, 0x01, 0x0c, 0x01, 0x14}, {0x00, 0x10, 0x01, 0x0c, 0x01, 0x18}, {0x00, 0x10, 0x01, 0x0c, 0x01, 0x20}, {0x00, 0x10, 0x01, 0x0c, 0x01, 0x28},
    {0x00, 0x10, 0x01, 0x0c, 0x01, 0x30}, {0x00, 0x20, 0x01, 0x1b, 0x01, 0x00}, {0x00, 0x20, 0x01, 0x1b, 0x01, 0x08}, {0x00, 0x20, 0x01, 0x1b, 0x01, 0x0c},
    {0x00, 0x20, 0x01, 0x1b, 0x01, 0x14}, {0x00, 0x20, 0x01, 0x1b, 0x01, 0x1c}, {0x00, 0x20, 0x01, 0x1b, 0x01, 0x20}, {0x00, 0x30, 0x01, 0x2c, 0x01, 0x00},
    {0x00, 0x30, 0x01, 0x2c, 0x01, 0x04}, {0x00, 0x30, 0x01, 0x2c, 0x01, 0x0c}, {0x00, 0x30, 0x01, 0x2c, 0x01, 0x10}, {0x00, 0x30, 0x01, 0x2c, 0x01, 0x14},
    {0x00, 0x30, 0x01, 0x2c, 0x01, 0x18}, {0x00, 0x30, 0x01, 0x2c, 0x01, 0x20}, {0x00, 0x30, 0x01, 0x2c, 0x01, 0x24}, {0x00, 0x30, 0x01, 0x2c, 0x01, 0x28},
    {0x00, 0x30, 0x01, 0x2c, 0x01, 0x2c}, {0x00, 0x30, 0x01, 0x2c, 0x01, 0x34}, {0x00, 0x30, 0x01, 0x2c, 0x01, 0x38}, {0x00, 0x40, 0x01, 0x3f, 0x01, 0x00},

    {0x00, 0x40, 0x01, 0x3f, 0x01, 0x00}, {0x00, 0x40, 0x01, 0x3f, 0x01, 0x08}, {0x00, 0x40, 0x01, 0x3f, 0x01, 0x10}, {0x00, 0x40, 0x01, 0x3f, 0x01, 0x18},
    {0x00, 0x40, 0x01, 0x3f, 0x01, 0x20}, {0x00, 0x50, 0x02, 0x16, 0x01, 0x00}, {0x00, 0x50, 0x02, 0x16, 0x01, 0x08}, {0x00, 0x50, 0x02, 0x16, 0x01, 0x10},
    {0x00, 0x50, 0x02, 0x16, 0x01, 0x18}, {0x00, 0x50, 0x02, 0x16, 0x01, 0x1c}, {0x00, 0x50, 0x02, 0x16, 0x01, 0x24}, {0x00, 0x50, 0x02, 0x16, 0x01, 0x2c},
    {0x00, 0x60, 0x02, 0x35, 0x01, 0x00}, {0x00, 0x60, 0x02, 0x35, 0x01, 0x04}, {0x00, 0x60, 0x02, 0x35, 0x01, 0x08}, {0x00, 0x60, 0x02, 0x35, 0x01, 0x10},
    {0x00, 0x60, 0x02, 0x35, 0x01, 0x18}, {0x00, 0x60, 0x02, 0x35, 0x01, 0x1c}, {0x00, 0x60, 0x02, 0x35, 0x01, 0x24}, {0x00, 0x70, 0x03, 0x16, 0x01, 0x00},
    {0x00, 0x70, 0x03, 0x16, 0x01, 0x08}, {0x00, 0x70, 0x03, 0x16, 0x01, 0x0c}, {0x00, 0x70, 0x03, 0x16, 0x01, 0x10}, {0x00, 0x70, 0x03, 0x16, 0x01, 0x14},
    {0x00, 0x70, 0x03, 0x16, 0x01, 0x1c}, {0x00, 0x70, 0x03, 0x16, 0x01, 0x20}, {0x00, 0x70, 0x03, 0x16, 0x01, 0x28}, {0x00, 0x70, 0x03, 0x16, 0x01, 0x2c},
    {0x00, 0x70, 0x03, 0x16, 0x01, 0x30}, {0x00, 0x70, 0x03, 0x16, 0x01, 0x34}, {0x00, 0x70, 0x03, 0x16, 0x01, 0x3c}, {0x00, 0x70, 0x03, 0x16, 0x01, 0x40},

    {0x00, 0x70, 0x03, 0x16, 0x01, 0x40}, {0x00, 0x80, 0x04, 0x02, 0x01, 0x00}, {0x00, 0x80, 0x04, 0x02, 0x01, 0x08}, {0x00, 0x80, 0x04, 0x02, 0x01, 0x14},
    {0x00, 0x80, 0x04, 0x02, 0x01, 0x1c}, {0x00, 0x80, 0x04, 0x02, 0x01, 0x24}, {0x00, 0x90, 0x04, 0x31, 0x01, 0x04}, {0x00, 0x90, 0x04, 0x31, 0x01, 0x08},
    {0x00, 0x90, 0x04, 0x31, 0x01, 0x10}, {0x00, 0x90, 0x04, 0x31, 0x01, 0x18}, {0x00, 0x90, 0x04, 0x31, 0x01, 0x20}, {0x00, 0x90, 0x04, 0x31, 0x01, 0x24},
    {0x00, 0x90, 0x04, 0x31, 0x01, 0x2c}, {0x00, 0xa0, 0x05, 0x32, 0x01, 0x00}, {0x00, 0xa0, 0x05, 0x32, 0x01, 0x04}, {0x00, 0xa0, 0x05, 0x32, 0x01, 0x0c},
    {0x00, 0xa0, 0x05, 0x32, 0x01, 0x10}, {0x00, 0xa0, 0x05, 0x32, 0x01, 0x18}, {0x00, 0xa0, 0x05, 0x32, 0x01, 0x1c}, {0x00, 0xa0, 0x05, 0x32, 0x01, 0x24},
    {0x00, 0xb0, 0x06, 0x35, 0x01, 0x00}, {0x00, 0xb0, 0x06, 0x35, 0x01, 0x08}, {0x00, 0xb0, 0x06, 0x35, 0x01, 0x0c}, {0x00, 0xb0, 0x06, 0x35, 0x01, 0x10},
    {0x00, 0xb0, 0x06, 0x35, 0x01, 0x14}, {0x00, 0xb0, 0x06, 0x35, 0x01, 0x1c}, {0x00, 0xb0, 0x06, 0x35, 0x01, 0x20}, {0x00, 0xb0, 0x06, 0x35, 0x01, 0x24},
    {0x00, 0xb0, 0x06, 0x35, 0x01, 0x28}, {0x00, 0xb0, 0x06, 0x35, 0x01, 0x30}, {0x00, 0xb0, 0x06, 0x35, 0x01, 0x34}, {0x00, 0xc0, 0x08, 0x04, 0x01, 0x00},

    {0x00, 0xc0, 0x08, 0x04, 0x01, 0x04}, {0x00, 0xc0, 0x08, 0x04, 0x01, 0x0c}, {0x00, 0xc0, 0x08, 0x04, 0x01, 0x14}, {0x00, 0xc0, 0x08, 0x04, 0x01, 0x1c},
    {0x00, 0xc0, 0x08, 0x04, 0x01, 0x24}, {0x00, 0x5a, 0x09, 0x19, 0x01, 0x04}, {0x00, 0x5a, 0x09, 0x19, 0x01, 0x0c}, {0x00, 0x5a, 0x09, 0x19, 0x01, 0x14},
    {0x00, 0x5a, 0x09, 0x19, 0x01, 0x18}, {0x00, 0x5a, 0x09, 0x19, 0x01, 0x20}, {0x00, 0x5a, 0x09, 0x19, 0x01, 0x28}, {0x00, 0x5a, 0x09, 0x19, 0x01, 0x30},
    {0x00, 0x5a, 0x09, 0x19, 0x01, 0x38}, {0x00, 0x83, 0x0b, 0x0f, 0x01, 0x00}, {0x00, 0x83, 0x0b, 0x0f, 0x01, 0x04}, {0x00, 0x83, 0x0b, 0x0f, 0x01, 0x0c},
    {0x00, 0x83, 0x0b, 0x0f, 0x01, 0x10}, {0x00, 0x83, 0x0b, 0x0f, 0x01, 0x18}, {0x00, 0x83, 0x0b, 0x0f, 0x01, 0x1c}, {0x00, 0x83, 0x0b, 0x0f, 0x01, 0x24},
    {0x00, 0x93, 0x0d, 0x12, 0x01, 0x00}, {0x00, 0x93, 0x0d, 0x12, 0x01, 0x08}, {0x00, 0x93, 0x0d, 0x12, 0x01, 0x0c}, {0x00, 0x93, 0x0d, 0x12, 0x01, 0x10},
    {0x00, 0x93, 0x0d, 0x12, 0x01, 0x14}, {0x00, 0x93, 0x0d, 0x12, 0x01, 0x1c}, {0x00, 0x93, 0x0d, 0x12, 0x01, 0x20}, {0x00, 0x93, 0x0d, 0x12, 0x01, 0x24},
    {0x00, 0x93, 0x0d, 0x12, 0x01, 0x28}, {0x00, 0x93, 0x0d, 0x12, 0x01, 0x30}, {0x00, 0x93, 0x0d, 0x12, 0x01, 0x34}, {0x00, 0x93, 0x0d, 0x12, 0x01, 0x38},

    {0x00, 0x93, 0x0d, 0x12, 0x01, 0x38}, {0x00, 0x84, 0x10, 0x00, 0x01, 0x04}, {0x00, 0x84, 0x10, 0x00, 0x01, 0x0c}, {0x00, 0x84, 0x10, 0x00, 0x01, 0x14},
    {0x00, 0x84, 0x10, 0x00, 0x01, 0x20}, {0x00, 0x94, 0x12, 0x3a, 0x01, 0x00}, {0x00, 0x94, 0x12, 0x3a, 0x01, 0x08}, {0x00, 0x94, 0x12, 0x3a, 0x01, 0x0c},
    {0x00, 0x94, 0x12, 0x3a, 0x01, 0x14}, {0x00, 0x94, 0x12, 0x3a, 0x01, 0x1c}, {0x00, 0x94, 0x12, 0x3a, 0x01, 0x24}, {0x00, 0x94, 0x12, 0x3a, 0x01, 0x2c},
    {0x01, 0x2c, 0x1a, 0x02, 0x01, 0x00}, {0x01, 0x2c, 0x1a, 0x02, 0x01, 0x08}, {0x01, 0x2c, 0x1a, 0x02, 0x01, 0x0c}, {0x01, 0x2c, 0x1a, 0x02, 0x01, 0x14},
    {0x01, 0x2c, 0x1a, 0x02, 0x01, 0x18}, {0x01, 0x2c, 0x1a, 0x02, 0x01, 0x20}, {0x01, 0x2c, 0x1a, 0x02, 0x01, 0x24}, {0x01, 0x3c, 0x1b, 0x20, 0x01, 0x04},
    {0x01, 0x3c, 0x1b, 0x20, 0x01, 0x08}, {0x01, 0x3c, 0x1b, 0x20, 0x01, 0x0c}, {0x01, 0x3c, 0x1b, 0x20, 0x01, 0x14}, {0x01, 0x3c, 0x1b, 0x20, 0x01, 0x18},
    {0x01, 0x3c, 0x1b, 0x20, 0x01, 0x1c}, {0x01, 0x3c, 0x1b, 0x20, 0x01, 0x24}, {0x01, 0x3c, 0x1b, 0x20, 0x01, 0x28}, {0x01, 0x3c, 0x1b, 0x20, 0x01, 0x2c},
    {0x01, 0x3c, 0x1b, 0x20, 0x01, 0x30}, {0x01, 0x3c, 0x1b, 0x20, 0x01, 0x38}, {0x00, 0x8c, 0x20, 0x0f, 0x01, 0x00}, {0x00, 0x8c, 0x20, 0x0f, 0x01, 0x04},

    {0x00, 0x8c, 0x20, 0x0f, 0x01, 0x04}, {0x00, 0x8c, 0x20, 0x0f, 0x01, 0x18}, {0x00, 0x9c, 0x26, 0x07, 0x01, 0x00}, {0x00, 0x9c, 0x26, 0x07, 0x01, 0x10},
    {0x00, 0x9c, 0x26, 0x07, 0x01, 0x20}, {0x00, 0x9c, 0x26, 0x07, 0x01, 0x30}, {0x02, 0x64, 0x36, 0x21, 0x01, 0x00}, {0x02, 0x64, 0x36, 0x21, 0x01, 0x0c},
    {0x02, 0x64, 0x36, 0x21, 0x01, 0x18}, {0x02, 0x64, 0x36, 0x21, 0x01, 0x24}, {0x02, 0x74, 0x37, 0x3a, 0x01, 0x08}, {0x02, 0x74, 0x37, 0x3a, 0x01, 0x14},
    {0x02, 0x74, 0x37, 0x3a, 0x01, 0x1c}, {0x02, 0x74, 0x37, 0x3a, 0x01, 0x28}, {0x02, 0x74, 0x37, 0x3a, 0x01, 0x34}, {0x00, 0xc6, 0x3d, 0x02, 0x01, 0x00}
};

#if CONFIG_SOC_MIPI_CSI_SUPPORTED
static const esp_cam_sensor_isp_info_t gc2053_isp_info_mipi[] = {
    {
        .isp_v1_info = {
            .version     = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk        = 74250000,
            .hts         = 2200,
            .vts         = 1125,    /* per Linux driver: total_height=1125 */
            .exp_def     = 0x02a3,
            .gain_def    = 0x0,
            .tline_ns    = 29629,
            /*
             * CRITICAL: GC2053 native MIPI bayer output is GRBG.
             *   The v2 Rockchip Linux reference driver
             *   (gc2053v2.c) declares the format as
             *   MEDIA_BUS_FMT_SGRBG10_1X10; the "S" prefix is a
             *   V4L2 internal marker and the actual physical
             *   pattern is GRBG, which maps to
             *   ESP_CAM_SENSOR_BAYER_GRBG = 1 in esp_cam_sensor_types.h.
             *
             *   The earlier value ESP_CAM_SENSOR_BAYER_BGGR (=3)
             *   that landed in f400224 was a transcription error:
             *   the comment said "RGGB" but the code wrote BGGR,
             *   and BGGR is 90 degrees rotated from GRBG. The ISP
             *   demosaic still produced an image, but the colour
             *   channels were swapped and the green channels were
             *   placed at the wrong pixels, so every high-contrast
             *   edge showed "banding / false colour" (the user
             *   reported "形状边缘怪状，颜色错乱").
             *
             *   This value must stay in sync with the MIPI-CSI
             *   bridge's expectation. If the colour ever looks
             *   wrong, the *only* field to change is this enum;
             *   the rest of the pipeline (gamma, WB, CCM) takes
             *   its bayer order from here.
             */
            .bayer_type  = ESP_CAM_SENSOR_BAYER_RGGB, // notes: flip or mirror will change the default bayer type
        },
    },
    {
        .isp_v1_info = {
            .version     = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk        = 74250000,
            .hts         = 2200,
            .vts         = 1338,
            .exp_def     = 0x02a3,
            .gain_def    = 0x0,
            .tline_ns    = 29629,
            .bayer_type  = ESP_CAM_SENSOR_BAYER_RGGB,
        },
    },
};

#ifndef CONFIG_CAMERA_GC2053_MIPI_IF_FORMAT_INDEX_DEFAULT
#error "Please choose at least one format in menuconfig for GC2053"
#endif

static const uint8_t gc2053_format_default_index = CONFIG_CAMERA_GC2053_MIPI_IF_FORMAT_INDEX_DEFAULT;

static const uint8_t gc2053_format_index[] = {
#if CONFIG_CAMERA_GC2053_MIPI_RAW10_1920X1080_30FPS
    0,
#endif
#if CONFIG_CAMERA_GC2053_MIPI_RAW8_1920X1080_25FPS
    1,
#endif
};

static const esp_cam_sensor_format_t gc2053_format_info_mipi[] = {
    /* For MIPI */
#if CONFIG_CAMERA_GC2053_MIPI_RAW10_1920X1080_30FPS
    {
        .name = "MIPI_2lane_24Minput_RAW10_1920x1080_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1920,
        .height = 1080,
        .regs = init_reglist_MIPI_2lane_24Minput_RAW10_1920x1080_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_24Minput_RAW10_1920x1080_30fps),
        .fps = 30,
        .isp_info = &gc2053_isp_info_mipi[0],
        .mipi_info = {
            /* Linux driver uses 600 MHz mipi clock for MIPI 2-lane mode. */
            .mipi_clk = 600000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
#endif
#if CONFIG_CAMERA_GC2053_MIPI_RAW8_1920X1080_25FPS
    {
        .name = "MIPI_2lane_24Minput_RAW8_1920x1080_25fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1920,
        .height = 1080,
        .regs = init_reglist_MIPI_2lane_24Minput_RAW8_1920x1080_25fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_24Minput_RAW8_1920x1080_25fps),
        .fps = 25,
        .isp_info = &gc2053_isp_info_mipi[1],
        .mipi_info = {
            .mipi_clk = 594000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
#endif
};

static uint8_t get_gc2053_actual_format_index(void)
{
    for (int i = 0; i < ARRAY_SIZE(gc2053_format_index); i++) {
        if (gc2053_format_index[i] == gc2053_format_default_index) {
            return i;
        }
    }

    return 0;
}
#endif

static esp_err_t gc2053_read(esp_sccb_io_handle_t sccb_handle, uint8_t reg, uint8_t *value)
{
    return esp_sccb_transmit_receive_reg_a8v8(sccb_handle, reg, value);
}

static esp_err_t gc2053_write(esp_sccb_io_handle_t sccb_handle, uint8_t reg, uint8_t value)
{
    return esp_sccb_transmit_reg_a8v8(sccb_handle, reg, value);
}

static esp_err_t gc2053_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint8_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_data = 0;

    ret = gc2053_read(sccb_handle, reg, &reg_data);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (reg_data & ~mask) | ((value << offset) & mask);
    ret = gc2053_write(sccb_handle, reg, value);
    return ret;
}

static esp_err_t gc2053_write_array(esp_sccb_io_handle_t sccb_handle, const gc2053_reginfo_t *regarray)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && regarray[i].reg != GC2053_REG_END) {
        if (regarray[i].reg != GC2053_REG_DELAY) {
            ret = gc2053_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    ESP_LOGD(TAG, "write regs cnt=%d", i);
    return ret;
}

// Note that gc2053 xshutdown must be used to control reset(Especially in cases where only the SOC loses power.)
static esp_err_t gc2053_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(5);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(5);
    }
    return ESP_OK;
}

static esp_err_t gc2053_soft_reset(esp_cam_sensor_device_t *dev)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t gc2053_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_OK;
    ret = gc2053_write(dev->sccb_handle, GC2053_REG_PAGE_SELECT, 0x01);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = gc2053_set_reg_bits(dev->sccb_handle, 0x8c, 2, 1, enable ? 1 : 0);
    if (ret != ESP_OK) {
        return ret;
    }
    return ret;
}

static esp_err_t gc2053_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    esp_err_t ret;
    uint8_t high = 0, low = 0;

    ret = gc2053_write(dev->sccb_handle, GC2053_REG_PAGE_SELECT, 0x00);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = gc2053_read(dev->sccb_handle, GC2053_REG_CHIP_ID_HIGH, &high);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = gc2053_read(dev->sccb_handle, GC2053_REG_CHIP_ID_LOW, &low);
    if (ret != ESP_OK) {
        return ret;
    }

    id->midh = 0;
    id->midl = 0;
    id->pid = ((uint16_t)high << 8) | low;
    return ESP_OK;
}

static esp_err_t gc2053_set_exp_val(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    esp_err_t ret;
    struct gc2053_cam *cam_gc2053 = (struct gc2053_cam *)dev->priv;
    uint32_t value_buf = MAX(u32_val, s_gc2053_exp_min);
    value_buf = MIN(value_buf, cam_gc2053->gc2053_para.exposure_max);

    ESP_LOGD(TAG, "set exposure 0x%" PRIx32, value_buf);
    ret = gc2053_write(dev->sccb_handle, GC2053_REG_PAGE_SELECT, 0x00);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "page select failed");
    ret = gc2053_write(dev->sccb_handle, GC2053_REG_SHUTTER_TIME_H, GC2053_FETCH_EXP_H(value_buf));
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "shutter time high write failed");

    ret = gc2053_write(dev->sccb_handle, GC2053_REG_PAGE_SELECT, 0x00);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "page select failed");
    ret = gc2053_write(dev->sccb_handle, GC2053_REG_SHUTTER_TIME_L, GC2053_FETCH_EXP_L(value_buf));
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "shutter time low write failed");

    cam_gc2053->gc2053_para.exposure_val = value_buf;
    return ret;
}

static esp_err_t gc2053_set_total_gain_val(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    esp_err_t ret;
    struct gc2053_cam *cam_gc2053 = (struct gc2053_cam *)dev->priv;
    // limited_abs_gain_index is the max valid index (inclusive)
    if (u32_val > cam_gc2053->gc2053_para.limited_abs_gain_index) {
        u32_val = cam_gc2053->gc2053_para.limited_abs_gain_index;
    }

    ESP_LOGD(TAG, "gain index = 0x%" PRIx32, u32_val);

    ret = gc2053_write(dev->sccb_handle, GC2053_REG_PAGE_SELECT, 0x00);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "page select failed");

    ret = gc2053_write(dev->sccb_handle, 0xb4, gc2053_gain_map[u32_val].reg_b4);
    ret |= gc2053_write(dev->sccb_handle, 0xb3, gc2053_gain_map[u32_val].reg_b3);
    ret |= gc2053_write(dev->sccb_handle, 0xb8, gc2053_gain_map[u32_val].reg_b8);
    ret |= gc2053_write(dev->sccb_handle, 0xb9, gc2053_gain_map[u32_val].reg_b9);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "gain set failed");

    ret = gc2053_write(dev->sccb_handle, 0xb1, gc2053_gain_map[u32_val].reg_b1);
    ret |= gc2053_write(dev->sccb_handle, 0xb2, gc2053_gain_map[u32_val].reg_b2);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "pre-gain set failed");

    cam_gc2053->gc2053_para.gain_index = u32_val;
    ESP_LOGD(TAG, "Gain update done");
    return ret;
}

static esp_err_t gc2053_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t stream = (dev->cur_format->format == ESP_CAM_SENSOR_PIXFORMAT_RAW8) ? GC2053_MODE_SW_STANDBY_RAW8 : GC2053_MODE_SW_STANDBY_RAW10;
    /* Make sure the page select is on page 0, then drive the stream bit. */
    ret = gc2053_write(dev->sccb_handle, GC2053_REG_PAGE_SELECT, 0x00);
    if (ret != ESP_OK) {
        return ret;
    }
    /*
     * Per the user's complete init script the stream control register
     * is 0x3e, not 0x10:
     *   0x3e = 0x91  ->  bit7=lane_ena (1), bit4=MIPI_ena (1),
     *                    bit0=double_lane_en (1)  --> MIPI outputs data
     *   0x3e = 0x00  ->  all off                       --> no data
     *
     * 0x04 / 0x05 / 0x06 are window/region configuration in this init
     * (row start, col start, etc., with values 0x60 / 0x04 / 0x4c). They
     * are NOT power / clock / PLL enables. Writing 0x01 to them would
     * corrupt the active window and was the root cause of the previous
     * "stuck on red" symptom.
     */
    if (enable) {
        if (dev->cur_format->format == ESP_CAM_SENSOR_PIXFORMAT_RAW8) {
            stream = GC2053_MODE_STREAMING_RAW8; // raw8
        } else {
            stream = GC2053_MODE_STREAMING_RAW10; // raw10
        }
    }

    ret = gc2053_write(dev->sccb_handle, GC2053_REG_STREAM_EN, stream);

    if (ret == ESP_OK) {
        dev->stream_status = enable;
    }
    return ret;
}

static esp_err_t gc2053_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    esp_err_t ret = ESP_OK;
    struct gc2053_cam *cam_gc2053 = (struct gc2053_cam *)dev->priv;
    switch (qdesc->id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = s_gc2053_exp_min;
        qdesc->number.maximum = dev->cur_format->isp_info->isp_v1_info.vts - s_gc2053_exp_max_offset;
        qdesc->number.step = 1;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.exp_def;
        break;
    case ESP_CAM_SENSOR_EXPOSURE_US:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = MAX(0x01, EXPOSURE_GC2053_TO_V4L2(s_gc2053_exp_min, dev->cur_format)); // The minimum value must be greater than 1
        qdesc->number.maximum = EXPOSURE_GC2053_TO_V4L2((dev->cur_format->isp_info->isp_v1_info.vts - s_gc2053_exp_max_offset), dev->cur_format);
        qdesc->number.step = MAX(0x01, EXPOSURE_GC2053_TO_V4L2(0x01, dev->cur_format));
        qdesc->default_value = EXPOSURE_GC2053_TO_V4L2((dev->cur_format->isp_info->isp_v1_info.exp_def), dev->cur_format);
        break;
    case ESP_CAM_SENSOR_GAIN:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION;
        qdesc->enumeration.count = cam_gc2053->gc2053_para.limited_abs_gain_index + 1;
        qdesc->enumeration.elements = gc2053_total_gain_val_map;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.gain_def; // gain index
        break;
    case ESP_CAM_SENSOR_GROUP_EXP_GAIN:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_U8;
        qdesc->u8.size = sizeof(esp_cam_sensor_gh_exp_gain_t);
        break;
    default: {
        ESP_LOGD(TAG, "id=%"PRIx32" is not supported", qdesc->id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }
    return ret;
}

static esp_err_t gc2053_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    struct gc2053_cam *cam_gc2053 = (struct gc2053_cam *)dev->priv;
    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        *(uint32_t *)arg = cam_gc2053->gc2053_para.exposure_val;
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        *(uint32_t *)arg = cam_gc2053->gc2053_para.gain_index;
        break;
    }
    default: {
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    }
    return ret;
}

static esp_err_t gc2053_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;

    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        uint32_t u32_val = *(uint32_t *)arg;
        ret = gc2053_set_exp_val(dev, u32_val);
        break;
    }
    case ESP_CAM_SENSOR_EXPOSURE_US: {
        uint32_t u32_val = *(uint32_t *)arg;
        uint32_t ori_exp = EXPOSURE_V4L2_TO_GC2053(u32_val, dev->cur_format);
        ret = gc2053_set_exp_val(dev, ori_exp);
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        uint32_t u32_val = *(uint32_t *)arg;
        ret = gc2053_set_total_gain_val(dev, u32_val);
        break;
    }
    case ESP_CAM_SENSOR_GROUP_EXP_GAIN: {
        esp_cam_sensor_gh_exp_gain_t *value = (esp_cam_sensor_gh_exp_gain_t *)arg;
        uint32_t ori_exp = 0;
        if (value->exposure_val != 0) {
            ori_exp = value->exposure_val;
        } else if (value->exposure_us != 0) {
            ori_exp = EXPOSURE_V4L2_TO_GC2053(value->exposure_us, dev->cur_format);
        } else {
            ret = ESP_ERR_INVALID_ARG;
            break;
        }

        ret = gc2053_set_exp_val(dev, ori_exp);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "group exp/gain: exposure write failed");
        ret = gc2053_set_total_gain_val(dev, value->gain_index);
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

static esp_err_t gc2053_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    esp_err_t ret = ESP_FAIL;
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);
#if CONFIG_SOC_MIPI_CSI_SUPPORTED
    if (dev->sensor_port == ESP_CAM_SENSOR_MIPI_CSI) {
        formats->count = ARRAY_SIZE(gc2053_format_info_mipi);
        formats->format_array = &gc2053_format_info_mipi[0];
        ret = ESP_OK;
    }
#endif

    return ret;
}

static esp_err_t gc2053_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_raw = 1;
    return 0;
}

#if GC2053_EXPOSURE_TEST_EN
static volatile uint32_t s_exp_v = 0x05;
static bool s_exp_add = true;
TimerHandle_t ae_timer_handle;
static void ae_timer_callback(TimerHandle_t timer)
{
    esp_cam_sensor_device_t *dev = (esp_cam_sensor_device_t *)pvTimerGetTimerID(timer);
    struct gc2053_cam *cam_gc2053 = (struct gc2053_cam *)dev->priv;
#if GC2053_EXPOSURE_TEST_EN_GAIN
    if (s_exp_v >= cam_gc2053->gc2053_para.limited_abs_gain_index) {
        s_exp_add = false;
    } else if (s_exp_v < 1) {
        s_exp_add = true;
    }
    gc2053_set_total_gain_val(dev, s_exp_v);
    if (s_exp_add == true) {
        s_exp_v += 1;
    } else {
        s_exp_v -= 1;
    }
#else
    if (s_exp_v >= cam_gc2053->gc2053_para.exposure_max) {
        s_exp_add = false;
    } else if (s_exp_v < s_gc2053_exp_min) {
        s_exp_add = true;
    }
    gc2053_set_exp_val(dev, s_exp_v);
    if (s_exp_add == true) {
        s_exp_v += 2;
    } else {
        s_exp_v -= 2;
    }
#endif
    ESP_LOGI(TAG, "E=%" PRIu32, s_exp_v);
}
#endif

static esp_err_t gc2053_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    struct gc2053_cam *cam_gc2053 = (struct gc2053_cam *)dev->priv;
    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
#if CONFIG_SOC_MIPI_CSI_SUPPORTED
        if (dev->sensor_port == ESP_CAM_SENSOR_MIPI_CSI) {
            format = &gc2053_format_info_mipi[get_gc2053_actual_format_index()];
        }
#endif
    }
    ESP_RETURN_ON_FALSE(format != NULL, ESP_ERR_NOT_SUPPORTED, TAG, "format is NULL");

    ret = gc2053_write_array(dev->sccb_handle, (gc2053_reginfo_t *)format->regs);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT, TAG, "set format regs failed");
    ESP_LOGD(TAG, "Set format %s", format->name);
    dev->cur_format = format;

    // init para
    cam_gc2053->gc2053_para.exposure_val = dev->cur_format->isp_info->isp_v1_info.exp_def;
    cam_gc2053->gc2053_para.gain_index = dev->cur_format->isp_info->isp_v1_info.gain_def;
    cam_gc2053->gc2053_para.exposure_max = dev->cur_format->isp_info->isp_v1_info.vts - s_gc2053_exp_max_offset;
#if GC2053_EXPOSURE_TEST_EN
    ae_timer_handle = xTimerCreate("AE_t", 200 / portTICK_PERIOD_MS, pdTRUE,
                                   (void *)dev, ae_timer_callback);
    xTimerStart(ae_timer_handle, portMAX_DELAY);
#endif
    return ret;
}

static esp_err_t gc2053_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
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

static esp_err_t gc2053_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    GC2053_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = gc2053_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = gc2053_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = gc2053_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = gc2053_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = gc2053_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = gc2053_get_sensor_id(dev, arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = gc2053_set_test_pattern(dev, *(int *)arg);
        break;
    default:
        break;
    }

    GC2053_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t gc2053_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        GC2053_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->pwdn_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        ret = gpio_config(&conf);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "gpio config failed");

        gpio_set_level(dev->pwdn_pin, 1);
        delay_ms(5);
    }

    if (dev->reset_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->reset_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        ret = gpio_config(&conf);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "gpio config failed");

        gpio_set_level(dev->reset_pin, 1);
        delay_ms(5);
    }

    return ret;
}

static esp_err_t gc2053_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        GC2053_DISABLE_OUT_XCLK(dev->xclk_pin);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_set_level(dev->pwdn_pin, 0);
        delay_ms(10);
    }

    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
    }

    return ret;
}

static esp_err_t gc2053_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del gc2053 (%p)", dev);
    if (dev) {
        gc2053_power_off(dev);
        if (dev->priv) {
            free(dev->priv);
            dev->priv = NULL;
        }
        free(dev);
        dev = NULL;
    }

    return ESP_OK;
}

static const esp_cam_sensor_ops_t gc2053_ops = {
    .query_para_desc = gc2053_query_para_desc,
    .get_para_value = gc2053_get_para_value,
    .set_para_value = gc2053_set_para_value,
    .query_support_formats = gc2053_query_support_formats,
    .query_support_capability = gc2053_query_support_capability,
    .set_format = gc2053_set_format,
    .get_format = gc2053_get_format,
    .priv_ioctl = gc2053_priv_ioctl,
    .del = gc2053_delete
};

esp_cam_sensor_device_t *gc2053_detect(esp_cam_sensor_config_t *config)
{
    esp_cam_sensor_device_t *dev = NULL;
    struct gc2053_cam *cam_gc2053;

    if (config == NULL) {
        return NULL;
    }

    dev = calloc(1, sizeof(esp_cam_sensor_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    cam_gc2053 = heap_caps_calloc(1, sizeof(struct gc2053_cam), MALLOC_CAP_DEFAULT);
    if (!cam_gc2053) {
        ESP_LOGE(TAG, "failed to calloc cam");
        free(dev);
        return NULL;
    }

    cam_gc2053->gc2053_para.limited_abs_gain_index = ARRAY_SIZE(gc2053_total_gain_val_map) - 1;
    for (size_t i = 0; i < ARRAY_SIZE(gc2053_total_gain_val_map); i++) {
        if (gc2053_total_gain_val_map[i] > s_limited_gain) {
            cam_gc2053->gc2053_para.limited_abs_gain_index = (i > 0) ? (i - 1) : 0;
            break;
        }
    }

    dev->name = (char *)GC2053_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &gc2053_ops;
    dev->priv = cam_gc2053;
#if CONFIG_SOC_MIPI_CSI_SUPPORTED
    if (config->sensor_port == ESP_CAM_SENSOR_MIPI_CSI) {
        dev->cur_format = &gc2053_format_info_mipi[get_gc2053_actual_format_index()];
    }
#endif

    // Configure sensor power, clock, and SCCB port
    if (gc2053_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (gc2053_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != GC2053_PID) {
        ESP_LOGE(TAG, "Camera sensor is not GC2053, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    gc2053_power_off(dev);
    free(dev->priv);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_GC2053_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(gc2053_detect, ESP_CAM_SENSOR_MIPI_CSI, GC2053_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return gc2053_detect(config);
}
#endif
