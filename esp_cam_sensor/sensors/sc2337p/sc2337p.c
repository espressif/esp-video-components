/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <sys/param.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"
#include "sc2337p_settings.h"
#include "sc2337p.h"

/*
 * SC2337P camera sensor gain control.
 * Note1: The analog gain only has coarse gain, and no fine gain, so in the adjustment of analog gain.
 * Digital gain needs to replace analog fine gain for smooth transition, so as to avoid AGC oscillation.
 * Note2: the analog gain of sc2337p will be affected by temperature, it is recommended to increase Dgain first and then Again.
 */
typedef struct {
    uint8_t dgain_fine; // digital gain fine
    uint8_t dgain_coarse; // digital gain coarse
    uint8_t analog_gain;
} sc2337p_gain_t;

typedef struct {
    uint32_t exposure_val;
    uint32_t exposure_max;
    uint32_t gain_index; // current gain index
    size_t limited_gain_index; // number of valid gain entries (exclusive upper bound)

    uint32_t vflip_en : 1;
    uint32_t hmirror_en : 1;
} sc2337p_para_t;

struct sc2337p_cam {
    sc2337p_para_t sc2337p_para;
};

#define SC2337P_IO_MUX_LOCK(mux)
#define SC2337P_IO_MUX_UNLOCK(mux)
#define SC2337P_ENABLE_OUT_XCLK(pin,clk)
#define SC2337P_DISABLE_OUT_XCLK(pin)

#define EXPOSURE_V4L2_UNIT_US                   100
#define EXPOSURE_V4L2_TO_SC2337P(v, sf)          \
    ((uint32_t)(((double)v) * EXPOSURE_V4L2_UNIT_US * 1000 / (((sf)->isp_info->isp_v1_info.tline_ns)) + 0.5))
#define EXPOSURE_SC2337P_TO_V4L2(v, sf)          \
    ((int32_t)(((double)v) * (((sf)->isp_info->isp_v1_info.tline_ns)) / EXPOSURE_V4L2_UNIT_US / 1000 + 0.5))

#define SC2337P_VTS_MAX          0x7fff // Max exposure is VTS-6
#define SC2337P_EXP_MAX_OFFSET   0x06

#define SC2337P_FETCH_EXP_H(val)     (((val) >> 12) & 0xF)
#define SC2337P_FETCH_EXP_M(val)     (((val) >> 4) & 0xFF)
#define SC2337P_FETCH_EXP_L(val)     (((val) & 0xF) << 4)

#define SC2337P_FETCH_DGAIN_COARSE(val)  (((val) >> 8) & 0x03)
#define SC2337P_FETCH_DGAIN_FINE(val)    ((val) & 0xFF)

#define SC2337P_GROUP_HOLD_START        0x00
#define SC2337P_GROUP_HOLD_END          0x30
#define SC2337P_GROUP_HOLD_DELAY_FRAMES 0x01

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))
#define SC2337P_SUPPORT_NUM CONFIG_CAMERA_SC2337P_MAX_SUPPORT

static const uint32_t s_limited_gain = CONFIG_CAMERA_SC2337P_ABSOLUTE_GAIN_LIMIT;
static const uint8_t s_sc2337p_exp_min = 0x08;
static const char *TAG = "sc2337p";

#if CONFIG_CAMERA_SC2337P_ANA_GAIN_PRIORITY
// total gain = analog_gain x digital_gain x 1000(To avoid decimal points, the final abs_gain is multiplied by 1000.)
static const uint32_t sc2337p_total_gain_val_map[] = {
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
    // 2X
    2000,
    2062,
    2126,
    2188,
    2250,
    2312,
    2376,
    2438,
    2500,
    2562,
    2626,
    2688,
    2750,
    2812,
    2876,
    2938,
    3000,
    3062,
    3126,
    3188,
    3250,
    3312,
    3376,
    3438,
    3500,
    3562,
    3626,
    3688,
    3750,
    3812,
    3876,
    3938,
    // 4X
    4000,
    4124,
    4252,
    4376,
    4500,
    4624,
    4752,
    4876,
    5000,
    5124,
    5252,
    5376,
    5500,
    5624,
    5752,
    5876,
    6000,
    6124,
    6252,
    6376,
    6500,
    6624,
    6752,
    6876,
    7000,
    7124,
    7252,
    7376,
    7500,
    7624,
    7752,
    7876,
    // 8X
    8000,
    8248,
    8504,
    8752,
    9000,
    9248,
    9504,
    9752,
    10000,
    10248,
    10504,
    10752,
    11000,
    11248,
    11504,
    11752,
    12000,
    12248,
    12504,
    12752,
    13000,
    13248,
    13504,
    13752,
    14000,
    14248,
    14504,
    14752,
    15000,
    15248,
    15504,
    15752,
    // 16X
    16000,
    16496,
    17008,
    17504,
    18000,
    18496,
    19008,
    19504,
    20000,
    20496,
    21008,
    21504,
    22000,
    22496,
    23008,
    23504,
    24000,
    24496,
    25008,
    25504,
    26000,
    26496,
    27008,
    27504,
    28000,
    28496,
    29008,
    29504,
    30000,
    30496,
    31008,
    31504,
    // 32X
    32000,
    32992,
    34016,
    35008,
    36000,
    36992,
    38016,
    39008,
    40000,
    40992,
    42016,
    43008,
    44000,
    44992,
    46016,
    47008,
    48000,
    48992,
    50016,
    51008,
    52000,
    52992,
    54016,
    55008,
    56000,
    56992,
    58016,
    59008,
    60000,
    60992,
    62016,
    63008,
};

// SC2337P Gain map format: [DIG_FINE, DIG_COARSE, ANG]
static const sc2337p_gain_t sc2337p_gain_map[] = {
    {0x80, 0x00, 0x00},
    {0x84, 0x00, 0x00},
    {0x88, 0x00, 0x00},
    {0x8c, 0x00, 0x00},
    {0x90, 0x00, 0x00},
    {0x94, 0x00, 0x00},
    {0x98, 0x00, 0x00},
    {0x9c, 0x00, 0x00},
    {0xa0, 0x00, 0x00},
    {0xa4, 0x00, 0x00},
    {0xa8, 0x00, 0x00},
    {0xac, 0x00, 0x00},
    {0xb0, 0x00, 0x00},
    {0xb4, 0x00, 0x00},
    {0xb8, 0x00, 0x00},
    {0xbc, 0x00, 0x00},
    {0xc0, 0x00, 0x00},
    {0xc4, 0x00, 0x00},
    {0xc8, 0x00, 0x00},
    {0xcc, 0x00, 0x00},
    {0xd0, 0x00, 0x00},
    {0xd4, 0x00, 0x00},
    {0xd8, 0x00, 0x00},
    {0xdc, 0x00, 0x00},
    {0xe0, 0x00, 0x00},
    {0xe4, 0x00, 0x00},
    {0xe8, 0x00, 0x00},
    {0xec, 0x00, 0x00},
    {0xf0, 0x00, 0x00},
    {0xf4, 0x00, 0x00},
    {0xf8, 0x00, 0x00},
    {0xfc, 0x00, 0x00},
    // 2X
    {0x80, 0x00, 0x08},
    {0x84, 0x00, 0x08},
    {0x88, 0x00, 0x08},
    {0x8c, 0x00, 0x08},
    {0x90, 0x00, 0x08},
    {0x94, 0x00, 0x08},
    {0x98, 0x00, 0x08},
    {0x9c, 0x00, 0x08},
    {0xa0, 0x00, 0x08},
    {0xa4, 0x00, 0x08},
    {0xa8, 0x00, 0x08},
    {0xac, 0x00, 0x08},
    {0xb0, 0x00, 0x08},
    {0xb4, 0x00, 0x08},
    {0xb8, 0x00, 0x08},
    {0xbc, 0x00, 0x08},
    {0xc0, 0x00, 0x08},
    {0xc4, 0x00, 0x08},
    {0xc8, 0x00, 0x08},
    {0xcc, 0x00, 0x08},
    {0xd0, 0x00, 0x08},
    {0xd4, 0x00, 0x08},
    {0xd8, 0x00, 0x08},
    {0xdc, 0x00, 0x08},
    {0xe0, 0x00, 0x08},
    {0xe4, 0x00, 0x08},
    {0xe8, 0x00, 0x08},
    {0xec, 0x00, 0x08},
    {0xf0, 0x00, 0x08},
    {0xf4, 0x00, 0x08},
    {0xf8, 0x00, 0x08},
    {0xfc, 0x00, 0x08},
    // 4X
    {0x80, 0x00, 0x09},
    {0x84, 0x00, 0x09},
    {0x88, 0x00, 0x09},
    {0x8c, 0x00, 0x09},
    {0x90, 0x00, 0x09},
    {0x94, 0x00, 0x09},
    {0x98, 0x00, 0x09},
    {0x9c, 0x00, 0x09},
    {0xa0, 0x00, 0x09},
    {0xa4, 0x00, 0x09},
    {0xa8, 0x00, 0x09},
    {0xac, 0x00, 0x09},
    {0xb0, 0x00, 0x09},
    {0xb4, 0x00, 0x09},
    {0xb8, 0x00, 0x09},
    {0xbc, 0x00, 0x09},
    {0xc0, 0x00, 0x09},
    {0xc4, 0x00, 0x09},
    {0xc8, 0x00, 0x09},
    {0xcc, 0x00, 0x09},
    {0xd0, 0x00, 0x09},
    {0xd4, 0x00, 0x09},
    {0xd8, 0x00, 0x09},
    {0xdc, 0x00, 0x09},
    {0xe0, 0x00, 0x09},
    {0xe4, 0x00, 0x09},
    {0xe8, 0x00, 0x09},
    {0xec, 0x00, 0x09},
    {0xf0, 0x00, 0x09},
    {0xf4, 0x00, 0x09},
    {0xf8, 0x00, 0x09},
    {0xfc, 0x00, 0x09},
    // 8X
    {0x80, 0x00, 0x0b},
    {0x84, 0x00, 0x0b},
    {0x88, 0x00, 0x0b},
    {0x8c, 0x00, 0x0b},
    {0x90, 0x00, 0x0b},
    {0x94, 0x00, 0x0b},
    {0x98, 0x00, 0x0b},
    {0x9c, 0x00, 0x0b},
    {0xa0, 0x00, 0x0b},
    {0xa4, 0x00, 0x0b},
    {0xa8, 0x00, 0x0b},
    {0xac, 0x00, 0x0b},
    {0xb0, 0x00, 0x0b},
    {0xb4, 0x00, 0x0b},
    {0xb8, 0x00, 0x0b},
    {0xbc, 0x00, 0x0b},
    {0xc0, 0x00, 0x0b},
    {0xc4, 0x00, 0x0b},
    {0xc8, 0x00, 0x0b},
    {0xcc, 0x00, 0x0b},
    {0xd0, 0x00, 0x0b},
    {0xd4, 0x00, 0x0b},
    {0xd8, 0x00, 0x0b},
    {0xdc, 0x00, 0x0b},
    {0xe0, 0x00, 0x0b},
    {0xe4, 0x00, 0x0b},
    {0xe8, 0x00, 0x0b},
    {0xec, 0x00, 0x0b},
    {0xf0, 0x00, 0x0b},
    {0xf4, 0x00, 0x0b},
    {0xf8, 0x00, 0x0b},
    {0xfc, 0x00, 0x0b},
    // 16X
    {0x80, 0x00, 0x0f},
    {0x84, 0x00, 0x0f},
    {0x88, 0x00, 0x0f},
    {0x8c, 0x00, 0x0f},
    {0x90, 0x00, 0x0f},
    {0x94, 0x00, 0x0f},
    {0x98, 0x00, 0x0f},
    {0x9c, 0x00, 0x0f},
    {0xa0, 0x00, 0x0f},
    {0xa4, 0x00, 0x0f},
    {0xa8, 0x00, 0x0f},
    {0xac, 0x00, 0x0f},
    {0xb0, 0x00, 0x0f},
    {0xb4, 0x00, 0x0f},
    {0xb8, 0x00, 0x0f},
    {0xbc, 0x00, 0x0f},
    {0xc0, 0x00, 0x0f},
    {0xc4, 0x00, 0x0f},
    {0xc8, 0x00, 0x0f},
    {0xcc, 0x00, 0x0f},
    {0xd0, 0x00, 0x0f},
    {0xd4, 0x00, 0x0f},
    {0xd8, 0x00, 0x0f},
    {0xdc, 0x00, 0x0f},
    {0xe0, 0x00, 0x0f},
    {0xe4, 0x00, 0x0f},
    {0xe8, 0x00, 0x0f},
    {0xec, 0x00, 0x0f},
    {0xf0, 0x00, 0x0f},
    {0xf4, 0x00, 0x0f},
    {0xf8, 0x00, 0x0f},
    {0xfc, 0x00, 0x0f},
    //32x
    {0x80, 0x00, 0x1f},
    {0x84, 0x00, 0x1f},
    {0x88, 0x00, 0x1f},
    {0x8c, 0x00, 0x1f},
    {0x90, 0x00, 0x1f},
    {0x94, 0x00, 0x1f},
    {0x98, 0x00, 0x1f},
    {0x9c, 0x00, 0x1f},
    {0xa0, 0x00, 0x1f},
    {0xa4, 0x00, 0x1f},
    {0xa8, 0x00, 0x1f},
    {0xac, 0x00, 0x1f},
    {0xb0, 0x00, 0x1f},
    {0xb4, 0x00, 0x1f},
    {0xb8, 0x00, 0x1f},
    {0xbc, 0x00, 0x1f},
    {0xc0, 0x00, 0x1f},
    {0xc4, 0x00, 0x1f},
    {0xc8, 0x00, 0x1f},
    {0xcc, 0x00, 0x1f},
    {0xd0, 0x00, 0x1f},
    {0xd4, 0x00, 0x1f},
    {0xd8, 0x00, 0x1f},
    {0xdc, 0x00, 0x1f},
    {0xe0, 0x00, 0x1f},
    {0xe4, 0x00, 0x1f},
    {0xe8, 0x00, 0x1f},
    {0xec, 0x00, 0x1f},
    {0xf0, 0x00, 0x1f},
    {0xf4, 0x00, 0x1f},
    {0xf8, 0x00, 0x1f},
    {0xfc, 0x00, 0x1f},
};
#elif CONFIG_CAMERA_SC2337P_DIG_GAIN_PRIORITY
// total gain = analog_gain x digital_gain x 1000(To avoid decimal points, the final total_gain is multiplied by 1000.)
static const uint32_t sc2337p_total_gain_val_map[] = {
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
    // 2X
    2000,
    2063,
    2125,
    2188,
    2250,
    2313,
    2375,
    2438,
    2500,
    2563,
    2625,
    2688,
    2750,
    2813,
    2875,
    2938,
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
    // 4X
    4000,
    4126,
    4250,
    4376,
    4500,
    4626,
    4750,
    4876,
    5000,
    5126,
    5250,
    5376,
    5500,
    5626,
    5750,
    5876,
    6000,
    6126,
    6250,
    6376,
    6500,
    6626,
    6750,
    6876,
    7000,
    7126,
    7250,
    7376,
    7500,
    7626,
    7750,
    7876,
    // 8X
    8000,
    8252,
    8500,
    8752,
    9000,
    9252,
    9500,
    9752,
    10000,
    10252,
    10500,
    10752,
    11000,
    11252,
    11500,
    11752,
    12000,
    12252,
    12500,
    12752,
    13000,
    13252,
    13500,
    13752,
    14000,
    14252,
    14500,
    14752,
    15000,
    15252,
    15500,
    15752,
    // 16X
    16000,
    16504,
    17000,
    17504,
    18000,
    18504,
    19000,
    19504,
    20000,
    20504,
    21000,
    21504,
    22000,
    22504,
    23000,
    23504,
    24000,
    24504,
    25000,
    25504,
    26000,
    26504,
    27000,
    27504,
    28000,
    28504,
    29000,
    29504,
    30000,
    30504,
    31000,
    31504,
    // 32X
    32000,
    33008,
    34000,
    35008,
    36000,
    37008,
    38000,
    39008,
    40000,
    41008,
    42000,
    43008,
    44000,
    45008,
    46000,
    47008,
    48000,
    49008,
    50000,
    51008,
    52000,
    53008,
    54000,
    55008,
    56000,
    57008,
    58000,
    59008,
    60000,
    61008,
    62000,
    63008,
    // 64X
    64000,
    66016,
    68000,
    70016,
    72000,
    74016,
    76000,
    78016,
    80000,
    82016,
    84000,
    86016,
    88000,
    90016,
    92000,
    94016,
    96000,
    98016,
    100000,
    102016,
    104000,
    106016,
    108000,
    110016,
    112000,
    114016,
    116000,
    118016,
    120000,
    122016,
    124000,
    126016,
};

// SC2337P Gain map format: [DIG_FINE, DIG_COARSE, ANG]
static const sc2337p_gain_t sc2337p_gain_map[] = {
    {0x80, 0x00, 0x00},
    {0x84, 0x00, 0x00},
    {0x88, 0x00, 0x00},
    {0x8c, 0x00, 0x00},
    {0x90, 0x00, 0x00},
    {0x94, 0x00, 0x00},
    {0x98, 0x00, 0x00},
    {0x9c, 0x00, 0x00},
    {0xa0, 0x00, 0x00},
    {0xa4, 0x00, 0x00},
    {0xa8, 0x00, 0x00},
    {0xac, 0x00, 0x00},
    {0xb0, 0x00, 0x00},
    {0xb4, 0x00, 0x00},
    {0xb8, 0x00, 0x00},
    {0xbc, 0x00, 0x00},
    {0xc0, 0x00, 0x00},
    {0xc4, 0x00, 0x00},
    {0xc8, 0x00, 0x00},
    {0xcc, 0x00, 0x00},
    {0xd0, 0x00, 0x00},
    {0xd4, 0x00, 0x00},
    {0xd8, 0x00, 0x00},
    {0xdc, 0x00, 0x00},
    {0xe0, 0x00, 0x00},
    {0xe4, 0x00, 0x00},
    {0xe8, 0x00, 0x00},
    {0xec, 0x00, 0x00},
    {0xf0, 0x00, 0x00},
    {0xf4, 0x00, 0x00},
    {0xf8, 0x00, 0x00},
    {0xfc, 0x00, 0x00},
    // 2X
    {0x80, 0x01, 0x00},
    {0x84, 0x01, 0x00},
    {0x88, 0x01, 0x00},
    {0x8c, 0x01, 0x00},
    {0x90, 0x01, 0x00},
    {0x94, 0x01, 0x00},
    {0x98, 0x01, 0x00},
    {0x9c, 0x01, 0x00},
    {0xa0, 0x01, 0x00},
    {0xa4, 0x01, 0x00},
    {0xa8, 0x01, 0x00},
    {0xac, 0x01, 0x00},
    {0xb0, 0x01, 0x00},
    {0xb4, 0x01, 0x00},
    {0xb8, 0x01, 0x00},
    {0xbc, 0x01, 0x00},
    {0xc0, 0x01, 0x00},
    {0xc4, 0x01, 0x00},
    {0xc8, 0x01, 0x00},
    {0xcc, 0x01, 0x00},
    {0xd0, 0x01, 0x00},
    {0xd4, 0x01, 0x00},
    {0xd8, 0x01, 0x00},
    {0xdc, 0x01, 0x00},
    {0xe0, 0x01, 0x00},
    {0xe4, 0x01, 0x00},
    {0xe8, 0x01, 0x00},
    {0xec, 0x01, 0x00},
    {0xf0, 0x01, 0x00},
    {0xf4, 0x01, 0x00},
    {0xf8, 0x01, 0x00},
    {0xfc, 0x01, 0x00},
    // 4X
    {0x80, 0x01, 0x08},
    {0x84, 0x01, 0x08},
    {0x88, 0x01, 0x08},
    {0x8c, 0x01, 0x08},
    {0x90, 0x01, 0x08},
    {0x94, 0x01, 0x08},
    {0x98, 0x01, 0x08},
    {0x9c, 0x01, 0x08},
    {0xa0, 0x01, 0x08},
    {0xa4, 0x01, 0x08},
    {0xa8, 0x01, 0x08},
    {0xac, 0x01, 0x08},
    {0xb0, 0x01, 0x08},
    {0xb4, 0x01, 0x08},
    {0xb8, 0x01, 0x08},
    {0xbc, 0x01, 0x08},
    {0xc0, 0x01, 0x08},
    {0xc4, 0x01, 0x08},
    {0xc8, 0x01, 0x08},
    {0xcc, 0x01, 0x08},
    {0xd0, 0x01, 0x08},
    {0xd4, 0x01, 0x08},
    {0xd8, 0x01, 0x08},
    {0xdc, 0x01, 0x08},
    {0xe0, 0x01, 0x08},
    {0xe4, 0x01, 0x08},
    {0xe8, 0x01, 0x08},
    {0xec, 0x01, 0x08},
    {0xf0, 0x01, 0x08},
    {0xf4, 0x01, 0x08},
    {0xf8, 0x01, 0x08},
    {0xfc, 0x01, 0x08},
    // 8X
    {0x80, 0x01, 0x09},
    {0x84, 0x01, 0x09},
    {0x88, 0x01, 0x09},
    {0x8c, 0x01, 0x09},
    {0x90, 0x01, 0x09},
    {0x94, 0x01, 0x09},
    {0x98, 0x01, 0x09},
    {0x9c, 0x01, 0x09},
    {0xa0, 0x01, 0x09},
    {0xa4, 0x01, 0x09},
    {0xa8, 0x01, 0x09},
    {0xac, 0x01, 0x09},
    {0xb0, 0x01, 0x09},
    {0xb4, 0x01, 0x09},
    {0xb8, 0x01, 0x09},
    {0xbc, 0x01, 0x09},
    {0xc0, 0x01, 0x09},
    {0xc4, 0x01, 0x09},
    {0xc8, 0x01, 0x09},
    {0xcc, 0x01, 0x09},
    {0xd0, 0x01, 0x09},
    {0xd4, 0x01, 0x09},
    {0xd8, 0x01, 0x09},
    {0xdc, 0x01, 0x09},
    {0xe0, 0x01, 0x09},
    {0xe4, 0x01, 0x09},
    {0xe8, 0x01, 0x09},
    {0xec, 0x01, 0x09},
    {0xf0, 0x01, 0x09},
    {0xf4, 0x01, 0x09},
    {0xf8, 0x01, 0x09},
    {0xfc, 0x01, 0x09},
    // 16X
    {0x80, 0x01, 0x0b},
    {0x84, 0x01, 0x0b},
    {0x88, 0x01, 0x0b},
    {0x8c, 0x01, 0x0b},
    {0x90, 0x01, 0x0b},
    {0x94, 0x01, 0x0b},
    {0x98, 0x01, 0x0b},
    {0x9c, 0x01, 0x0b},
    {0xa0, 0x01, 0x0b},
    {0xa4, 0x01, 0x0b},
    {0xa8, 0x01, 0x0b},
    {0xac, 0x01, 0x0b},
    {0xb0, 0x01, 0x0b},
    {0xb4, 0x01, 0x0b},
    {0xb8, 0x01, 0x0b},
    {0xbc, 0x01, 0x0b},
    {0xc0, 0x01, 0x0b},
    {0xc4, 0x01, 0x0b},
    {0xc8, 0x01, 0x0b},
    {0xcc, 0x01, 0x0b},
    {0xd0, 0x01, 0x0b},
    {0xd4, 0x01, 0x0b},
    {0xd8, 0x01, 0x0b},
    {0xdc, 0x01, 0x0b},
    {0xe0, 0x01, 0x0b},
    {0xe4, 0x01, 0x0b},
    {0xe8, 0x01, 0x0b},
    {0xec, 0x01, 0x0b},
    {0xf0, 0x01, 0x0b},
    {0xf4, 0x01, 0x0b},
    {0xf8, 0x01, 0x0b},
    {0xfc, 0x01, 0x0b},
    //32x
    {0x80, 0x01, 0x0f},
    {0x84, 0x01, 0x0f},
    {0x88, 0x01, 0x0f},
    {0x8c, 0x01, 0x0f},
    {0x90, 0x01, 0x0f},
    {0x94, 0x01, 0x0f},
    {0x98, 0x01, 0x0f},
    {0x9c, 0x01, 0x0f},
    {0xa0, 0x01, 0x0f},
    {0xa4, 0x01, 0x0f},
    {0xa8, 0x01, 0x0f},
    {0xac, 0x01, 0x0f},
    {0xb0, 0x01, 0x0f},
    {0xb4, 0x01, 0x0f},
    {0xb8, 0x01, 0x0f},
    {0xbc, 0x01, 0x0f},
    {0xc0, 0x01, 0x0f},
    {0xc4, 0x01, 0x0f},
    {0xc8, 0x01, 0x0f},
    {0xcc, 0x01, 0x0f},
    {0xd0, 0x01, 0x0f},
    {0xd4, 0x01, 0x0f},
    {0xd8, 0x01, 0x0f},
    {0xdc, 0x01, 0x0f},
    {0xe0, 0x01, 0x0f},
    {0xe4, 0x01, 0x0f},
    {0xe8, 0x01, 0x0f},
    {0xec, 0x01, 0x0f},
    {0xf0, 0x01, 0x0f},
    {0xf4, 0x01, 0x0f},
    {0xf8, 0x01, 0x0f},
    {0xfc, 0x01, 0x0f},
    //64x
    {0x80, 0x01, 0x1f},
    {0x84, 0x01, 0x1f},
    {0x88, 0x01, 0x1f},
    {0x8c, 0x01, 0x1f},
    {0x90, 0x01, 0x1f},
    {0x94, 0x01, 0x1f},
    {0x98, 0x01, 0x1f},
    {0x9c, 0x01, 0x1f},
    {0xa0, 0x01, 0x1f},
    {0xa4, 0x01, 0x1f},
    {0xa8, 0x01, 0x1f},
    {0xac, 0x01, 0x1f},
    {0xb0, 0x01, 0x1f},
    {0xb4, 0x01, 0x1f},
    {0xb8, 0x01, 0x1f},
    {0xbc, 0x01, 0x1f},
    {0xc0, 0x01, 0x1f},
    {0xc4, 0x01, 0x1f},
    {0xc8, 0x01, 0x1f},
    {0xcc, 0x01, 0x1f},
    {0xd0, 0x01, 0x1f},
    {0xd4, 0x01, 0x1f},
    {0xd8, 0x01, 0x1f},
    {0xdc, 0x01, 0x1f},
    {0xe0, 0x01, 0x1f},
    {0xe4, 0x01, 0x1f},
    {0xe8, 0x01, 0x1f},
    {0xec, 0x01, 0x1f},
    {0xf0, 0x01, 0x1f},
    {0xf4, 0x01, 0x1f},
    {0xf8, 0x01, 0x1f},
    {0xfc, 0x01, 0x1f},
};
#endif

#if CONFIG_SOC_MIPI_CSI_SUPPORTED
static const esp_cam_sensor_isp_info_t sc2337p_isp_info_mipi[] = {
    /* index 0: RAW10 1088x1080/1920x1080 30fps (371.25Mbps) */
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 74250000,
            .vts = 1125, //0x0465,
            .hts = 2200, //0x0898,
            .tline_ns = 29630,
            .gain_def = 0,
            .exp_def = 0x438,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        },
    },
    /* index 1: RAW8 1920x1080 30fps (324Mbps) */
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 81000000,
            .vts = 1200, //0x04b0
            .hts = 2250, //0x08ca
            .tline_ns = 27778,
            .gain_def = 0,
            .exp_def = 0x438,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        },
    },
};

static const esp_cam_sensor_format_t sc2337p_format_info_mipi[] = {
#if CONFIG_CAMERA_SC2337P_MIPI_RAW10_1088X1080_30FPS
    {
        .name = "MIPI_2lane_24Minput_RAW10_1088x1080_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1088,
        .height = 1080,
        .regs = sc2337p_mipi_2lane_24Minput_1088x1080_raw10_30fps,
        .regs_size = ARRAY_SIZE(sc2337p_mipi_2lane_24Minput_1088x1080_raw10_30fps),
        .fps = 30,
        .isp_info = &sc2337p_isp_info_mipi[0],
        .mipi_info = {
            .mipi_clk = 371250000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
#endif
#if CONFIG_CAMERA_SC2337P_MIPI_RAW10_1920X1080_30FPS
    {
        .name = "MIPI_2lane_24Minput_RAW10_1920x1080_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1920,
        .height = 1080,
        .regs = sc2337p_mipi_2lane_24Minput_1920x1080_raw10_30fps,
        .regs_size = ARRAY_SIZE(sc2337p_mipi_2lane_24Minput_1920x1080_raw10_30fps),
        .fps = 30,
        .isp_info = &sc2337p_isp_info_mipi[0],
        .mipi_info = {
            .mipi_clk = 371250000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
#endif
#if CONFIG_CAMERA_SC2337P_MIPI_RAW8_1920X1080_30FPS
    {
        .name = "MIPI_2lane_24Minput_RAW8_1920x1080_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1920,
        .height = 1080,
        .regs = sc2337p_mipi_2lane_24Minput_1920x1080_raw8_30fps,
        .regs_size = ARRAY_SIZE(sc2337p_mipi_2lane_24Minput_1920x1080_raw8_30fps),
        .fps = 30,
        .isp_info = &sc2337p_isp_info_mipi[1],
        .mipi_info = {
            .mipi_clk = 324000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
#endif
};

static const int sc2337p_mipi_format_index[] = {
#if CONFIG_CAMERA_SC2337P_MIPI_RAW10_1088X1080_30FPS
    0,
#endif
#if CONFIG_CAMERA_SC2337P_MIPI_RAW10_1920X1080_30FPS
    1,
#endif
#if CONFIG_CAMERA_SC2337P_MIPI_RAW8_1920X1080_30FPS
    2,
#endif
};

static int get_sc2337p_mipi_actual_format_index(void)
{
    int default_index = CONFIG_CAMERA_SC2337P_MIPI_IF_FORMAT_INDEX_DEFAULT;
    for (size_t i = 0; i < ARRAY_SIZE(sc2337p_mipi_format_index); i++) {
        if (sc2337p_mipi_format_index[i] == default_index) {
            return i;
        }
    }
    return 0;
}
#endif

static esp_err_t sc2337p_read(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a16v8(sccb_handle, reg, read_buf);
}

static esp_err_t sc2337p_write(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t data)
{
    return esp_sccb_transmit_reg_a16v8(sccb_handle, reg, data);
}

/* write a array of registers  */
static esp_err_t sc2337p_write_array(esp_sccb_io_handle_t sccb_handle, sc2337p_reginfo_t *regarray)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && regarray[i].reg != SC2337P_REG_END) {
        if (regarray[i].reg != SC2337P_REG_DELAY) {
            ret = sc2337p_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    return ret;
}

static esp_err_t sc2337p_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_data = 0;

    ret = sc2337p_read(sccb_handle, reg, &reg_data);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (reg_data & ~mask) | ((value << offset) & mask);
    ret = sc2337p_write(sccb_handle, reg, value);
    return ret;
}

static esp_err_t sc2337p_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    return sc2337p_set_reg_bits(dev->sccb_handle, 0x4501, 3, 1, enable ? 0x01 : 0x00);
}

static esp_err_t sc2337p_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return ESP_OK;
}

static esp_err_t sc2337p_soft_reset(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = sc2337p_set_reg_bits(dev->sccb_handle, 0x0103, 0, 1, 0x01);
    delay_ms(5);
    return ret;
}

static esp_err_t sc2337p_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t pid_h, pid_l;

    ret = sc2337p_read(dev->sccb_handle, SC2337P_REG_SENSOR_ID_H, &pid_h);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = sc2337p_read(dev->sccb_handle, SC2337P_REG_SENSOR_ID_L, &pid_l);
    if (ret != ESP_OK) {
        return ret;
    }
    id->pid = (pid_h << 8) | pid_l;

    return ret;
}

static esp_err_t sc2337p_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;
    ret = sc2337p_write(dev->sccb_handle, SC2337P_REG_SLEEP_MODE, enable ? 0x01 : 0x00);
    if (ret == ESP_OK) {
        dev->stream_status = enable;
        ESP_LOGD(TAG, "Stream=%d", enable);
    }
    return ret;
}

static esp_err_t sc2337p_set_mirror(esp_cam_sensor_device_t *dev, int enable)
{
    return sc2337p_set_reg_bits(dev->sccb_handle, 0x3221, 1, 2,  enable ? 0x03 : 0x00);
}

static esp_err_t sc2337p_set_vflip(esp_cam_sensor_device_t *dev, int enable)
{
    return sc2337p_set_reg_bits(dev->sccb_handle, 0x3221, 5, 2, enable ? 0x03 : 0x00);
}

static esp_err_t sc2337p_set_exp_val(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    esp_err_t ret;
    struct sc2337p_cam *cam_sc2337p = (struct sc2337p_cam *)dev->priv;
    uint32_t value_buf = MAX(u32_val, s_sc2337p_exp_min);
    value_buf = MIN(value_buf, cam_sc2337p->sc2337p_para.exposure_max);

    ESP_LOGD(TAG, "set exposure 0x%" PRIx32, value_buf);
    /* 4 least significant bits of expsoure are fractional part */
    ret = sc2337p_write(dev->sccb_handle,
                        SC2337P_REG_SHUTTER_TIME_H,
                        SC2337P_FETCH_EXP_H(value_buf));
    ret |= sc2337p_write(dev->sccb_handle,
                         SC2337P_REG_SHUTTER_TIME_M,
                         SC2337P_FETCH_EXP_M(value_buf));
    ret |= sc2337p_write(dev->sccb_handle,
                         SC2337P_REG_SHUTTER_TIME_L,
                         SC2337P_FETCH_EXP_L(value_buf));
    if (ret == ESP_OK) {
        cam_sc2337p->sc2337p_para.exposure_val = value_buf;
    }
    return ret;
}

static esp_err_t sc2337p_set_total_gain_val(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    esp_err_t ret;
    struct sc2337p_cam *cam_sc2337p = (struct sc2337p_cam *)dev->priv;
    u32_val = MIN(u32_val, cam_sc2337p->sc2337p_para.limited_gain_index);
    // Limit gain index to valid range
    if (u32_val >= cam_sc2337p->sc2337p_para.limited_gain_index) {
        if (cam_sc2337p->sc2337p_para.limited_gain_index > 0) {
            u32_val = cam_sc2337p->sc2337p_para.limited_gain_index - 1;
        } else {
            u32_val = 0;  // Fallback to minimum gain if limit is 0
        }
    }

    ESP_LOGD(TAG, "dgain_fine %" PRIx8 ", dgain_coarse %" PRIx8 ", again_coarse %" PRIx8, sc2337p_gain_map[u32_val].dgain_fine, sc2337p_gain_map[u32_val].dgain_coarse, sc2337p_gain_map[u32_val].analog_gain);
    ret = sc2337p_write(dev->sccb_handle,
                        SC2337P_REG_DIG_FINE_GAIN,
                        sc2337p_gain_map[u32_val].dgain_fine);
    ret |= sc2337p_write(dev->sccb_handle,
                         SC2337P_REG_DIG_COARSE_GAIN,
                         sc2337p_gain_map[u32_val].dgain_coarse);
    ret |= sc2337p_write(dev->sccb_handle,
                         SC2337P_REG_ANG_GAIN,
                         sc2337p_gain_map[u32_val].analog_gain);
    if (ret == ESP_OK) {
        cam_sc2337p->sc2337p_para.gain_index = u32_val;
    }
    return ret;
}

static esp_err_t sc2337p_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    esp_err_t ret = ESP_OK;
    struct sc2337p_cam *cam_sc2337p = (struct sc2337p_cam *)dev->priv;
    switch (qdesc->id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = s_sc2337p_exp_min;
        qdesc->number.maximum = dev->cur_format->isp_info->isp_v1_info.vts - SC2337P_EXP_MAX_OFFSET; // max = VTS-6 = height+vblank-6, so when update vblank, exposure_max must be updated
        qdesc->number.step = 1;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.exp_def;
        break;
    case ESP_CAM_SENSOR_EXPOSURE_US:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = EXPOSURE_SC2337P_TO_V4L2(s_sc2337p_exp_min, dev->cur_format);
        qdesc->number.maximum = EXPOSURE_SC2337P_TO_V4L2((dev->cur_format->isp_info->isp_v1_info.vts - SC2337P_EXP_MAX_OFFSET), dev->cur_format); // max = VTS-6 = height+vblank-6, so when update vblank, exposure_max must be updated
        qdesc->number.step = MAX(EXPOSURE_SC2337P_TO_V4L2(0x01, dev->cur_format), 1);
        qdesc->default_value = EXPOSURE_SC2337P_TO_V4L2((dev->cur_format->isp_info->isp_v1_info.exp_def), dev->cur_format);
        break;
    case ESP_CAM_SENSOR_GAIN:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION;
        qdesc->enumeration.count = cam_sc2337p->sc2337p_para.limited_gain_index;
        qdesc->enumeration.elements = sc2337p_total_gain_val_map;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.gain_def; // gain index
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

static esp_err_t sc2337p_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    struct sc2337p_cam *cam_sc2337p = (struct sc2337p_cam *)dev->priv;
    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        *(uint32_t *)arg = cam_sc2337p->sc2337p_para.exposure_val;
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        *(uint32_t *)arg = cam_sc2337p->sc2337p_para.gain_index;
        break;
    }
    default: {
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    }
    return ret;
}

static esp_err_t sc2337p_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;

    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        uint32_t u32_val = *(uint32_t *)arg;
        ret = sc2337p_set_exp_val(dev, u32_val);
        break;
    }
    case ESP_CAM_SENSOR_EXPOSURE_US: {
        uint32_t u32_val = *(uint32_t *)arg;
        uint32_t ori_exp = EXPOSURE_V4L2_TO_SC2337P(u32_val, dev->cur_format);
        ret = sc2337p_set_exp_val(dev, ori_exp);
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        uint32_t u32_val = *(uint32_t *)arg;
        ret = sc2337p_set_total_gain_val(dev, u32_val);
        break;
    }
    case ESP_CAM_SENSOR_GROUP_EXP_GAIN: {
        esp_cam_sensor_gh_exp_gain_t *value = (esp_cam_sensor_gh_exp_gain_t *)arg;
        uint32_t ori_exp = 0;
        if (value->exposure_us != 0) {
            ori_exp = EXPOSURE_V4L2_TO_SC2337P(value->exposure_us, dev->cur_format);
        } else if (value->exposure_val != 0) {
            ori_exp = value->exposure_val;
        } else {
            ret = ESP_ERR_INVALID_ARG;
            break;
        }
        ret = sc2337p_set_exp_val(dev, ori_exp);
        ret |= sc2337p_set_total_gain_val(dev, value->gain_index);
        break;
    }
    case ESP_CAM_SENSOR_VFLIP: {
        int *value = (int *)arg;
        ret = sc2337p_set_vflip(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        int *value = (int *)arg;
        ret = sc2337p_set_mirror(dev, *value);
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

static esp_err_t sc2337p_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);
    formats->count = ARRAY_SIZE(sc2337p_format_info_mipi);
    formats->format_array = &sc2337p_format_info_mipi[0];

    return ESP_OK;
}

static esp_err_t sc2337p_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_raw = 1;
    return 0;
}

static esp_err_t sc2337p_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    struct sc2337p_cam *cam_sc2337p = (struct sc2337p_cam *)dev->priv;
    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        format = &sc2337p_format_info_mipi[get_sc2337p_mipi_actual_format_index()];
    }

    ret = sc2337p_write_array(dev->sccb_handle, (sc2337p_reginfo_t *)format->regs);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set format regs fail");
        return ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT;
    }

    dev->cur_format = format;
    // init para
    cam_sc2337p->sc2337p_para.exposure_val = dev->cur_format->isp_info->isp_v1_info.exp_def;
    cam_sc2337p->sc2337p_para.gain_index = dev->cur_format->isp_info->isp_v1_info.gain_def;
    cam_sc2337p->sc2337p_para.exposure_max = dev->cur_format->isp_info->isp_v1_info.vts - SC2337P_EXP_MAX_OFFSET;

    return ret;
}

static esp_err_t sc2337p_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
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

static esp_err_t sc2337p_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    SC2337P_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = sc2337p_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = sc2337p_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = sc2337p_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = sc2337p_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = sc2337p_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = sc2337p_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = sc2337p_get_sensor_id(dev, arg);
        break;
    default:
        break;
    }

    SC2337P_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t sc2337p_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        SC2337P_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
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

    if (dev->pwdn_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->pwdn_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        ret = gpio_config(&conf);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "gpio config failed");
        gpio_set_level(dev->pwdn_pin, 1);
        delay_ms(5);
    }

    return ret;
}

static esp_err_t sc2337p_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        SC2337P_DISABLE_OUT_XCLK(dev->xclk_pin);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_set_level(dev->pwdn_pin, 0);
        delay_ms(5);
    }

    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(5);
    }

    return ret;
}

static esp_err_t sc2337p_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del sc2337p (%p)", dev);
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

static const esp_cam_sensor_ops_t sc2337p_ops = {
    .query_para_desc = sc2337p_query_para_desc,
    .get_para_value = sc2337p_get_para_value,
    .set_para_value = sc2337p_set_para_value,
    .query_support_formats = sc2337p_query_support_formats,
    .query_support_capability = sc2337p_query_support_capability,
    .set_format = sc2337p_set_format,
    .get_format = sc2337p_get_format,
    .priv_ioctl = sc2337p_priv_ioctl,
    .del = sc2337p_delete
};

esp_cam_sensor_device_t *sc2337p_detect(esp_cam_sensor_config_t *config)
{
    esp_cam_sensor_device_t *dev = NULL;
    struct sc2337p_cam *cam_sc2337p;
    if (config == NULL) {
        return NULL;
    }

    dev = calloc(1, sizeof(esp_cam_sensor_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    cam_sc2337p = heap_caps_calloc(1, sizeof(struct sc2337p_cam), MALLOC_CAP_DEFAULT);
    if (!cam_sc2337p) {
        ESP_LOGE(TAG, "failed to calloc cam");
        free(dev);
        return NULL;
    }

    dev->name = (char *)SC2337P_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &sc2337p_ops;
    dev->priv = cam_sc2337p;

    cam_sc2337p->sc2337p_para.limited_gain_index = ARRAY_SIZE(sc2337p_total_gain_val_map);
    for (size_t i = 0; i < ARRAY_SIZE(sc2337p_total_gain_val_map); i++) {
        if (sc2337p_total_gain_val_map[i] > s_limited_gain) {
            cam_sc2337p->sc2337p_para.limited_gain_index = i;
            break;
        }
    }
    dev->cur_format = &sc2337p_format_info_mipi[get_sc2337p_mipi_actual_format_index()];

    // Configure sensor power, clock, and SCCB port
    if (sc2337p_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (sc2337p_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != SC2337P_PID1 && dev->id.pid != SC2337P_PID2) {
        ESP_LOGE(TAG, "Camera sensor is not SC2337P, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    sc2337p_power_off(dev);
    free(dev->priv);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_SC2337P_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(sc2337p_detect, ESP_CAM_SENSOR_MIPI_CSI, SC2337P_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return sc2337p_detect(config);
}
#endif
