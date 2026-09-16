/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/****************************************************
Copyright (C), 2026, MetaSilicon Tech. Co., Ltd.
All rights reserved.
****************************************************/

#include <string.h>
#include <sys/param.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"
#include "mit245_settings.h"
#include "mit245.h"

/*
 * MIT245 camera sensor gain control.
 * AnalogGain = analog_gain / 32 + 1.
 * The gain table keeps the same explicit layout as the MT212 template.
 */
typedef struct {
    uint8_t again_msb;
    uint8_t again_lsb;
} mit245_gain_t;

typedef struct {
    uint32_t exposure_val;
    uint32_t exposure_max;
    uint32_t gain_index; // current gain index
    size_t limited_gain_index; // max valid gain index (inclusive)

    uint32_t vflip_en : 1;
    uint32_t hmirror_en : 1;
} mit245_para_t;

struct mit245_cam {
    mit245_para_t mit245_para;
};

#define MIT245_IO_MUX_LOCK(mux)
#define MIT245_IO_MUX_UNLOCK(mux)
#define MIT245_ENABLE_OUT_XCLK(pin,clk)
#define MIT245_DISABLE_OUT_XCLK(pin)

#define EXPOSURE_V4L2_UNIT_US                   100
#define EXPOSURE_V4L2_TO_MIT245(v, sf)          \
    ((uint32_t)(((double)v) * EXPOSURE_V4L2_UNIT_US * 1000 / (((sf)->isp_info->isp_v1_info.tline_ns)) + 0.5))
#define EXPOSURE_MIT245_TO_V4L2(v, sf)          \
    ((int32_t)(((double)v) * (((sf)->isp_info->isp_v1_info.tline_ns)) / EXPOSURE_V4L2_UNIT_US / 1000 + 0.5))

#define MIT245_VTS_MAX          0xfffc
#define MIT245_EXP_MAX_OFFSET   0x03

#define MIT245_FETCH_EXP_H(val)   (((val) >> 8) & 0xFF)
#define MIT245_FETCH_EXP_L(val)   ((val) & 0xFF)

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))
#define MIT245_SUPPORT_NUM CONFIG_CAMERA_MIT245_MAX_SUPPORT

static const uint32_t s_limited_gain = CONFIG_CAMERA_MIT245_ABSOLUTE_GAIN_LIMIT;
static const uint8_t s_mit245_exp_min = 0x01;
static const char *TAG = "mit245";

#define MIT245_EXPOSURE_TEST_EN 0
#define MIT245_EXPOSURE_TEST_EN_GAIN 0

// Values are total gain multiplied by 1000.
static const uint32_t mit245_total_gain_val_map[] = {
    1000, 1031, 1063, 1094, 1125, 1156, 1188, 1219,
    1250, 1281, 1313, 1344, 1375, 1406, 1438, 1469,
    1500, 1531, 1563, 1594, 1625, 1656, 1688, 1719,
    1750, 1781, 1813, 1844, 1875, 1906, 1938, 1969,

    2000, 2031, 2063, 2094, 2125, 2156, 2188, 2219,
    2250, 2281, 2313, 2344, 2375, 2406, 2438, 2469,
    2500, 2531, 2563, 2594, 2625, 2656, 2688, 2719,
    2750, 2781, 2813, 2844, 2875, 2906, 2938, 2969,

    3000, 3031, 3063, 3094, 3125, 3156, 3188, 3219,
    3250, 3281, 3313, 3344, 3375, 3406, 3438, 3469,
    3500, 3531, 3563, 3594, 3625, 3656, 3688, 3719,
    3750, 3781, 3813, 3844, 3875, 3906, 3938, 3969,

    4000, 4031, 4063, 4094, 4125, 4156, 4188, 4219,
    4250, 4281, 4313, 4344, 4375, 4406, 4438, 4469,
    4500, 4531, 4563, 4594, 4625, 4656, 4688, 4719,
    4750, 4781, 4813, 4844, 4875, 4906, 4938, 4969,

    5000, 5031, 5063, 5094, 5125, 5156, 5188, 5219,
    5250, 5281, 5313, 5344, 5375, 5406, 5438, 5469,
    5500, 5531, 5563, 5594, 5625, 5656, 5688, 5719,
    5750, 5781, 5813, 5844, 5875, 5906, 5938, 5969,

    6000, 6031, 6063, 6094, 6125, 6156, 6188, 6219,
    6250, 6281, 6313, 6344, 6375, 6406, 6438, 6469,
    6500, 6531, 6563, 6594, 6625, 6656, 6688, 6719,
    6750, 6781, 6813, 6844, 6875, 6906, 6938, 6969,

    7000, 7031, 7063, 7094, 7125, 7156, 7188, 7219,
    7250, 7281, 7313, 7344, 7375, 7406, 7438, 7469,
    7500, 7531, 7563, 7594, 7625, 7656, 7688, 7719,
    7750, 7781, 7813, 7844, 7875, 7906, 7938, 7969,

    8000, 8031, 8063, 8094, 8125, 8156, 8188, 8219,
    8250, 8281, 8313, 8344, 8375, 8406, 8438, 8469,
    8500, 8531, 8563, 8594, 8625, 8656, 8688, 8719,
    8750, 8781, 8813, 8844, 8875, 8906, 8938, 8969,

    9000, 9031, 9063, 9094, 9125, 9156, 9188, 9219,
    9250, 9281, 9313, 9344, 9375, 9406, 9438, 9469,
    9500, 9531, 9563, 9594, 9625, 9656, 9688, 9719,
    9750, 9781, 9813, 9844, 9875, 9906, 9938, 9969,

    10000, 10031, 10063, 10094, 10125, 10156, 10188, 10219,
    10250, 10281, 10313, 10344, 10375, 10406, 10438, 10469,
    10500, 10531, 10563, 10594, 10625, 10656, 10688, 10719,
    10750, 10781, 10813, 10844, 10875, 10906, 10938, 10969,

    11000, 11031, 11063, 11094, 11125, 11156, 11188, 11219,
    11250, 11281, 11313, 11344, 11375, 11406, 11438, 11469,
    11500, 11531, 11563, 11594, 11625, 11656, 11688, 11719,
    11750, 11781, 11813, 11844, 11875, 11906, 11938, 11969,

    12000, 12031, 12063, 12094, 12125, 12156, 12188, 12219,
    12250, 12281, 12313, 12344, 12375, 12406, 12438, 12469,
    12500, 12531, 12563, 12594, 12625, 12656, 12688, 12719,
    12750, 12781, 12813, 12844, 12875, 12906, 12938, 12969,

    13000, 13031, 13063, 13094, 13125, 13156, 13188, 13219,
    13250, 13281, 13313, 13344, 13375, 13406, 13438, 13469,
    13500, 13531, 13563, 13594, 13625, 13656, 13688, 13719,
    13750, 13781, 13813, 13844, 13875, 13906, 13938, 13969,

    14000, 14031, 14063, 14094, 14125, 14156, 14188, 14219,
    14250, 14281, 14313, 14344, 14375, 14406, 14438, 14469,
    14500, 14531, 14563, 14594, 14625, 14656, 14688, 14719,
    14750, 14781, 14813, 14844, 14875, 14906, 14938, 14969,

    15000, 15031, 15063, 15094, 15125, 15156, 15188, 15219,
    15250, 15281, 15313, 15344, 15375, 15406, 15438, 15469,
    15500, 15531, 15563, 15594, 15625, 15656, 15688, 15719,
    15750, 15781, 15813, 15844, 15875, 15906, 15938, 15969,

    16000, 16031, 16063, 16094, 16125, 16156, 16188, 16219,
    16250, 16281, 16313, 16344, 16375, 16406, 16438, 16469,
    16500, 16531, 16563, 16594, 16625, 16656, 16688, 16719,
    16750, 16781, 16813, 16844, 16875, 16906, 16938, 16969,

    17000, 17031, 17063, 17094, 17125, 17156, 17188, 17219,
    17250, 17281, 17313, 17344, 17375, 17406, 17438, 17469,
    17500, 17531, 17563, 17594, 17625, 17656, 17688, 17719,
    17750, 17781, 17813, 17844, 17875, 17906, 17938, 17969,

    18000, 18031, 18063, 18094, 18125, 18156, 18188, 18219,
    18250, 18281, 18313, 18344, 18375, 18406, 18438, 18469,
    18500, 18531, 18563, 18594, 18625, 18656, 18688, 18719,
    18750, 18781, 18813, 18844, 18875, 18906, 18938, 18969,

    19000, 19031, 19063, 19094, 19125, 19156, 19188, 19219,
    19250, 19281, 19313, 19344, 19375, 19406, 19438, 19469,
    19500, 19531, 19563, 19594, 19625, 19656, 19688, 19719,
    19750, 19781, 19813, 19844, 19875, 19906, 19938, 19969,

    20000, 20031, 20063, 20094, 20125, 20156, 20188, 20219,
    20250, 20281, 20313, 20344, 20375, 20406, 20438, 20469,
    20500, 20531, 20563, 20594, 20625, 20656, 20688, 20719,
    20750, 20781, 20813, 20844, 20875, 20906, 20938, 20969,

    21000, 21031, 21063, 21094, 21125, 21156, 21188, 21219,
    21250, 21281, 21313, 21344, 21375, 21406, 21438, 21469,
    21500, 21531, 21563, 21594, 21625, 21656, 21688, 21719,
    21750, 21781, 21813, 21844, 21875, 21906, 21938, 21969,

    22000, 22031, 22063, 22094, 22125, 22156, 22188, 22219,
    22250, 22281, 22313, 22344, 22375, 22406, 22438, 22469,
    22500, 22531, 22563, 22594, 22625, 22656, 22688, 22719,
    22750, 22781, 22813, 22844, 22875, 22906, 22938, 22969,

    23000, 23031, 23063, 23094, 23125, 23156, 23188, 23219,
    23250, 23281, 23313, 23344, 23375, 23406, 23438, 23469,
    23500, 23531, 23563, 23594, 23625, 23656, 23688, 23719,
    23750, 23781, 23813, 23844, 23875, 23906, 23938, 23969,

    24000, 24031, 24063, 24094, 24125, 24156, 24188, 24219,
    24250, 24281, 24313, 24344, 24375, 24406, 24438, 24469,
    24500, 24531, 24563, 24594, 24625, 24656, 24688, 24719,
    24750, 24781, 24813, 24844, 24875, 24906, 24938, 24969,

    25000, 25031, 25063, 25094, 25125, 25156, 25188, 25219,
    25250, 25281, 25313, 25344, 25375, 25406, 25438, 25469,
    25500, 25531, 25563, 25594, 25625, 25656, 25688, 25719,
    25750, 25781, 25813, 25844, 25875, 25906, 25938, 25969,

    26000,
};

// MIT245 gain map: [AGain_MSB, AGain_LSB]
static const mit245_gain_t mit245_gain_map[] = {
    {0x00, 0x00}, {0x00, 0x01}, {0x00, 0x02}, {0x00, 0x03},
    {0x00, 0x04}, {0x00, 0x05}, {0x00, 0x06}, {0x00, 0x07},
    {0x00, 0x08}, {0x00, 0x09}, {0x00, 0x0a}, {0x00, 0x0b},
    {0x00, 0x0c}, {0x00, 0x0d}, {0x00, 0x0e}, {0x00, 0x0f},
    {0x00, 0x10}, {0x00, 0x11}, {0x00, 0x12}, {0x00, 0x13},
    {0x00, 0x14}, {0x00, 0x15}, {0x00, 0x16}, {0x00, 0x17},
    {0x00, 0x18}, {0x00, 0x19}, {0x00, 0x1a}, {0x00, 0x1b},
    {0x00, 0x1c}, {0x00, 0x1d}, {0x00, 0x1e}, {0x00, 0x1f},

    {0x00, 0x20}, {0x00, 0x21}, {0x00, 0x22}, {0x00, 0x23},
    {0x00, 0x24}, {0x00, 0x25}, {0x00, 0x26}, {0x00, 0x27},
    {0x00, 0x28}, {0x00, 0x29}, {0x00, 0x2a}, {0x00, 0x2b},
    {0x00, 0x2c}, {0x00, 0x2d}, {0x00, 0x2e}, {0x00, 0x2f},
    {0x00, 0x30}, {0x00, 0x31}, {0x00, 0x32}, {0x00, 0x33},
    {0x00, 0x34}, {0x00, 0x35}, {0x00, 0x36}, {0x00, 0x37},
    {0x00, 0x38}, {0x00, 0x39}, {0x00, 0x3a}, {0x00, 0x3b},
    {0x00, 0x3c}, {0x00, 0x3d}, {0x00, 0x3e}, {0x00, 0x3f},

    {0x00, 0x40}, {0x00, 0x41}, {0x00, 0x42}, {0x00, 0x43},
    {0x00, 0x44}, {0x00, 0x45}, {0x00, 0x46}, {0x00, 0x47},
    {0x00, 0x48}, {0x00, 0x49}, {0x00, 0x4a}, {0x00, 0x4b},
    {0x00, 0x4c}, {0x00, 0x4d}, {0x00, 0x4e}, {0x00, 0x4f},
    {0x00, 0x50}, {0x00, 0x51}, {0x00, 0x52}, {0x00, 0x53},
    {0x00, 0x54}, {0x00, 0x55}, {0x00, 0x56}, {0x00, 0x57},
    {0x00, 0x58}, {0x00, 0x59}, {0x00, 0x5a}, {0x00, 0x5b},
    {0x00, 0x5c}, {0x00, 0x5d}, {0x00, 0x5e}, {0x00, 0x5f},

    {0x00, 0x60}, {0x00, 0x61}, {0x00, 0x62}, {0x00, 0x63},
    {0x00, 0x64}, {0x00, 0x65}, {0x00, 0x66}, {0x00, 0x67},
    {0x00, 0x68}, {0x00, 0x69}, {0x00, 0x6a}, {0x00, 0x6b},
    {0x00, 0x6c}, {0x00, 0x6d}, {0x00, 0x6e}, {0x00, 0x6f},
    {0x00, 0x70}, {0x00, 0x71}, {0x00, 0x72}, {0x00, 0x73},
    {0x00, 0x74}, {0x00, 0x75}, {0x00, 0x76}, {0x00, 0x77},
    {0x00, 0x78}, {0x00, 0x79}, {0x00, 0x7a}, {0x00, 0x7b},
    {0x00, 0x7c}, {0x00, 0x7d}, {0x00, 0x7e}, {0x00, 0x7f},

    {0x00, 0x80}, {0x00, 0x81}, {0x00, 0x82}, {0x00, 0x83},
    {0x00, 0x84}, {0x00, 0x85}, {0x00, 0x86}, {0x00, 0x87},
    {0x00, 0x88}, {0x00, 0x89}, {0x00, 0x8a}, {0x00, 0x8b},
    {0x00, 0x8c}, {0x00, 0x8d}, {0x00, 0x8e}, {0x00, 0x8f},
    {0x00, 0x90}, {0x00, 0x91}, {0x00, 0x92}, {0x00, 0x93},
    {0x00, 0x94}, {0x00, 0x95}, {0x00, 0x96}, {0x00, 0x97},
    {0x00, 0x98}, {0x00, 0x99}, {0x00, 0x9a}, {0x00, 0x9b},
    {0x00, 0x9c}, {0x00, 0x9d}, {0x00, 0x9e}, {0x00, 0x9f},

    {0x00, 0xa0}, {0x00, 0xa1}, {0x00, 0xa2}, {0x00, 0xa3},
    {0x00, 0xa4}, {0x00, 0xa5}, {0x00, 0xa6}, {0x00, 0xa7},
    {0x00, 0xa8}, {0x00, 0xa9}, {0x00, 0xaa}, {0x00, 0xab},
    {0x00, 0xac}, {0x00, 0xad}, {0x00, 0xae}, {0x00, 0xaf},
    {0x00, 0xb0}, {0x00, 0xb1}, {0x00, 0xb2}, {0x00, 0xb3},
    {0x00, 0xb4}, {0x00, 0xb5}, {0x00, 0xb6}, {0x00, 0xb7},
    {0x00, 0xb8}, {0x00, 0xb9}, {0x00, 0xba}, {0x00, 0xbb},
    {0x00, 0xbc}, {0x00, 0xbd}, {0x00, 0xbe}, {0x00, 0xbf},

    {0x00, 0xc0}, {0x00, 0xc1}, {0x00, 0xc2}, {0x00, 0xc3},
    {0x00, 0xc4}, {0x00, 0xc5}, {0x00, 0xc6}, {0x00, 0xc7},
    {0x00, 0xc8}, {0x00, 0xc9}, {0x00, 0xca}, {0x00, 0xcb},
    {0x00, 0xcc}, {0x00, 0xcd}, {0x00, 0xce}, {0x00, 0xcf},
    {0x00, 0xd0}, {0x00, 0xd1}, {0x00, 0xd2}, {0x00, 0xd3},
    {0x00, 0xd4}, {0x00, 0xd5}, {0x00, 0xd6}, {0x00, 0xd7},
    {0x00, 0xd8}, {0x00, 0xd9}, {0x00, 0xda}, {0x00, 0xdb},
    {0x00, 0xdc}, {0x00, 0xdd}, {0x00, 0xde}, {0x00, 0xdf},

    {0x00, 0xe0}, {0x00, 0xe1}, {0x00, 0xe2}, {0x00, 0xe3},
    {0x00, 0xe4}, {0x00, 0xe5}, {0x00, 0xe6}, {0x00, 0xe7},
    {0x00, 0xe8}, {0x00, 0xe9}, {0x00, 0xea}, {0x00, 0xeb},
    {0x00, 0xec}, {0x00, 0xed}, {0x00, 0xee}, {0x00, 0xef},
    {0x00, 0xf0}, {0x00, 0xf1}, {0x00, 0xf2}, {0x00, 0xf3},
    {0x00, 0xf4}, {0x00, 0xf5}, {0x00, 0xf6}, {0x00, 0xf7},
    {0x00, 0xf8}, {0x00, 0xf9}, {0x00, 0xfa}, {0x00, 0xfb},
    {0x00, 0xfc}, {0x00, 0xfd}, {0x00, 0xfe}, {0x00, 0xff},

    {0x01, 0x00}, {0x01, 0x01}, {0x01, 0x02}, {0x01, 0x03},
    {0x01, 0x04}, {0x01, 0x05}, {0x01, 0x06}, {0x01, 0x07},
    {0x01, 0x08}, {0x01, 0x09}, {0x01, 0x0a}, {0x01, 0x0b},
    {0x01, 0x0c}, {0x01, 0x0d}, {0x01, 0x0e}, {0x01, 0x0f},
    {0x01, 0x10}, {0x01, 0x11}, {0x01, 0x12}, {0x01, 0x13},
    {0x01, 0x14}, {0x01, 0x15}, {0x01, 0x16}, {0x01, 0x17},
    {0x01, 0x18}, {0x01, 0x19}, {0x01, 0x1a}, {0x01, 0x1b},
    {0x01, 0x1c}, {0x01, 0x1d}, {0x01, 0x1e}, {0x01, 0x1f},

    {0x01, 0x20}, {0x01, 0x21}, {0x01, 0x22}, {0x01, 0x23},
    {0x01, 0x24}, {0x01, 0x25}, {0x01, 0x26}, {0x01, 0x27},
    {0x01, 0x28}, {0x01, 0x29}, {0x01, 0x2a}, {0x01, 0x2b},
    {0x01, 0x2c}, {0x01, 0x2d}, {0x01, 0x2e}, {0x01, 0x2f},
    {0x01, 0x30}, {0x01, 0x31}, {0x01, 0x32}, {0x01, 0x33},
    {0x01, 0x34}, {0x01, 0x35}, {0x01, 0x36}, {0x01, 0x37},
    {0x01, 0x38}, {0x01, 0x39}, {0x01, 0x3a}, {0x01, 0x3b},
    {0x01, 0x3c}, {0x01, 0x3d}, {0x01, 0x3e}, {0x01, 0x3f},

    {0x01, 0x40}, {0x01, 0x41}, {0x01, 0x42}, {0x01, 0x43},
    {0x01, 0x44}, {0x01, 0x45}, {0x01, 0x46}, {0x01, 0x47},
    {0x01, 0x48}, {0x01, 0x49}, {0x01, 0x4a}, {0x01, 0x4b},
    {0x01, 0x4c}, {0x01, 0x4d}, {0x01, 0x4e}, {0x01, 0x4f},
    {0x01, 0x50}, {0x01, 0x51}, {0x01, 0x52}, {0x01, 0x53},
    {0x01, 0x54}, {0x01, 0x55}, {0x01, 0x56}, {0x01, 0x57},
    {0x01, 0x58}, {0x01, 0x59}, {0x01, 0x5a}, {0x01, 0x5b},
    {0x01, 0x5c}, {0x01, 0x5d}, {0x01, 0x5e}, {0x01, 0x5f},

    {0x01, 0x60}, {0x01, 0x61}, {0x01, 0x62}, {0x01, 0x63},
    {0x01, 0x64}, {0x01, 0x65}, {0x01, 0x66}, {0x01, 0x67},
    {0x01, 0x68}, {0x01, 0x69}, {0x01, 0x6a}, {0x01, 0x6b},
    {0x01, 0x6c}, {0x01, 0x6d}, {0x01, 0x6e}, {0x01, 0x6f},
    {0x01, 0x70}, {0x01, 0x71}, {0x01, 0x72}, {0x01, 0x73},
    {0x01, 0x74}, {0x01, 0x75}, {0x01, 0x76}, {0x01, 0x77},
    {0x01, 0x78}, {0x01, 0x79}, {0x01, 0x7a}, {0x01, 0x7b},
    {0x01, 0x7c}, {0x01, 0x7d}, {0x01, 0x7e}, {0x01, 0x7f},

    {0x01, 0x80}, {0x01, 0x81}, {0x01, 0x82}, {0x01, 0x83},
    {0x01, 0x84}, {0x01, 0x85}, {0x01, 0x86}, {0x01, 0x87},
    {0x01, 0x88}, {0x01, 0x89}, {0x01, 0x8a}, {0x01, 0x8b},
    {0x01, 0x8c}, {0x01, 0x8d}, {0x01, 0x8e}, {0x01, 0x8f},
    {0x01, 0x90}, {0x01, 0x91}, {0x01, 0x92}, {0x01, 0x93},
    {0x01, 0x94}, {0x01, 0x95}, {0x01, 0x96}, {0x01, 0x97},
    {0x01, 0x98}, {0x01, 0x99}, {0x01, 0x9a}, {0x01, 0x9b},
    {0x01, 0x9c}, {0x01, 0x9d}, {0x01, 0x9e}, {0x01, 0x9f},

    {0x01, 0xa0}, {0x01, 0xa1}, {0x01, 0xa2}, {0x01, 0xa3},
    {0x01, 0xa4}, {0x01, 0xa5}, {0x01, 0xa6}, {0x01, 0xa7},
    {0x01, 0xa8}, {0x01, 0xa9}, {0x01, 0xaa}, {0x01, 0xab},
    {0x01, 0xac}, {0x01, 0xad}, {0x01, 0xae}, {0x01, 0xaf},
    {0x01, 0xb0}, {0x01, 0xb1}, {0x01, 0xb2}, {0x01, 0xb3},
    {0x01, 0xb4}, {0x01, 0xb5}, {0x01, 0xb6}, {0x01, 0xb7},
    {0x01, 0xb8}, {0x01, 0xb9}, {0x01, 0xba}, {0x01, 0xbb},
    {0x01, 0xbc}, {0x01, 0xbd}, {0x01, 0xbe}, {0x01, 0xbf},

    {0x01, 0xc0}, {0x01, 0xc1}, {0x01, 0xc2}, {0x01, 0xc3},
    {0x01, 0xc4}, {0x01, 0xc5}, {0x01, 0xc6}, {0x01, 0xc7},
    {0x01, 0xc8}, {0x01, 0xc9}, {0x01, 0xca}, {0x01, 0xcb},
    {0x01, 0xcc}, {0x01, 0xcd}, {0x01, 0xce}, {0x01, 0xcf},
    {0x01, 0xd0}, {0x01, 0xd1}, {0x01, 0xd2}, {0x01, 0xd3},
    {0x01, 0xd4}, {0x01, 0xd5}, {0x01, 0xd6}, {0x01, 0xd7},
    {0x01, 0xd8}, {0x01, 0xd9}, {0x01, 0xda}, {0x01, 0xdb},
    {0x01, 0xdc}, {0x01, 0xdd}, {0x01, 0xde}, {0x01, 0xdf},

    {0x01, 0xe0}, {0x01, 0xe1}, {0x01, 0xe2}, {0x01, 0xe3},
    {0x01, 0xe4}, {0x01, 0xe5}, {0x01, 0xe6}, {0x01, 0xe7},
    {0x01, 0xe8}, {0x01, 0xe9}, {0x01, 0xea}, {0x01, 0xeb},
    {0x01, 0xec}, {0x01, 0xed}, {0x01, 0xee}, {0x01, 0xef},
    {0x01, 0xf0}, {0x01, 0xf1}, {0x01, 0xf2}, {0x01, 0xf3},
    {0x01, 0xf4}, {0x01, 0xf5}, {0x01, 0xf6}, {0x01, 0xf7},
    {0x01, 0xf8}, {0x01, 0xf9}, {0x01, 0xfa}, {0x01, 0xfb},
    {0x01, 0xfc}, {0x01, 0xfd}, {0x01, 0xfe}, {0x01, 0xff},

    {0x02, 0x00}, {0x02, 0x01}, {0x02, 0x02}, {0x02, 0x03},
    {0x02, 0x04}, {0x02, 0x05}, {0x02, 0x06}, {0x02, 0x07},
    {0x02, 0x08}, {0x02, 0x09}, {0x02, 0x0a}, {0x02, 0x0b},
    {0x02, 0x0c}, {0x02, 0x0d}, {0x02, 0x0e}, {0x02, 0x0f},
    {0x02, 0x10}, {0x02, 0x11}, {0x02, 0x12}, {0x02, 0x13},
    {0x02, 0x14}, {0x02, 0x15}, {0x02, 0x16}, {0x02, 0x17},
    {0x02, 0x18}, {0x02, 0x19}, {0x02, 0x1a}, {0x02, 0x1b},
    {0x02, 0x1c}, {0x02, 0x1d}, {0x02, 0x1e}, {0x02, 0x1f},

    {0x02, 0x20}, {0x02, 0x21}, {0x02, 0x22}, {0x02, 0x23},
    {0x02, 0x24}, {0x02, 0x25}, {0x02, 0x26}, {0x02, 0x27},
    {0x02, 0x28}, {0x02, 0x29}, {0x02, 0x2a}, {0x02, 0x2b},
    {0x02, 0x2c}, {0x02, 0x2d}, {0x02, 0x2e}, {0x02, 0x2f},
    {0x02, 0x30}, {0x02, 0x31}, {0x02, 0x32}, {0x02, 0x33},
    {0x02, 0x34}, {0x02, 0x35}, {0x02, 0x36}, {0x02, 0x37},
    {0x02, 0x38}, {0x02, 0x39}, {0x02, 0x3a}, {0x02, 0x3b},
    {0x02, 0x3c}, {0x02, 0x3d}, {0x02, 0x3e}, {0x02, 0x3f},

    {0x02, 0x40}, {0x02, 0x41}, {0x02, 0x42}, {0x02, 0x43},
    {0x02, 0x44}, {0x02, 0x45}, {0x02, 0x46}, {0x02, 0x47},
    {0x02, 0x48}, {0x02, 0x49}, {0x02, 0x4a}, {0x02, 0x4b},
    {0x02, 0x4c}, {0x02, 0x4d}, {0x02, 0x4e}, {0x02, 0x4f},
    {0x02, 0x50}, {0x02, 0x51}, {0x02, 0x52}, {0x02, 0x53},
    {0x02, 0x54}, {0x02, 0x55}, {0x02, 0x56}, {0x02, 0x57},
    {0x02, 0x58}, {0x02, 0x59}, {0x02, 0x5a}, {0x02, 0x5b},
    {0x02, 0x5c}, {0x02, 0x5d}, {0x02, 0x5e}, {0x02, 0x5f},

    {0x02, 0x60}, {0x02, 0x61}, {0x02, 0x62}, {0x02, 0x63},
    {0x02, 0x64}, {0x02, 0x65}, {0x02, 0x66}, {0x02, 0x67},
    {0x02, 0x68}, {0x02, 0x69}, {0x02, 0x6a}, {0x02, 0x6b},
    {0x02, 0x6c}, {0x02, 0x6d}, {0x02, 0x6e}, {0x02, 0x6f},
    {0x02, 0x70}, {0x02, 0x71}, {0x02, 0x72}, {0x02, 0x73},
    {0x02, 0x74}, {0x02, 0x75}, {0x02, 0x76}, {0x02, 0x77},
    {0x02, 0x78}, {0x02, 0x79}, {0x02, 0x7a}, {0x02, 0x7b},
    {0x02, 0x7c}, {0x02, 0x7d}, {0x02, 0x7e}, {0x02, 0x7f},

    {0x02, 0x80}, {0x02, 0x81}, {0x02, 0x82}, {0x02, 0x83},
    {0x02, 0x84}, {0x02, 0x85}, {0x02, 0x86}, {0x02, 0x87},
    {0x02, 0x88}, {0x02, 0x89}, {0x02, 0x8a}, {0x02, 0x8b},
    {0x02, 0x8c}, {0x02, 0x8d}, {0x02, 0x8e}, {0x02, 0x8f},
    {0x02, 0x90}, {0x02, 0x91}, {0x02, 0x92}, {0x02, 0x93},
    {0x02, 0x94}, {0x02, 0x95}, {0x02, 0x96}, {0x02, 0x97},
    {0x02, 0x98}, {0x02, 0x99}, {0x02, 0x9a}, {0x02, 0x9b},
    {0x02, 0x9c}, {0x02, 0x9d}, {0x02, 0x9e}, {0x02, 0x9f},

    {0x02, 0xa0}, {0x02, 0xa1}, {0x02, 0xa2}, {0x02, 0xa3},
    {0x02, 0xa4}, {0x02, 0xa5}, {0x02, 0xa6}, {0x02, 0xa7},
    {0x02, 0xa8}, {0x02, 0xa9}, {0x02, 0xaa}, {0x02, 0xab},
    {0x02, 0xac}, {0x02, 0xad}, {0x02, 0xae}, {0x02, 0xaf},
    {0x02, 0xb0}, {0x02, 0xb1}, {0x02, 0xb2}, {0x02, 0xb3},
    {0x02, 0xb4}, {0x02, 0xb5}, {0x02, 0xb6}, {0x02, 0xb7},
    {0x02, 0xb8}, {0x02, 0xb9}, {0x02, 0xba}, {0x02, 0xbb},
    {0x02, 0xbc}, {0x02, 0xbd}, {0x02, 0xbe}, {0x02, 0xbf},

    {0x02, 0xc0}, {0x02, 0xc1}, {0x02, 0xc2}, {0x02, 0xc3},
    {0x02, 0xc4}, {0x02, 0xc5}, {0x02, 0xc6}, {0x02, 0xc7},
    {0x02, 0xc8}, {0x02, 0xc9}, {0x02, 0xca}, {0x02, 0xcb},
    {0x02, 0xcc}, {0x02, 0xcd}, {0x02, 0xce}, {0x02, 0xcf},
    {0x02, 0xd0}, {0x02, 0xd1}, {0x02, 0xd2}, {0x02, 0xd3},
    {0x02, 0xd4}, {0x02, 0xd5}, {0x02, 0xd6}, {0x02, 0xd7},
    {0x02, 0xd8}, {0x02, 0xd9}, {0x02, 0xda}, {0x02, 0xdb},
    {0x02, 0xdc}, {0x02, 0xdd}, {0x02, 0xde}, {0x02, 0xdf},

    {0x02, 0xe0}, {0x02, 0xe1}, {0x02, 0xe2}, {0x02, 0xe3},
    {0x02, 0xe4}, {0x02, 0xe5}, {0x02, 0xe6}, {0x02, 0xe7},
    {0x02, 0xe8}, {0x02, 0xe9}, {0x02, 0xea}, {0x02, 0xeb},
    {0x02, 0xec}, {0x02, 0xed}, {0x02, 0xee}, {0x02, 0xef},
    {0x02, 0xf0}, {0x02, 0xf1}, {0x02, 0xf2}, {0x02, 0xf3},
    {0x02, 0xf4}, {0x02, 0xf5}, {0x02, 0xf6}, {0x02, 0xf7},
    {0x02, 0xf8}, {0x02, 0xf9}, {0x02, 0xfa}, {0x02, 0xfb},
    {0x02, 0xfc}, {0x02, 0xfd}, {0x02, 0xfe}, {0x02, 0xff},

    {0x03, 0x00}, {0x03, 0x01}, {0x03, 0x02}, {0x03, 0x03},
    {0x03, 0x04}, {0x03, 0x05}, {0x03, 0x06}, {0x03, 0x07},
    {0x03, 0x08}, {0x03, 0x09}, {0x03, 0x0a}, {0x03, 0x0b},
    {0x03, 0x0c}, {0x03, 0x0d}, {0x03, 0x0e}, {0x03, 0x0f},
    {0x03, 0x10}, {0x03, 0x11}, {0x03, 0x12}, {0x03, 0x13},
    {0x03, 0x14}, {0x03, 0x15}, {0x03, 0x16}, {0x03, 0x17},
    {0x03, 0x18}, {0x03, 0x19}, {0x03, 0x1a}, {0x03, 0x1b},
    {0x03, 0x1c}, {0x03, 0x1d}, {0x03, 0x1e}, {0x03, 0x1f},

    {0x03, 0x20}
};

static const esp_cam_sensor_isp_info_t mit245_isp_info_mipi[] = {
    /* index 0: RAW8 1920x1080 30fps */
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 79200000,
            .vts = 1108,
            .hts = 2383,
            .tline_ns = 30083,
            .gain_def = 0,
            .exp_def = 0x014c,
            .bayer_type = ESP_CAM_SENSOR_BAYER_RGGB,
        }
    },
    /* index 1: RAW10 1920x1080 30fps */
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 79200000,
            .vts = 1108,
            .hts = 2383,
            .tline_ns = 30083,
            .gain_def = 0,
            .exp_def = 0x014c,
            .bayer_type = ESP_CAM_SENSOR_BAYER_RGGB,
        }
    }
};

static const esp_cam_sensor_format_t mit245_format_info_mipi[] = {
    /* For MIPI */
#if CONFIG_CAMERA_MIT245_MIPI_RAW8_1920X1080_30FPS
    {
        .name = "MIPI_2lane_24Minput_RAW8_1920x1080_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1920,
        .height = 1080,
        .regs = mit245_mipi_2lane_24Minput_1920x1080_raw8_30fps,
        .regs_size = ARRAY_SIZE(mit245_mipi_2lane_24Minput_1920x1080_raw8_30fps),
        .fps = 30,
        .isp_info = &mit245_isp_info_mipi[0],
        .mipi_info = {
            .mipi_clk = 396000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
#endif
#if CONFIG_CAMERA_MIT245_MIPI_RAW10_1920X1080_30FPS
    {
        .name = "MIPI_2lane_24Minput_RAW10_1920x1080_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1920,
        .height = 1080,
        .regs = mit245_mipi_2lane_24Minput_1920x1080_raw10_30fps,
        .regs_size = ARRAY_SIZE(mit245_mipi_2lane_24Minput_1920x1080_raw10_30fps),
        .fps = 30,
        .isp_info = &mit245_isp_info_mipi[1],
        .mipi_info = {
            .mipi_clk = 396000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
#endif
};

#ifndef CONFIG_CAMERA_MIT245_MIPI_IF_FORMAT_INDEX_DEFAULT
#error "Please choose at least one format in menuconfig for MIT245"
#endif

static const int mit245_mipi_format_index[] = {
#if CONFIG_CAMERA_MIT245_MIPI_RAW8_1920X1080_30FPS
    0,
#endif
#if CONFIG_CAMERA_MIT245_MIPI_RAW10_1920X1080_30FPS
    1,
#endif
};

static int get_mit245_mipi_actual_format_index(void)
{
    int default_index = CONFIG_CAMERA_MIT245_MIPI_IF_FORMAT_INDEX_DEFAULT;
    for (size_t i = 0; i < ARRAY_SIZE(mit245_mipi_format_index); i++) {
        if (mit245_mipi_format_index[i] == default_index) {
            return i;
        }
    }
    return 0;
}

static esp_err_t mit245_read(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a16v8(sccb_handle, reg, read_buf);
}

static esp_err_t mit245_write(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t data)
{
    return esp_sccb_transmit_reg_a16v8(sccb_handle, reg, data);
}

/* write a array of registers  */
static esp_err_t mit245_write_array(esp_sccb_io_handle_t sccb_handle, mit245_reginfo_t *regarray)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && regarray[i].reg != MIT245_REG_END) {
        if (regarray[i].reg != MIT245_REG_DELAY) {
            ret = mit245_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    return ret;
}

static esp_err_t mit245_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_data = 0;

    ret = mit245_read(sccb_handle, reg, &reg_data);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (reg_data & ~mask) | ((value << offset) & mask);
    ret = mit245_write(sccb_handle, reg, value);
    return ret;
}

static esp_err_t mit245_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    return mit245_write(dev->sccb_handle, 0x0446, enable ? 0x01 : 0x00);
}

static esp_err_t mit245_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return ESP_OK;
}

static esp_err_t mit245_soft_reset(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = mit245_set_reg_bits(dev->sccb_handle, 0x0103, 0, 1, 0x01);
    delay_ms(5);
    return ret;
}

static esp_err_t mit245_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t pid_h, pid_l;

    ret = mit245_read(dev->sccb_handle, MIT245_REG_SENSOR_ID_H, &pid_h);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = mit245_read(dev->sccb_handle, MIT245_REG_SENSOR_ID_L, &pid_l);
    if (ret != ESP_OK) {
        return ret;
    }
    id->pid = (pid_h << 8) | pid_l;

    return ret;
}

static esp_err_t mit245_set_mirror(esp_cam_sensor_device_t *dev, int enable)
{
    return mit245_set_reg_bits(dev->sccb_handle, 0x0101, 0, 1,  enable ? 0x01 : 0x00);
}

static esp_err_t mit245_set_vflip(esp_cam_sensor_device_t *dev, int enable)
{
    return mit245_set_reg_bits(dev->sccb_handle, 0x0101, 1, 1, enable ? 0x01 : 0x00);
}

static esp_err_t mit245_set_exp_val(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    esp_err_t ret = ESP_OK;
    struct mit245_cam *cam_mit245 = (struct mit245_cam *)dev->priv;
    uint32_t value_buf = MAX(u32_val, s_mit245_exp_min);
    value_buf = MIN(value_buf, cam_mit245->mit245_para.exposure_max);

    ESP_LOGD(TAG, "set exposure 0x%" PRIx32, value_buf);
    ret |= mit245_write(dev->sccb_handle, MIT245_REG_GROUP_HOLD, 0x01);
    ret |= mit245_write(dev->sccb_handle,
                        MIT245_REG_SHUTTER_TIME_H,
                        MIT245_FETCH_EXP_H(value_buf));
    ret |= mit245_write(dev->sccb_handle,
                        MIT245_REG_SHUTTER_TIME_L,
                        MIT245_FETCH_EXP_L(value_buf));
    ret |= mit245_write(dev->sccb_handle, MIT245_REG_GROUP_HOLD, 0x00);
    if (ret == ESP_OK) {
        cam_mit245->mit245_para.exposure_val = value_buf;
    }
    return ret;
}

static esp_err_t mit245_set_total_gain_val(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    esp_err_t ret = ESP_OK;
    struct mit245_cam *cam_mit245 = (struct mit245_cam *)dev->priv;
    const mit245_gain_t *gain;

    if (u32_val > cam_mit245->mit245_para.limited_gain_index) {
        u32_val = cam_mit245->mit245_para.limited_gain_index;
    }
    gain = &mit245_gain_map[u32_val];

    ESP_LOGD(TAG, "again_h %" PRIx8 ", again_l %" PRIx8,
             gain->again_msb, gain->again_lsb);
    ret |= mit245_write(dev->sccb_handle, MIT245_REG_GROUP_HOLD, 0x01);
    ret |= mit245_write(dev->sccb_handle, MIT245_REG_ANG_GAIN_H, gain->again_msb);
    ret |= mit245_write(dev->sccb_handle, MIT245_REG_ANG_GAIN_L, gain->again_lsb);
    ret |= mit245_write(dev->sccb_handle, MIT245_REG_GROUP_HOLD, 0x00);
    if (ret == ESP_OK) {
        cam_mit245->mit245_para.gain_index = u32_val;
    }
    return ret;
}

#if MIT245_EXPOSURE_TEST_EN
static volatile uint32_t s_exp_v = 3;
static bool s_exp_add = true;
static TimerHandle_t ae_timer_handle = NULL;

static void ae_timer_callback(TimerHandle_t timer)
{
    esp_cam_sensor_device_t *dev = (esp_cam_sensor_device_t *)pvTimerGetTimerID(timer);
    if (dev == NULL) {
        return;
    }
    struct mit245_cam *cam_mit245 = (struct mit245_cam *)dev->priv;
#if MIT245_EXPOSURE_TEST_EN_GAIN
    uint32_t min_gain_index = 0;
    uint32_t max_gain_index = cam_mit245->mit245_para.limited_gain_index;

    if (s_exp_add) {
        if (s_exp_v >= max_gain_index) {
            s_exp_add = false;
            if (s_exp_v > max_gain_index) {
                s_exp_v = max_gain_index;
            }
        } else {
            s_exp_v += 1;
        }
    } else {
        if (s_exp_v <= min_gain_index) {
            s_exp_add = true;
            if (s_exp_v < min_gain_index) {
                s_exp_v = min_gain_index;
            }
        } else {
            s_exp_v -= 1;
        }
    }
    mit245_set_total_gain_val(dev, s_exp_v);
    ESP_LOGI(TAG, "Gain index=%" PRIu32 " (direction=%s)", s_exp_v, s_exp_add ? "UP" : "DOWN");
#else
    uint32_t min_exp = s_mit245_exp_min;
    uint32_t max_exp = cam_mit245->mit245_para.exposure_max;
    const uint32_t exp_step = 2;

    if (s_exp_add) {
        if (s_exp_v >= max_exp) {
            s_exp_add = false;
            if (s_exp_v > max_exp) {
                s_exp_v = max_exp;
            }
        } else {
            s_exp_v += exp_step;
            if (s_exp_v > max_exp) {
                s_exp_v = max_exp;
            }
        }
    } else {
        if (s_exp_v <= min_exp) {
            s_exp_add = true;
            if (s_exp_v < min_exp) {
                s_exp_v = min_exp;
            }
        } else {
            if (s_exp_v > exp_step) {
                s_exp_v -= exp_step;
            } else {
                s_exp_v = min_exp;
            }
        }
    }

    mit245_set_exp_val(dev, s_exp_v);
    ESP_LOGI(TAG, "Exposure=0x%" PRIx32 " (direction=%s)", s_exp_v, s_exp_add ? "UP" : "DOWN");
#endif
}
#endif

static esp_err_t mit245_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;
    ret = mit245_write(dev->sccb_handle, MIT245_REG_SLEEP_MODE, enable ? 0x01 : 0x00);

    if (ret == ESP_OK) {
        dev->stream_status = enable;
    }
    ESP_LOGD(TAG, "Stream=%d", enable);
#if MIT245_EXPOSURE_TEST_EN
    if (enable) {
        if (ae_timer_handle == NULL) {
#if MIT245_EXPOSURE_TEST_EN_GAIN
            s_exp_v = 0;
#else
            s_exp_v = s_mit245_exp_min;
#endif
            s_exp_add = true;

            ae_timer_handle = xTimerCreate("AE_t", 300 / portTICK_PERIOD_MS, pdTRUE,
                                           (void *)dev, ae_timer_callback);
            if (ae_timer_handle != NULL) {
                xTimerStart(ae_timer_handle, portMAX_DELAY);
            }
        }
    } else {
        if (ae_timer_handle != NULL) {
            xTimerStop(ae_timer_handle, portMAX_DELAY);
            xTimerDelete(ae_timer_handle, portMAX_DELAY);
            ae_timer_handle = NULL;
        }
    }
#endif
    return ret;
}

static esp_err_t mit245_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    esp_err_t ret = ESP_OK;
    struct mit245_cam *cam_mit245 = (struct mit245_cam *)dev->priv;
    switch (qdesc->id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = s_mit245_exp_min;
        qdesc->number.maximum = dev->cur_format->isp_info->isp_v1_info.vts - MIT245_EXP_MAX_OFFSET;
        qdesc->number.step = 1;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.exp_def;
        break;
    case ESP_CAM_SENSOR_EXPOSURE_US:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = EXPOSURE_MIT245_TO_V4L2(s_mit245_exp_min, dev->cur_format);
        qdesc->number.maximum = EXPOSURE_MIT245_TO_V4L2((dev->cur_format->isp_info->isp_v1_info.vts - MIT245_EXP_MAX_OFFSET), dev->cur_format);
        qdesc->number.step = MAX(EXPOSURE_MIT245_TO_V4L2(0x01, dev->cur_format), 1);
        qdesc->default_value = EXPOSURE_MIT245_TO_V4L2((dev->cur_format->isp_info->isp_v1_info.exp_def), dev->cur_format);
        break;
    case ESP_CAM_SENSOR_GAIN:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION;
        qdesc->enumeration.count = cam_mit245->mit245_para.limited_gain_index + 1;
        qdesc->enumeration.elements = mit245_total_gain_val_map;
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

static esp_err_t mit245_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    struct mit245_cam *cam_mit245 = (struct mit245_cam *)dev->priv;
    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        *(uint32_t *)arg = cam_mit245->mit245_para.exposure_val;
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        *(uint32_t *)arg = cam_mit245->mit245_para.gain_index;
        break;
    }
    default: {
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    }
    return ret;
}

static esp_err_t mit245_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;

    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        uint32_t u32_val = *(uint32_t *)arg;
        ret = mit245_set_exp_val(dev, u32_val);
        break;
    }
    case ESP_CAM_SENSOR_EXPOSURE_US: {
        uint32_t u32_val = *(uint32_t *)arg;
        uint32_t ori_exp = EXPOSURE_V4L2_TO_MIT245(u32_val, dev->cur_format);
        ret = mit245_set_exp_val(dev, ori_exp);
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        uint32_t u32_val = *(uint32_t *)arg;
        ret = mit245_set_total_gain_val(dev, u32_val);
        break;
    }
    case ESP_CAM_SENSOR_GROUP_EXP_GAIN: {
        esp_cam_sensor_gh_exp_gain_t *value = (esp_cam_sensor_gh_exp_gain_t *)arg;
        uint32_t ori_exp = 0;
        if (value->exposure_val != 0) {
            ori_exp = value->exposure_val;
        } else if (value->exposure_us != 0) {
            ori_exp = EXPOSURE_V4L2_TO_MIT245(value->exposure_us, dev->cur_format);
        } else {
            ret = ESP_ERR_INVALID_ARG;
            break;
        }
        ret = mit245_set_exp_val(dev, ori_exp);
        ret |= mit245_set_total_gain_val(dev, value->gain_index);
        break;
    }
    case ESP_CAM_SENSOR_VFLIP: {
        int *value = (int *)arg;
        ret = mit245_set_vflip(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        int *value = (int *)arg;
        ret = mit245_set_mirror(dev, *value);
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

static esp_err_t mit245_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);
    formats->count = ARRAY_SIZE(mit245_format_info_mipi);
    formats->format_array = &mit245_format_info_mipi[0];
    return ESP_OK;
}

static esp_err_t mit245_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_raw = 1;
    return 0;
}

static esp_err_t mit245_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    struct mit245_cam *cam_mit245 = (struct mit245_cam *)dev->priv;
    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        format = &mit245_format_info_mipi[get_mit245_mipi_actual_format_index()];
    }

    ret = mit245_write_array(dev->sccb_handle, (mit245_reginfo_t *)format->regs);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set format regs fail");
        return ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT;
    }

    dev->cur_format = format;
    // init para
    cam_mit245->mit245_para.exposure_val = dev->cur_format->isp_info->isp_v1_info.exp_def;
    cam_mit245->mit245_para.gain_index = dev->cur_format->isp_info->isp_v1_info.gain_def;
    cam_mit245->mit245_para.exposure_max = dev->cur_format->isp_info->isp_v1_info.vts - MIT245_EXP_MAX_OFFSET;

    return ret;
}

static esp_err_t mit245_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
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

static esp_err_t mit245_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    MIT245_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = mit245_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = mit245_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = mit245_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = mit245_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = mit245_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = mit245_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = mit245_get_sensor_id(dev, arg);
        break;
    default:
        break;
    }

    MIT245_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t mit245_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        MIT245_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
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

static esp_err_t mit245_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        MIT245_DISABLE_OUT_XCLK(dev->xclk_pin);
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

static esp_err_t mit245_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del mit245 (%p)", dev);
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

static const esp_cam_sensor_ops_t mit245_ops = {
    .query_para_desc = mit245_query_para_desc,
    .get_para_value = mit245_get_para_value,
    .set_para_value = mit245_set_para_value,
    .query_support_formats = mit245_query_support_formats,
    .query_support_capability = mit245_query_support_capability,
    .set_format = mit245_set_format,
    .get_format = mit245_get_format,
    .priv_ioctl = mit245_priv_ioctl,
    .del = mit245_delete
};

esp_cam_sensor_device_t *mit245_detect(esp_cam_sensor_config_t *config)
{
    ESP_LOGI(TAG, "mit245_detect");
    esp_cam_sensor_device_t *dev = NULL;
    struct mit245_cam *cam_mit245;

    if (config == NULL) {
        return NULL;
    }

    dev = calloc(1, sizeof(esp_cam_sensor_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    cam_mit245 = heap_caps_calloc(1, sizeof(struct mit245_cam), MALLOC_CAP_DEFAULT);
    if (!cam_mit245) {
        ESP_LOGE(TAG, "failed to calloc cam");
        free(dev);
        return NULL;
    }

    dev->name = (char *)MIT245_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &mit245_ops;
    dev->priv = cam_mit245;

    cam_mit245->mit245_para.limited_gain_index = ARRAY_SIZE(mit245_total_gain_val_map) - 1;
    for (size_t i = 0; i < ARRAY_SIZE(mit245_total_gain_val_map); i++) {
        if (mit245_total_gain_val_map[i] > s_limited_gain) {
            cam_mit245->mit245_para.limited_gain_index = (i > 0) ? (i - 1) : 0;
            break;
        }
    }
    dev->cur_format = &mit245_format_info_mipi[get_mit245_mipi_actual_format_index()];

    // Configure sensor power, clock, and SCCB port
    if (mit245_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (mit245_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != MIT245_PID) {
        ESP_LOGE(TAG, "Camera sensor is not MIT245, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    mit245_power_off(dev);
    free(dev->priv);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_MIT245_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(mit245_detect, ESP_CAM_SENSOR_MIPI_CSI, MIT245_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return mit245_detect(config);
}
#endif
