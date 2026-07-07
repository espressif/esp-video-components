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
#include "gc4053_settings.h"
#include "gc4053.h"

/*
 * GC4053 camera sensor gain control.
 */
typedef struct {
    uint8_t total_gain_h;
    uint8_t total_gain_l;
} gc4053_gain_t;

typedef struct {
    uint32_t exposure_val;
    uint32_t exposure_max;
    uint32_t gain_index; // current gain index
    size_t limited_abs_gain_index; // max valid gain index (inclusive)
    bool stream_en;

    uint32_t vflip_en : 1;
    uint32_t hmirror_en : 1;
} gc4053_para_t;

struct gc4053_cam {
    gc4053_para_t gc4053_para;
};

#define GC4053_IO_MUX_LOCK(mux)
#define GC4053_IO_MUX_UNLOCK(mux)
#define GC4053_ENABLE_OUT_XCLK(pin,clk)
#define GC4053_DISABLE_OUT_XCLK(pin)

#define EXPOSURE_V4L2_UNIT_US                   100
#define EXPOSURE_V4L2_TO_GC4053(v, sf)          \
    ((uint32_t)(((double)v) * EXPOSURE_V4L2_UNIT_US * 1000 / (((sf)->isp_info->isp_v1_info.tline_ns)) + 0.5))
#define EXPOSURE_GC4053_TO_V4L2(v, sf)          \
    ((int32_t)(((double)v) * (((sf)->isp_info->isp_v1_info.tline_ns)) / EXPOSURE_V4L2_UNIT_US / 1000 + 0.5))

#define GC4053_FETCH_EXP_H(val)     (((val) >> 8) & 0x3F)
#define GC4053_FETCH_EXP_L(val)     ((val) & 0xFF)

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))

static const uint32_t s_limited_gain = CONFIG_CAMERA_GC4053_ABSOLUTE_GAIN_LIMIT;
static const uint8_t s_gc4053_exp_max_offset = 16; // min:4, max:VTS-16
static const uint8_t s_gc4053_exp_min = 0x04;
static const char *TAG = "gc4053";
#define GC4053_MODE_SW_STANDBY  0x11
#define GC4053_MODE_STREAMING   0x91
#define GC4053_EXPOSURE_TEST_EN 0
#define GC4053_EXPOSURE_TEST_EN_GAIN 0

/* total_gain = gain_val * 0x40 / 1000; 0x0807=bit[13:8], 0x0808=bit[7:0] */
static const uint32_t gc4053_total_gain_val_map[] = {
    // 1x
    1000,
    1031,
    1063,
    1094,
    1125,
    1156,
    1188,
    1219,
    1250,
    1281,
    1313,
    1344,
    1375,
    1406,
    1438,
    1469,
    1500,
    1531,
    1563,
    1594,
    1625,
    1656,
    1688,
    1719,
    1750,
    1781,
    1813,
    1844,
    1875,
    1906,
    1938,
    1969,
    // 2x
    2000,
    2031,
    2063,
    2094,
    2125,
    2156,
    2188,
    2219,
    2250,
    2281,
    2313,
    2344,
    2375,
    2406,
    2438,
    2469,
    2500,
    2531,
    2563,
    2594,
    2625,
    2656,
    2688,
    2719,
    2750,
    2781,
    2813,
    2844,
    2875,
    2906,
    2938,
    2969,
    // 3x
    3000,
    3063,
    3125,
    3188,
    3250,
    3313,
    3375,
    3438,
    3500,
    3563,
    3625,
    3688,
    3750,
    3813,
    3875,
    3938,
    // 4x
    4000,
    4063,
    4125,
    4188,
    4250,
    4313,
    4375,
    4438,
    4500,
    4563,
    4625,
    4688,
    4750,
    4813,
    4875,
    4938,
    // 5x
    5000,
    5063,
    5125,
    5188,
    5250,
    5313,
    5375,
    5438,
    5500,
    5563,
    5625,
    5688,
    5750,
    5813,
    5875,
    5938,
    // 6x
    6000,
    6063,
    6125,
    6188,
    6250,
    6313,
    6375,
    6438,
    6500,
    6563,
    6625,
    6688,
    6750,
    6813,
    6875,
    6938,
    // 7x
    7000,
    7063,
    7125,
    7188,
    7250,
    7313,
    7375,
    7438,
    7500,
    7563,
    7625,
    7688,
    7750,
    7813,
    7875,
    7938,
    // 8x
    8000,
    8125,
    8250,
    8375,
    8500,
    8625,
    8750,
    8875,
    // 9x
    9000,
    9125,
    9250,
    9375,
    9500,
    9625,
    9750,
    9875,
    // 10x
    10000,
    10125,
    10250,
    10375,
    10500,
    10625,
    10750,
    10875,
    // 11x
    11000,
    11125,
    11250,
    11375,
    11500,
    11625,
    11750,
    11875,
    // 12x
    12000,
    12250,
    12500,
    12750,
    // 13x
    13000,
    13250,
    13500,
    13750,
    // 14x
    14000,
    14250,
    14500,
    14750,
    // 15x
    15000,
    15250,
    15500,
    15750,
    // 16x
    16000,
    16250,
    16500,
    16750,
    // 17x
    17000,
    17250,
    17500,
    17750,
    // 18x
    18000,
    18250,
    18500,
    18750,
    // 19x
    19000,
    19250,
    19500,
    19750,
    // 20x
    20000,
    20250,
    20500,
    20750,
    // 21x
    21000,
    21250,
    21500,
    21750,
    // 22x
    22000,
    22250,
    22500,
    22750,
    // 23x
    23000,
    23250,
    23500,
    23750,
    // 24x
    24000,
    24250,
    24500,
    24750,
    // 25x
    25000,
    25250,
    25500,
    25750,
    // 26x
    26000,
    26250,
    26500,
    26750,
    // 27x
    27000,
    27250,
    27500,
    27750,
    // 28x
    28000,
    28250,
    28500,
    28750,
    // 29x
    29000,
    29250,
    29500,
    29750,
    // 30x
    30000,
    30250,
    30500,
    30750,
    // 31x
    31000,
    31250,
    31500,
    31750,
    // 32x ~ 33x
    32000,
    32250,
    32500,
    32750,
    // 33x ~ 34x
    33000,
    33250,
    33500,
    33750,
    // 34x ~ 35x
    34000,
    34250,
    34500,
    34750,
    // 35x ~ 36x
    35000,
    35250,
    35500,
    35750,
    // 36x ~ 37x
    36000,
    36250,
    36500,
    36750,
    // 37x ~ 38x
    37000,
    37250,
    37500,
    37750,
    // 38x ~ 39x
    38000,
    38250,
    38500,
    38750,
    // 39x ~ 40x
    39000,
    39250,
    39500,
    39750,
    // 40x ~ 41x
    40000,
    40250,
    40500,
    40750,
    // 41x ~ 42x
    41000,
    41250,
    41500,
    41750,
    // 42x ~ 43x
    42000,
    42250,
    42500,
    42750,
    // 43x ~ 44x
    43000,
    43250,
    43500,
    43750,
    // 44x ~ 45x
    44000,
    44250,
    44500,
    44750,
    // 45x ~ 46x
    45000,
    45250,
    45500,
    45750,
    // 46x ~ 47x
    46000,
    46250,
    46500,
    46750,
    // 47x ~ 48x
    47000,
    47250,
    47500,
    47750,
    // 48x ~ 49x
    48000,
    48250,
    48500,
    48750,
    // 49x ~ 50x
    49000,
    49250,
    49500,
    49750,
    // 50x ~ 51x
    50000,
    50250,
    50500,
    50750,
    // 51x ~ 52x
    51000,
    51250,
    51500,
    51750,
    // 52x ~ 53x
    52000,
    52250,
    52500,
    52750,
    // 53x ~ 54x
    53000,
    53250,
    53500,
    53750,
    // 54x ~ 55x
    54000,
    54250,
    54500,
    54750,
    // 55x ~ 56x
    55000,
    55250,
    55500,
    55750,
    // 56x ~ 57x
    56000,
    56250,
    56500,
    56750,
    // 57x ~ 58x
    57000,
    57250,
    57500,
    57750,
    // 58x ~ 59x
    58000,
    58250,
    58500,
    58750,
    // 59x ~ 60x
    59000,
    59250,
    59500,
    59750,
    // 60x ~ 61x
    60000,
    60250,
    60500,
    60750,
    // 61x ~ 62x
    61000,
    61250,
    61500,
    61750,
    // 62x ~ 63x
    62000,
    62250,
    62500,
    62750,
    // 63x ~ 64x
    63000,
    63250,
    63500,
    63750,
    // 64x
    64000,
};

static const gc4053_gain_t gc4053_gain_map[] = {
    // 1x
    {0x00, 0x40},
    {0x00, 0x42},
    {0x00, 0x44},
    {0x00, 0x46},
    {0x00, 0x48},
    {0x00, 0x4A},
    {0x00, 0x4C},
    {0x00, 0x4E},
    {0x00, 0x50},
    {0x00, 0x52},
    {0x00, 0x54},
    {0x00, 0x56},
    {0x00, 0x58},
    {0x00, 0x5A},
    {0x00, 0x5C},
    {0x00, 0x5E},
    {0x00, 0x60},
    {0x00, 0x62},
    {0x00, 0x64},
    {0x00, 0x66},
    {0x00, 0x68},
    {0x00, 0x6A},
    {0x00, 0x6C},
    {0x00, 0x6E},
    {0x00, 0x70},
    {0x00, 0x72},
    {0x00, 0x74},
    {0x00, 0x76},
    {0x00, 0x78},
    {0x00, 0x7A},
    {0x00, 0x7C},
    {0x00, 0x7E},
    // 2x
    {0x00, 0x80},
    {0x00, 0x82},
    {0x00, 0x84},
    {0x00, 0x86},
    {0x00, 0x88},
    {0x00, 0x8A},
    {0x00, 0x8C},
    {0x00, 0x8E},
    {0x00, 0x90},
    {0x00, 0x92},
    {0x00, 0x94},
    {0x00, 0x96},
    {0x00, 0x98},
    {0x00, 0x9A},
    {0x00, 0x9C},
    {0x00, 0x9E},
    {0x00, 0xA0},
    {0x00, 0xA2},
    {0x00, 0xA4},
    {0x00, 0xA6},
    {0x00, 0xA8},
    {0x00, 0xAA},
    {0x00, 0xAC},
    {0x00, 0xAE},
    {0x00, 0xB0},
    {0x00, 0xB2},
    {0x00, 0xB4},
    {0x00, 0xB6},
    {0x00, 0xB8},
    {0x00, 0xBA},
    {0x00, 0xBC},
    {0x00, 0xBE},
    // 3x
    {0x00, 0xC0},
    {0x00, 0xC4},
    {0x00, 0xC8},
    {0x00, 0xCC},
    {0x00, 0xD0},
    {0x00, 0xD4},
    {0x00, 0xD8},
    {0x00, 0xDC},
    {0x00, 0xE0},
    {0x00, 0xE4},
    {0x00, 0xE8},
    {0x00, 0xEC},
    {0x00, 0xF0},
    {0x00, 0xF4},
    {0x00, 0xF8},
    {0x00, 0xFC},
    // 4x
    {0x01, 0x00},
    {0x01, 0x04},
    {0x01, 0x08},
    {0x01, 0x0C},
    {0x01, 0x10},
    {0x01, 0x14},
    {0x01, 0x18},
    {0x01, 0x1C},
    {0x01, 0x20},
    {0x01, 0x24},
    {0x01, 0x28},
    {0x01, 0x2C},
    {0x01, 0x30},
    {0x01, 0x34},
    {0x01, 0x38},
    {0x01, 0x3C},
    // 5x
    {0x01, 0x40},
    {0x01, 0x44},
    {0x01, 0x48},
    {0x01, 0x4C},
    {0x01, 0x50},
    {0x01, 0x54},
    {0x01, 0x58},
    {0x01, 0x5C},
    {0x01, 0x60},
    {0x01, 0x64},
    {0x01, 0x68},
    {0x01, 0x6C},
    {0x01, 0x70},
    {0x01, 0x74},
    {0x01, 0x78},
    {0x01, 0x7C},
    // 6x
    {0x01, 0x80},
    {0x01, 0x84},
    {0x01, 0x88},
    {0x01, 0x8C},
    {0x01, 0x90},
    {0x01, 0x94},
    {0x01, 0x98},
    {0x01, 0x9C},
    {0x01, 0xA0},
    {0x01, 0xA4},
    {0x01, 0xA8},
    {0x01, 0xAC},
    {0x01, 0xB0},
    {0x01, 0xB4},
    {0x01, 0xB8},
    {0x01, 0xBC},
    // 7x
    {0x01, 0xC0},
    {0x01, 0xC4},
    {0x01, 0xC8},
    {0x01, 0xCC},
    {0x01, 0xD0},
    {0x01, 0xD4},
    {0x01, 0xD8},
    {0x01, 0xDC},
    {0x01, 0xE0},
    {0x01, 0xE4},
    {0x01, 0xE8},
    {0x01, 0xEC},
    {0x01, 0xF0},
    {0x01, 0xF4},
    {0x01, 0xF8},
    {0x01, 0xFC},
    // 8x
    {0x02, 0x00},
    {0x02, 0x08},
    {0x02, 0x10},
    {0x02, 0x18},
    {0x02, 0x20},
    {0x02, 0x28},
    {0x02, 0x30},
    {0x02, 0x38},
    // 9x
    {0x02, 0x40},
    {0x02, 0x48},
    {0x02, 0x50},
    {0x02, 0x58},
    {0x02, 0x60},
    {0x02, 0x68},
    {0x02, 0x70},
    {0x02, 0x78},
    // 10x
    {0x02, 0x80},
    {0x02, 0x88},
    {0x02, 0x90},
    {0x02, 0x98},
    {0x02, 0xA0},
    {0x02, 0xA8},
    {0x02, 0xB0},
    {0x02, 0xB8},
    // 11x
    {0x02, 0xC0},
    {0x02, 0xC8},
    {0x02, 0xD0},
    {0x02, 0xD8},
    {0x02, 0xE0},
    {0x02, 0xE8},
    {0x02, 0xF0},
    {0x02, 0xF8},
    // 12x
    {0x03, 0x00},
    {0x03, 0x10},
    {0x03, 0x20},
    {0x03, 0x30},
    // 13x
    {0x03, 0x40},
    {0x03, 0x50},
    {0x03, 0x60},
    {0x03, 0x70},
    // 14x
    {0x03, 0x80},
    {0x03, 0x90},
    {0x03, 0xA0},
    {0x03, 0xB0},
    // 15x
    {0x03, 0xC0},
    {0x03, 0xD0},
    {0x03, 0xE0},
    {0x03, 0xF0},
    // 16x
    {0x04, 0x00},
    {0x04, 0x10},
    {0x04, 0x20},
    {0x04, 0x30},
    // 17x
    {0x04, 0x40},
    {0x04, 0x50},
    {0x04, 0x60},
    {0x04, 0x70},
    // 18x
    {0x04, 0x80},
    {0x04, 0x90},
    {0x04, 0xA0},
    {0x04, 0xB0},
    // 19x
    {0x04, 0xC0},
    {0x04, 0xD0},
    {0x04, 0xE0},
    {0x04, 0xF0},
    // 20x
    {0x05, 0x00},
    {0x05, 0x10},
    {0x05, 0x20},
    {0x05, 0x30},
    // 21x
    {0x05, 0x40},
    {0x05, 0x50},
    {0x05, 0x60},
    {0x05, 0x70},
    // 22x
    {0x05, 0x80},
    {0x05, 0x90},
    {0x05, 0xA0},
    {0x05, 0xB0},
    // 23x
    {0x05, 0xC0},
    {0x05, 0xD0},
    {0x05, 0xE0},
    {0x05, 0xF0},
    // 24x
    {0x06, 0x00},
    {0x06, 0x10},
    {0x06, 0x20},
    {0x06, 0x30},
    // 25x
    {0x06, 0x40},
    {0x06, 0x50},
    {0x06, 0x60},
    {0x06, 0x70},
    // 26x
    {0x06, 0x80},
    {0x06, 0x90},
    {0x06, 0xA0},
    {0x06, 0xB0},
    // 27x
    {0x06, 0xC0},
    {0x06, 0xD0},
    {0x06, 0xE0},
    {0x06, 0xF0},
    // 28x
    {0x07, 0x00},
    {0x07, 0x10},
    {0x07, 0x20},
    {0x07, 0x30},
    // 29x
    {0x07, 0x40},
    {0x07, 0x50},
    {0x07, 0x60},
    {0x07, 0x70},
    // 30x
    {0x07, 0x80},
    {0x07, 0x90},
    {0x07, 0xA0},
    {0x07, 0xB0},
    // 31x
    {0x07, 0xC0},
    {0x07, 0xD0},
    {0x07, 0xE0},
    {0x07, 0xF0},
    // 32x ~ 33x
    {0x08, 0x00},
    {0x08, 0x10},
    {0x08, 0x20},
    {0x08, 0x30},
    // 33x ~ 34x
    {0x08, 0x40},
    {0x08, 0x50},
    {0x08, 0x60},
    {0x08, 0x70},
    // 34x ~ 35x
    {0x08, 0x80},
    {0x08, 0x90},
    {0x08, 0xA0},
    {0x08, 0xB0},
    // 35x ~ 36x
    {0x08, 0xC0},
    {0x08, 0xD0},
    {0x08, 0xE0},
    {0x08, 0xF0},
    // 36x ~ 37x
    {0x09, 0x00},
    {0x09, 0x10},
    {0x09, 0x20},
    {0x09, 0x30},
    // 37x ~ 38x
    {0x09, 0x40},
    {0x09, 0x50},
    {0x09, 0x60},
    {0x09, 0x70},
    // 38x ~ 39x
    {0x09, 0x80},
    {0x09, 0x90},
    {0x09, 0xA0},
    {0x09, 0xB0},
    // 39x ~ 40x
    {0x09, 0xC0},
    {0x09, 0xD0},
    {0x09, 0xE0},
    {0x09, 0xF0},
    // 40x ~ 41x
    {0x0A, 0x00},
    {0x0A, 0x10},
    {0x0A, 0x20},
    {0x0A, 0x30},
    // 41x ~ 42x
    {0x0A, 0x40},
    {0x0A, 0x50},
    {0x0A, 0x60},
    {0x0A, 0x70},
    // 42x ~ 43x
    {0x0A, 0x80},
    {0x0A, 0x90},
    {0x0A, 0xA0},
    {0x0A, 0xB0},
    // 43x ~ 44x
    {0x0A, 0xC0},
    {0x0A, 0xD0},
    {0x0A, 0xE0},
    {0x0A, 0xF0},
    // 44x ~ 45x
    {0x0B, 0x00},
    {0x0B, 0x10},
    {0x0B, 0x20},
    {0x0B, 0x30},
    // 45x ~ 46x
    {0x0B, 0x40},
    {0x0B, 0x50},
    {0x0B, 0x60},
    {0x0B, 0x70},
    // 46x ~ 47x
    {0x0B, 0x80},
    {0x0B, 0x90},
    {0x0B, 0xA0},
    {0x0B, 0xB0},
    // 47x ~ 48x
    {0x0B, 0xC0},
    {0x0B, 0xD0},
    {0x0B, 0xE0},
    {0x0B, 0xF0},
    // 48x ~ 49x
    {0x0C, 0x00},
    {0x0C, 0x10},
    {0x0C, 0x20},
    {0x0C, 0x30},
    // 49x ~ 50x
    {0x0C, 0x40},
    {0x0C, 0x50},
    {0x0C, 0x60},
    {0x0C, 0x70},
    // 50x ~ 51x
    {0x0C, 0x80},
    {0x0C, 0x90},
    {0x0C, 0xA0},
    {0x0C, 0xB0},
    // 51x ~ 52x
    {0x0C, 0xC0},
    {0x0C, 0xD0},
    {0x0C, 0xE0},
    {0x0C, 0xF0},
    // 52x ~ 53x
    {0x0D, 0x00},
    {0x0D, 0x10},
    {0x0D, 0x20},
    {0x0D, 0x30},
    // 53x ~ 54x
    {0x0D, 0x40},
    {0x0D, 0x50},
    {0x0D, 0x60},
    {0x0D, 0x70},
    // 54x ~ 55x
    {0x0D, 0x80},
    {0x0D, 0x90},
    {0x0D, 0xA0},
    {0x0D, 0xB0},
    // 55x ~ 56x
    {0x0D, 0xC0},
    {0x0D, 0xD0},
    {0x0D, 0xE0},
    {0x0D, 0xF0},
    // 56x ~ 57x
    {0x0E, 0x00},
    {0x0E, 0x10},
    {0x0E, 0x20},
    {0x0E, 0x30},
    // 57x ~ 58x
    {0x0E, 0x40},
    {0x0E, 0x50},
    {0x0E, 0x60},
    {0x0E, 0x70},
    // 58x ~ 59x
    {0x0E, 0x80},
    {0x0E, 0x90},
    {0x0E, 0xA0},
    {0x0E, 0xB0},
    // 59x ~ 60x
    {0x0E, 0xC0},
    {0x0E, 0xD0},
    {0x0E, 0xE0},
    {0x0E, 0xF0},
    // 60x ~ 61x
    {0x0F, 0x00},
    {0x0F, 0x10},
    {0x0F, 0x20},
    {0x0F, 0x30},
    // 61x ~ 62x
    {0x0F, 0x40},
    {0x0F, 0x50},
    {0x0F, 0x60},
    {0x0F, 0x70},
    // 62x ~ 63x
    {0x0F, 0x80},
    {0x0F, 0x90},
    {0x0F, 0xA0},
    {0x0F, 0xB0},
    // 63x ~ 64x
    {0x0F, 0xC0},
    {0x0F, 0xD0},
    {0x0F, 0xE0},
    {0x0F, 0xF0},
    // 64x
    {0x10, 0x00},
};

#if CONFIG_SOC_MIPI_CSI_SUPPORTED
static const esp_cam_sensor_isp_info_t gc4053_isp_info_mipi[] = {
    {
        .isp_v1_info = {
            .version     = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk        = 40500000,
            .hts         = 600,
            .vts         = 2700,
            .exp_def     = 0x02a3,
            .gain_def    = 0x0,
            .tline_ns    = 14810,
            .bayer_type  = ESP_CAM_SENSOR_BAYER_GRBG,
        },
    },
};

#ifndef CONFIG_CAMERA_GC4053_MIPI_IF_FORMAT_INDEX_DEFAULT
#error "Please choose at least one format in menuconfig for GC4053"
#endif

static const uint8_t gc4053_format_default_index = CONFIG_CAMERA_GC4053_MIPI_IF_FORMAT_INDEX_DEFAULT;

static const uint8_t gc4053_format_index[] = {
#if CONFIG_CAMERA_GC4053_MIPI_RAW10_1440X1440_25FPS
    0,
#endif
};

static const esp_cam_sensor_format_t gc4053_format_info_mipi[] = {
    /* For MIPI */
#if CONFIG_CAMERA_GC4053_MIPI_RAW10_1440X1440_25FPS
    {
        .name = "MIPI_2lane_24Minput_RAW10_1440x1440_25fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1440,
        .height = 1440,
        .regs = init_reglist_MIPI_2lane_24Minput_RAW10_1440x1440_25fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_24Minput_RAW10_1440x1440_25fps),
        .fps = 25,
        .isp_info = &gc4053_isp_info_mipi[0],
        .mipi_info = {
            .mipi_clk = 972000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
#endif
};

static uint8_t get_gc4053_actual_format_index(void)
{
    for (int i = 0; i < ARRAY_SIZE(gc4053_format_index); i++) {
        if (gc4053_format_index[i] == gc4053_format_default_index) {
            return i;
        }
    }

    return 0;
}
#endif

static esp_err_t gc4053_read(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t *value)
{
    return esp_sccb_transmit_receive_reg_a16v8(sccb_handle, reg, value);
}

static esp_err_t gc4053_write(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t value)
{
    return esp_sccb_transmit_reg_a16v8(sccb_handle, reg, value);
}

static esp_err_t gc4053_write_array(esp_sccb_io_handle_t sccb_handle, const gc4053_reginfo_t *regarray)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && regarray[i].reg != GC4053_REG_END) {
        if (regarray[i].reg != GC4053_REG_DELAY) {
            ret = gc4053_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    ESP_LOGW(TAG, "write regs cnt=%d", i);
    return ret;
}

static esp_err_t gc4053_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(6);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return ESP_OK;
}

static esp_err_t gc4053_soft_reset(esp_cam_sensor_device_t *dev)
{
    return ESP_OK;
}

static esp_err_t gc4053_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    return ESP_OK;
}

static esp_err_t gc4053_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    esp_err_t ret;
    uint8_t high = 0, low = 0;

    ret = gc4053_read(dev->sccb_handle, GC4053_REG_CHIP_ID_HIGH, &high);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = gc4053_read(dev->sccb_handle, GC4053_REG_CHIP_ID_LOW, &low);
    if (ret != ESP_OK) {
        return ret;
    }

    id->midh = 0;
    id->midl = 0;
    id->pid = ((uint16_t)high << 8) | low;
    return ESP_OK;
}

static esp_err_t gc4053_set_exp_val(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    esp_err_t ret;
    struct gc4053_cam *cam_gc4053 = (struct gc4053_cam *)dev->priv;
    uint32_t value_buf = MAX(u32_val, s_gc4053_exp_min);
    value_buf = MIN(value_buf, cam_gc4053->gc4053_para.exposure_max);

    ESP_LOGD(TAG, "set exposure 0x%" PRIx32, value_buf);

    ret = gc4053_write(dev->sccb_handle, GC4053_REG_SHUTTER_TIME_H, GC4053_FETCH_EXP_H(value_buf));
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "shutter time high write failed");
    ret = gc4053_write(dev->sccb_handle, GC4053_REG_SHUTTER_TIME_L, GC4053_FETCH_EXP_L(value_buf));
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "shutter time low write failed");

    cam_gc4053->gc4053_para.exposure_val = value_buf;
    return ret;
}

static esp_err_t gc4053_set_total_gain_val(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    esp_err_t ret;
    struct gc4053_cam *cam_gc4053 = (struct gc4053_cam *)dev->priv;
    // limited_abs_gain_index is the max valid index (inclusive)
    if (u32_val > cam_gc4053->gc4053_para.limited_abs_gain_index) {
        u32_val = cam_gc4053->gc4053_para.limited_abs_gain_index;
    }

    ESP_LOGD(TAG, "gain index = 0x%" PRIx32, u32_val);

    ret = gc4053_write(dev->sccb_handle, GC4053_REG_TOTAL_GAIN_H, gc4053_gain_map[u32_val].total_gain_h);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "page select failed");

    ret = gc4053_write(dev->sccb_handle, GC4053_REG_TOTAL_GAIN_L, gc4053_gain_map[u32_val].total_gain_l);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "gain set failed");

    cam_gc4053->gc4053_para.gain_index = u32_val;
    ESP_LOGD(TAG, "Gain update done");
    return ret;
}

static esp_err_t gc4053_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;

    ret = gc4053_write(dev->sccb_handle, GC4053_REG_LANE_EN, enable ? 0x03 : 0x00);
    if (ret == ESP_OK) {
        dev->stream_status = enable;
    }
    ESP_LOGW(TAG, "Stream=%d", enable);
    return ret;
}

static esp_err_t gc4053_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    esp_err_t ret = ESP_OK;
    struct gc4053_cam *cam_gc4053 = (struct gc4053_cam *)dev->priv;
    switch (qdesc->id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = s_gc4053_exp_min;
        qdesc->number.maximum = dev->cur_format->isp_info->isp_v1_info.vts - s_gc4053_exp_max_offset;
        qdesc->number.step = 1;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.exp_def;
        break;
    case ESP_CAM_SENSOR_EXPOSURE_US:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = MAX(0x01, EXPOSURE_GC4053_TO_V4L2(s_gc4053_exp_min, dev->cur_format)); // The minimum value must be greater than 1
        qdesc->number.maximum = EXPOSURE_GC4053_TO_V4L2((dev->cur_format->isp_info->isp_v1_info.vts - s_gc4053_exp_max_offset), dev->cur_format);
        qdesc->number.step = MAX(0x01, EXPOSURE_GC4053_TO_V4L2(0x01, dev->cur_format));
        qdesc->default_value = EXPOSURE_GC4053_TO_V4L2((dev->cur_format->isp_info->isp_v1_info.exp_def), dev->cur_format);
        break;
    case ESP_CAM_SENSOR_GAIN:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION;
        qdesc->enumeration.count = cam_gc4053->gc4053_para.limited_abs_gain_index + 1;
        qdesc->enumeration.elements = gc4053_total_gain_val_map;
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

static esp_err_t gc4053_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    struct gc4053_cam *cam_gc4053 = (struct gc4053_cam *)dev->priv;
    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        *(uint32_t *)arg = cam_gc4053->gc4053_para.exposure_val;
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        *(uint32_t *)arg = cam_gc4053->gc4053_para.gain_index;
        break;
    }
    default: {
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    }
    return ret;
}

static esp_err_t gc4053_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;

    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        uint32_t u32_val = *(uint32_t *)arg;
        ret = gc4053_set_exp_val(dev, u32_val);
        break;
    }
    case ESP_CAM_SENSOR_EXPOSURE_US: {
        uint32_t u32_val = *(uint32_t *)arg;
        uint32_t ori_exp = EXPOSURE_V4L2_TO_GC4053(u32_val, dev->cur_format);
        ret = gc4053_set_exp_val(dev, ori_exp);
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        uint32_t u32_val = *(uint32_t *)arg;
        ret = gc4053_set_total_gain_val(dev, u32_val);
        break;
    }
    case ESP_CAM_SENSOR_GROUP_EXP_GAIN: {
        esp_cam_sensor_gh_exp_gain_t *value = (esp_cam_sensor_gh_exp_gain_t *)arg;
        uint32_t ori_exp = 0;
        if (value->exposure_us != 0) {
            ori_exp = EXPOSURE_V4L2_TO_GC4053(value->exposure_us, dev->cur_format);
        } else if (value->exposure_val != 0) {
            ori_exp = value->exposure_val;
        } else {
            ret = ESP_ERR_INVALID_ARG;
            break;
        }
        ret = gc4053_set_exp_val(dev, ori_exp);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "group exp/gain: exposure write failed");
        ret = gc4053_set_total_gain_val(dev, value->gain_index);
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

static esp_err_t gc4053_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    esp_err_t ret = ESP_FAIL;
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);
#if CONFIG_SOC_MIPI_CSI_SUPPORTED
    if (dev->sensor_port == ESP_CAM_SENSOR_MIPI_CSI) {
        formats->count = ARRAY_SIZE(gc4053_format_info_mipi);
        formats->format_array = &gc4053_format_info_mipi[0];
        ret = ESP_OK;
    }
#endif

    return ret;
}

static esp_err_t gc4053_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_raw = 1;
    return 0;
}

#if GC4053_EXPOSURE_TEST_EN
static volatile uint32_t s_exp_v = 0x05;
static bool s_exp_add = true;
TimerHandle_t ae_timer_handle;
static void ae_timer_callback(TimerHandle_t timer)
{
    esp_cam_sensor_device_t *dev = (esp_cam_sensor_device_t *)pvTimerGetTimerID(timer);
    struct gc4053_cam *cam_gc4053 = (struct gc4053_cam *)dev->priv;
#if GC4053_EXPOSURE_TEST_EN_GAIN
    if (s_exp_v >= cam_gc4053->gc4053_para.limited_abs_gain_index) {
        s_exp_add = false;
    } else if (s_exp_v < 1) {
        s_exp_add = true;
    }
    gc4053_set_total_gain_val(dev, s_exp_v);
    if (s_exp_add == true) {
        s_exp_v += 1;
    } else {
        s_exp_v -= 1;
    }
#else
    if (s_exp_v >= cam_gc4053->gc4053_para.exposure_max) {
        s_exp_add = false;
    } else if (s_exp_v < s_gc4053_exp_min) {
        s_exp_add = true;
    }
    gc4053_set_exp_val(dev, s_exp_v);
    if (s_exp_add == true) {
        s_exp_v += 2;
    } else {
        s_exp_v -= 2;
    }
#endif
    ESP_LOGI(TAG, "E=%" PRIu32, s_exp_v);
}
#endif

static esp_err_t gc4053_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    struct gc4053_cam *cam_gc4053 = (struct gc4053_cam *)dev->priv;
    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
#if CONFIG_SOC_MIPI_CSI_SUPPORTED
        if (dev->sensor_port == ESP_CAM_SENSOR_MIPI_CSI) {
            format = &gc4053_format_info_mipi[get_gc4053_actual_format_index()];
        }
#endif
    }
    ESP_RETURN_ON_FALSE(format != NULL, ESP_ERR_NOT_SUPPORTED, TAG, "format is NULL");

    ret = gc4053_write_array(dev->sccb_handle, (gc4053_reginfo_t *)format->regs);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT, TAG, "set format regs failed");
    ESP_LOGD(TAG, "Set format %s", format->name);
    dev->cur_format = format;

    // init para
    cam_gc4053->gc4053_para.exposure_val = dev->cur_format->isp_info->isp_v1_info.exp_def;
    cam_gc4053->gc4053_para.gain_index = dev->cur_format->isp_info->isp_v1_info.gain_def;
    cam_gc4053->gc4053_para.exposure_max = dev->cur_format->isp_info->isp_v1_info.vts - s_gc4053_exp_max_offset;
#if GC4053_EXPOSURE_TEST_EN
    ae_timer_handle = xTimerCreate("AE_t", 200 / portTICK_PERIOD_MS, pdTRUE,
                                   (void *)dev, ae_timer_callback);
    xTimerStart(ae_timer_handle, portMAX_DELAY);
#endif
    return ret;
}

static esp_err_t gc4053_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
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

static esp_err_t gc4053_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    GC4053_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = gc4053_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = gc4053_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = gc4053_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = gc4053_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = gc4053_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = gc4053_get_sensor_id(dev, arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = gc4053_set_test_pattern(dev, *(int *)arg);
        break;
    default:
        break;
    }

    GC4053_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t gc4053_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        GC4053_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->pwdn_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        ret = gpio_config(&conf);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "gpio config failed");

        // carefully, logic is inverted compared to reset pin
        gpio_set_level(dev->pwdn_pin, 1);
        delay_ms(10);
    }

    if (dev->reset_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->reset_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        ret = gpio_config(&conf);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "gpio config failed");

        gpio_set_level(dev->reset_pin, 1);
    }
    delay_ms(6);

    return ret;
}

static esp_err_t gc4053_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        GC4053_DISABLE_OUT_XCLK(dev->xclk_pin);
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

static esp_err_t gc4053_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del gc4053 (%p)", dev);
    if (dev) {
        gc4053_power_off(dev);
        if (dev->priv) {
            free(dev->priv);
            dev->priv = NULL;
        }
        free(dev);
        dev = NULL;
    }

    return ESP_OK;
}

static const esp_cam_sensor_ops_t gc4053_ops = {
    .query_para_desc = gc4053_query_para_desc,
    .get_para_value = gc4053_get_para_value,
    .set_para_value = gc4053_set_para_value,
    .query_support_formats = gc4053_query_support_formats,
    .query_support_capability = gc4053_query_support_capability,
    .set_format = gc4053_set_format,
    .get_format = gc4053_get_format,
    .priv_ioctl = gc4053_priv_ioctl,
    .del = gc4053_delete
};

esp_cam_sensor_device_t *gc4053_detect(esp_cam_sensor_config_t *config)
{
    esp_cam_sensor_device_t *dev = NULL;
    struct gc4053_cam *cam_gc4053;

    if (config == NULL) {
        return NULL;
    }

    dev = calloc(1, sizeof(esp_cam_sensor_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    cam_gc4053 = heap_caps_calloc(1, sizeof(struct gc4053_cam), MALLOC_CAP_DEFAULT);
    if (!cam_gc4053) {
        ESP_LOGE(TAG, "failed to calloc cam");
        free(dev);
        return NULL;
    }

    cam_gc4053->gc4053_para.limited_abs_gain_index = ARRAY_SIZE(gc4053_total_gain_val_map) - 1;
    for (size_t i = 0; i < ARRAY_SIZE(gc4053_total_gain_val_map); i++) {
        if (gc4053_total_gain_val_map[i] > s_limited_gain) {
            cam_gc4053->gc4053_para.limited_abs_gain_index = (i > 0) ? (i - 1) : 0;
            break;
        }
    }

    dev->name = (char *)GC4053_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &gc4053_ops;
    dev->priv = cam_gc4053;
#if CONFIG_SOC_MIPI_CSI_SUPPORTED
    if (config->sensor_port == ESP_CAM_SENSOR_MIPI_CSI) {
        dev->cur_format = &gc4053_format_info_mipi[get_gc4053_actual_format_index()];
    }
#endif

    // Configure sensor power, clock, and SCCB port
    if (gc4053_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (gc4053_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != GC4053_PID) {
        ESP_LOGE(TAG, "Camera sensor is not GC4053, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    gc4053_power_off(dev);
    free(dev->priv);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_GC4053_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(gc4053_detect, ESP_CAM_SENSOR_MIPI_CSI, GC4053_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return gc4053_detect(config);
}
#endif
