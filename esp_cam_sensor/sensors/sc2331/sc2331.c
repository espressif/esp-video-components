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
#include "sc2331_settings.h"
#include "sc2331.h"

/*
 * SC2331 camera sensor gain control.
 * Note1: The analog gain only has coarse gain, and no fine gain, so in the adjustment of analog gain.
 * Digital gain needs to replace analog fine gain for smooth transition, so as to avoid AGC oscillation.
 * Note2: the analog gain of sc2331 will be affected by temperature, it is recommended to increase Dgain first and then Again.
 */
typedef struct {
    uint8_t dgain_fine; // digital gain fine
    uint8_t dgain_coarse; // digital gain coarse
    uint8_t analog_gain;
} sc2331_gain_t;

typedef struct {
    uint32_t exposure_val;
    uint32_t exposure_max;
    uint32_t gain_index; // current gain index
    size_t limited_gain_index; // max valid gain index

    uint32_t vflip_en : 1;
    uint32_t hmirror_en : 1;
} sc2331_para_t;

struct sc2331_cam {
    sc2331_para_t sc2331_para;
};

#define SC2331_IO_MUX_LOCK(mux)
#define SC2331_IO_MUX_UNLOCK(mux)
#define SC2331_ENABLE_OUT_XCLK(pin,clk)
#define SC2331_DISABLE_OUT_XCLK(pin)

#define EXPOSURE_V4L2_UNIT_US                   100
#define EXPOSURE_V4L2_TO_SC2331(v, sf)          \
    ((uint32_t)(((double)v) * (sf)->fps * (sf)->isp_info->isp_v1_info.vts / (1000000 / EXPOSURE_V4L2_UNIT_US) + 0.5))
#define EXPOSURE_SC2331_TO_V4L2(v, sf)          \
    ((int32_t)(((double)v) * 1000000 / (sf)->fps / (sf)->isp_info->isp_v1_info.vts / EXPOSURE_V4L2_UNIT_US + 0.5))

#define SC2331_FETCH_EXP_H(val)     (((val) >> 12) & 0xF)
#define SC2331_FETCH_EXP_M(val)     (((val) >> 4) & 0xFF)
#define SC2331_FETCH_EXP_L(val)     (((val) & 0xF) << 4)

#define SC2331_FETCH_DGAIN_COARSE(val)  (((val) >> 8) & 0x03)
#define SC2331_FETCH_DGAIN_FINE(val)    ((val) & 0xFF)

#define SC2331_GROUP_HOLD_START        0x00
#define SC2331_GROUP_HOLD_END          0x30
#define SC2331_GROUP_HOLD_DELAY_FRAMES 0x01

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))
#define SC2331_SUPPORT_NUM CONFIG_CAMERA_SC2331_MAX_SUPPORT

static const uint32_t s_limited_gain = CONFIG_CAMERA_SC2331_ABSOLUTE_GAIN_LIMIT;
static const uint8_t s_sc2331_exp_min = 0x03;
static const uint8_t s_sc2331_exp_max_offset = 13;
static const char *TAG = "sc2331";

// total gain = analog_gain x digital_gain x 1000(To avoid decimal points, the final abs_gain is multiplied by 1000.)
static const uint32_t sc2331_total_gain_val_map[] = {
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

// SC2331 Gain map format: [DIG_FINE, DIG_COARSE, ANG]
static const sc2331_gain_t sc2331_gain_map[] = {
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

#if CONFIG_SOC_MIPI_CSI_SUPPORTED
static const esp_cam_sensor_isp_info_t sc2331_isp_info_mipi[] = {
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 81000000,
            .vts = 0x0465,
            .hts = 0x08c0,
            .tline_ns = 35555,
            .gain_def = 0,
            .exp_def = 0x8bd,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        },
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 36000000,
            .vts = 0x0bb8,
            .hts = 0x0960,
            .tline_ns = 66666,
            .gain_def = 0,
            .exp_def = 0x8bd,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        },
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 63000000,
            .vts = 0x0465,
            .hts = 0x08c0,
            .tline_ns = 35555,
            .gain_def = 0,
            .exp_def = 0x8bd,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        },
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 79200000,
            .vts = 0x04b0,
            .hts = 0x0898,
            .tline_ns = 27777,
            .gain_def = 0,
            .exp_def = 0x8bd,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        },
    },
};

static const esp_cam_sensor_format_t sc2331_format_info_mipi[] = {
#if CONFIG_CAMERA_SC2331_MIPI_RAW10_1920X1080_25FPS
    {
        .name = "MIPI_2lane_24Minput_RAW10_1920x1080_25fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1920,
        .height = 1080,
        .regs = sc2331_mipi_2lane_24Minput_1920x1080_raw10_25fps,
        .regs_size = ARRAY_SIZE(sc2331_mipi_2lane_24Minput_1920x1080_raw10_25fps),
        .fps = 25,
        .isp_info = &sc2331_isp_info_mipi[0],
        .mipi_info = {
            .mipi_clk = 315000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
#endif
#if CONFIG_CAMERA_SC2331_MIPI_RAW8_1920X1080_5FPS
    {
        .name = "MIPI_2lane_24Minput_RAW8_1920x1080_5fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1920,
        .height = 1080,
        .regs = sc2331_mipi_2lane_24Minput_1920x1080_raw8_5fps,
        .regs_size = ARRAY_SIZE(sc2331_mipi_2lane_24Minput_1920x1080_raw8_5fps),
        .fps = 5,
        .isp_info = &sc2331_isp_info_mipi[1],
        .mipi_info = {
            .mipi_clk = 144000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
#endif
#if CONFIG_CAMERA_SC2331_MIPI_RAW8_1920X1080_25FPS
    {
        .name = "MIPI_2lane_24Minput_RAW8_1920x1080_25fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1920,
        .height = 1080,
        .regs = sc2331_mipi_2lane_24Minput_1920x1080_raw8_25fps,
        .regs_size = ARRAY_SIZE(sc2331_mipi_2lane_24Minput_1920x1080_raw8_25fps),
        .fps = 25,
        .isp_info = &sc2331_isp_info_mipi[2],
        .mipi_info = {
            .mipi_clk = 252000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
#endif
#if CONFIG_CAMERA_SC2331_MIPI_RAW8_1920X1080_30FPS
    {
        .name = "MIPI_2lane_24Minput_RAW8_1920x1080_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1920,
        .height = 1080,
        .regs = sc2331_mipi_2lane_24Minput_1920x1080_raw8_30fps,
        .regs_size = ARRAY_SIZE(sc2331_mipi_2lane_24Minput_1920x1080_raw8_30fps),
        .fps = 30,
        .isp_info = &sc2331_isp_info_mipi[3],
        .mipi_info = {
            .mipi_clk = 316800000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
#endif
#if CONFIG_CAMERA_SC2331_MIPI_RAW8_1280X720_25FPS
    {
        .name = "MIPI_2lane_24Minput_RAW8_1280x720_25fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1280,
        .height = 720,
        .regs = sc2331_mipi_2lane_24Minput_1280x720_raw8_25fps,
        .regs_size = ARRAY_SIZE(sc2331_mipi_2lane_24Minput_1280x720_raw8_25fps),
        .fps = 25,
        .isp_info = &sc2331_isp_info_mipi[2],
        .mipi_info = {
            .mipi_clk = 252000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
#endif
#if CONFIG_CAMERA_SC2331_MIPI_RAW8_1280X720_30FPS
    {
        .name = "MIPI_2lane_24Minput_RAW8_1280x720_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1280,
        .height = 720,
        .regs = sc2331_mipi_2lane_24Minput_1280x720_raw8_30fps,
        .regs_size = ARRAY_SIZE(sc2331_mipi_2lane_24Minput_1280x720_raw8_30fps),
        .fps = 30,
        .isp_info = &sc2331_isp_info_mipi[3],
        .mipi_info = {
            .mipi_clk = 316800000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
#endif
};

#ifndef CONFIG_CAMERA_SC2331_MIPI_IF_FORMAT_INDEX_DEFAULT
#error "Please choose at least one format in menuconfig for SC2331"
#endif

static const int sc2331_format_index[] = {
#if CONFIG_CAMERA_SC2331_MIPI_RAW10_1920X1080_25FPS
    0,
#endif
#if CONFIG_CAMERA_SC2331_MIPI_RAW8_1920X1080_5FPS
    1,
#endif
#if CONFIG_CAMERA_SC2331_MIPI_RAW8_1920X1080_25FPS
    2,
#endif
#if CONFIG_CAMERA_SC2331_MIPI_RAW8_1920X1080_30FPS
    3,
#endif
#if CONFIG_CAMERA_SC2331_MIPI_RAW8_1280X720_25FPS
    4,
#endif
#if CONFIG_CAMERA_SC2331_MIPI_RAW8_1280X720_30FPS
    5,
#endif
};

static int get_sc2331_actual_format_index(void)
{
    int default_index = CONFIG_CAMERA_SC2331_MIPI_IF_FORMAT_INDEX_DEFAULT;
    for (size_t i = 0; i < ARRAY_SIZE(sc2331_format_index); i++) {
        if (sc2331_format_index[i] == default_index) {
            return i;
        }
    }
    return 0;
}
#endif

static esp_err_t sc2331_read(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a16v8(sccb_handle, reg, read_buf);
}

static esp_err_t sc2331_write(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t data)
{
    return esp_sccb_transmit_reg_a16v8(sccb_handle, reg, data);
}

/* write a array of registers  */
static esp_err_t sc2331_write_array(esp_sccb_io_handle_t sccb_handle, sc2331_reginfo_t *regarray)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && regarray[i].reg != SC2331_REG_END) {
        if (regarray[i].reg != SC2331_REG_DELAY) {
            ret = sc2331_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    ESP_LOGD(TAG, "Set array done[i=%d]", i);
    return ret;
}

static esp_err_t sc2331_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_data = 0;

    ret = sc2331_read(sccb_handle, reg, &reg_data);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (reg_data & ~mask) | ((value << offset) & mask);
    ret = sc2331_write(sccb_handle, reg, value);
    return ret;
}

static esp_err_t sc2331_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    return sc2331_set_reg_bits(dev->sccb_handle, 0x4501, 3, 1, enable ? 0x01 : 0x00);
}

static esp_err_t sc2331_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return ESP_OK;
}

static esp_err_t sc2331_soft_reset(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = sc2331_set_reg_bits(dev->sccb_handle, 0x0103, 0, 1, 0x01);
    delay_ms(5);
    return ret;
}

static esp_err_t sc2331_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t pid_h, pid_l;

    ret = sc2331_read(dev->sccb_handle, SC2331_REG_SENSOR_ID_H, &pid_h);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = sc2331_read(dev->sccb_handle, SC2331_REG_SENSOR_ID_L, &pid_l);
    if (ret != ESP_OK) {
        return ret;
    }
    id->pid = (pid_h << 8) | pid_l;

    return ret;
}

static esp_err_t sc2331_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;
    ret = sc2331_write(dev->sccb_handle, SC2331_REG_SLEEP_MODE, enable ? 0x01 : 0x00);

    if (ret == ESP_OK) {
        dev->stream_status = enable;
    }
    ESP_LOGD(TAG, "Stream=%d", enable);
    return ret;
}

static esp_err_t sc2331_set_mirror(esp_cam_sensor_device_t *dev, int enable)
{
    return sc2331_set_reg_bits(dev->sccb_handle, 0x3221, 1, 2,  enable ? 0x03 : 0x00);
}

static esp_err_t sc2331_set_vflip(esp_cam_sensor_device_t *dev, int enable)
{
    return sc2331_set_reg_bits(dev->sccb_handle, 0x3221, 5, 2, enable ? 0x03 : 0x00);
}

static esp_err_t sc2331_set_exp_val(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    esp_err_t ret;
    struct sc2331_cam *cam_sc2331 = (struct sc2331_cam *)dev->priv;
    uint32_t value_buf = MAX(u32_val, s_sc2331_exp_min);
    value_buf = MIN(value_buf, cam_sc2331->sc2331_para.exposure_max);

    ESP_LOGD(TAG, "set exposure 0x%" PRIx32, value_buf);
    /* 4 least significant bits of expsoure are fractional part */
    ret = sc2331_write(dev->sccb_handle,
                       SC2331_REG_SHUTTER_TIME_H,
                       SC2331_FETCH_EXP_H(value_buf));
    ret |= sc2331_write(dev->sccb_handle,
                        SC2331_REG_SHUTTER_TIME_M,
                        SC2331_FETCH_EXP_M(value_buf));
    ret |= sc2331_write(dev->sccb_handle,
                        SC2331_REG_SHUTTER_TIME_L,
                        SC2331_FETCH_EXP_L(value_buf));
    if (ret == ESP_OK) {
        cam_sc2331->sc2331_para.exposure_val = value_buf;
    }
    return ret;
}

static esp_err_t sc2331_set_total_gain_val(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    esp_err_t ret;
    struct sc2331_cam *cam_sc2331 = (struct sc2331_cam *)dev->priv;
    u32_val = MIN(u32_val, cam_sc2331->sc2331_para.limited_gain_index);
    // Limit gain index to valid range
    if (u32_val >= cam_sc2331->sc2331_para.limited_gain_index) {
        if (cam_sc2331->sc2331_para.limited_gain_index > 0) {
            u32_val = cam_sc2331->sc2331_para.limited_gain_index - 1;
        } else {
            u32_val = 0;  // Fallback to minimum gain if limit is 0
        }
    }

    ESP_LOGD(TAG, "dgain_fine %" PRIx8 ", dgain_coarse %" PRIx8 ", again_coarse %" PRIx8, sc2331_gain_map[u32_val].dgain_fine, sc2331_gain_map[u32_val].dgain_coarse, sc2331_gain_map[u32_val].analog_gain);
    ret = sc2331_write(dev->sccb_handle,
                       SC2331_REG_DIG_FINE_GAIN,
                       sc2331_gain_map[u32_val].dgain_fine);
    ret |= sc2331_write(dev->sccb_handle,
                        SC2331_REG_DIG_COARSE_GAIN,
                        sc2331_gain_map[u32_val].dgain_coarse);
    ret |= sc2331_write(dev->sccb_handle,
                        SC2331_REG_ANG_GAIN,
                        sc2331_gain_map[u32_val].analog_gain);
    if (ret == ESP_OK) {
        cam_sc2331->sc2331_para.gain_index = u32_val;
    }
    return ret;
}

static esp_err_t sc2331_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    esp_err_t ret = ESP_OK;
    struct sc2331_cam *cam_sc2331 = (struct sc2331_cam *)dev->priv;

    switch (qdesc->id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = s_sc2331_exp_min;
        qdesc->number.maximum = (2 * dev->cur_format->isp_info->isp_v1_info.vts) - s_sc2331_exp_max_offset;
        qdesc->number.step = 1;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.exp_def;
        break;
    case ESP_CAM_SENSOR_EXPOSURE_US:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = EXPOSURE_SC2331_TO_V4L2(s_sc2331_exp_min, dev->cur_format);
        qdesc->number.maximum = EXPOSURE_SC2331_TO_V4L2(((2 * dev->cur_format->isp_info->isp_v1_info.vts) - s_sc2331_exp_max_offset), dev->cur_format);
        qdesc->number.step = MAX(EXPOSURE_SC2331_TO_V4L2(0x01, dev->cur_format), 1);
        qdesc->default_value = EXPOSURE_SC2331_TO_V4L2((dev->cur_format->isp_info->isp_v1_info.exp_def), dev->cur_format);
        break;
    case ESP_CAM_SENSOR_GAIN:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION;
        qdesc->enumeration.count = cam_sc2331->sc2331_para.limited_gain_index;
        qdesc->enumeration.elements = sc2331_total_gain_val_map;
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

static esp_err_t sc2331_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    struct sc2331_cam *cam_sc2331 = (struct sc2331_cam *)dev->priv;
    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        *(uint32_t *)arg = cam_sc2331->sc2331_para.exposure_val;
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        *(uint32_t *)arg = cam_sc2331->sc2331_para.gain_index;
        break;
    }
    default: {
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    }
    return ret;
}

static esp_err_t sc2331_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;

    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        uint32_t u32_val = *(uint32_t *)arg;
        ret = sc2331_set_exp_val(dev, u32_val);
        break;
    }
    case ESP_CAM_SENSOR_EXPOSURE_US: {
        uint32_t u32_val = *(uint32_t *)arg;
        uint32_t ori_exp = EXPOSURE_V4L2_TO_SC2331(u32_val, dev->cur_format);
        ret = sc2331_set_exp_val(dev, ori_exp);
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        uint32_t u32_val = *(uint32_t *)arg;
        ret = sc2331_set_total_gain_val(dev, u32_val);
        break;
    }
    case ESP_CAM_SENSOR_GROUP_EXP_GAIN: {
        esp_cam_sensor_gh_exp_gain_t *value = (esp_cam_sensor_gh_exp_gain_t *)arg;
        uint32_t ori_exp = 0;
        if (value->exposure_us != 0) {
            ori_exp = EXPOSURE_V4L2_TO_SC2331(value->exposure_us, dev->cur_format);
        } else if (value->exposure_val != 0) {
            ori_exp = value->exposure_val;
        } else {
            ret = ESP_ERR_INVALID_ARG;
            break;
        }
        ret = sc2331_set_exp_val(dev, ori_exp);
        ret |= sc2331_set_total_gain_val(dev, value->gain_index);
        break;
    }
    case ESP_CAM_SENSOR_VFLIP: {
        int *value = (int *)arg;
        ret = sc2331_set_vflip(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        int *value = (int *)arg;
        ret = sc2331_set_mirror(dev, *value);
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

static esp_err_t sc2331_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);
#if CONFIG_SOC_MIPI_CSI_SUPPORTED
    if (dev->sensor_port == ESP_CAM_SENSOR_MIPI_CSI) {
        formats->count = ARRAY_SIZE(sc2331_format_info_mipi);
        formats->format_array = &sc2331_format_info_mipi[0];
    }
#endif
    return ESP_OK;
}

static esp_err_t sc2331_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_raw = 1;
    return 0;
}

static esp_err_t sc2331_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    struct sc2331_cam *cam_sc2331 = (struct sc2331_cam *)dev->priv;
    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        format = &sc2331_format_info_mipi[get_sc2331_actual_format_index()];
    }

    ret = sc2331_write_array(dev->sccb_handle, (sc2331_reginfo_t *)format->regs);

    // Improve MIPI driving capabilities
    sc2331_write(dev->sccb_handle, 0x3650, 0x33);
    sc2331_write(dev->sccb_handle, 0x3651, 0x7e);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set format regs fail");
        return ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT;
    }

    dev->cur_format = format;
    // init para
    cam_sc2331->sc2331_para.exposure_val = dev->cur_format->isp_info->isp_v1_info.exp_def;
    cam_sc2331->sc2331_para.gain_index = dev->cur_format->isp_info->isp_v1_info.gain_def;
    cam_sc2331->sc2331_para.exposure_max = (2 * dev->cur_format->isp_info->isp_v1_info.vts) - s_sc2331_exp_max_offset;

    return ret;
}

static esp_err_t sc2331_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
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

static esp_err_t sc2331_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    SC2331_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = sc2331_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = sc2331_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = sc2331_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = sc2331_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = sc2331_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = sc2331_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = sc2331_get_sensor_id(dev, arg);
        break;
    default:
        break;
    }

    SC2331_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t sc2331_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        SC2331_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->pwdn_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        ret = gpio_config(&conf);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "gpio config failed");

        // carefully, logic is inverted compared to reset pin
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

static esp_err_t sc2331_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        SC2331_DISABLE_OUT_XCLK(dev->xclk_pin);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_set_level(dev->pwdn_pin, 0);
        delay_ms(5);
    }

    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
    }

    return ret;
}

static esp_err_t sc2331_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del sc2331 (%p)", dev);
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

static const esp_cam_sensor_ops_t sc2331_ops = {
    .query_para_desc = sc2331_query_para_desc,
    .get_para_value = sc2331_get_para_value,
    .set_para_value = sc2331_set_para_value,
    .query_support_formats = sc2331_query_support_formats,
    .query_support_capability = sc2331_query_support_capability,
    .set_format = sc2331_set_format,
    .get_format = sc2331_get_format,
    .priv_ioctl = sc2331_priv_ioctl,
    .del = sc2331_delete
};

esp_cam_sensor_device_t *sc2331_detect(esp_cam_sensor_config_t *config)
{
    esp_cam_sensor_device_t *dev = NULL;
    struct sc2331_cam *cam_sc2331;
    if (config == NULL) {
        return NULL;
    }

    dev = calloc(1, sizeof(esp_cam_sensor_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    cam_sc2331 = heap_caps_calloc(1, sizeof(struct sc2331_cam), MALLOC_CAP_DEFAULT);
    if (!cam_sc2331) {
        ESP_LOGE(TAG, "failed to calloc cam");
        free(dev);
        return NULL;
    }

    dev->name = (char *)SC2331_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &sc2331_ops;
    dev->priv = cam_sc2331;

    cam_sc2331->sc2331_para.limited_gain_index = ARRAY_SIZE(sc2331_total_gain_val_map);
    for (size_t i = 0; i < ARRAY_SIZE(sc2331_total_gain_val_map); i++) {
        if (sc2331_total_gain_val_map[i] > s_limited_gain) {
            cam_sc2331->sc2331_para.limited_gain_index = i;
            break;
        }
    }
#if CONFIG_SOC_MIPI_CSI_SUPPORTED
    if (config->sensor_port == ESP_CAM_SENSOR_MIPI_CSI) {
        dev->cur_format = &sc2331_format_info_mipi[get_sc2331_actual_format_index()];
    }
#endif
    if (dev->cur_format == NULL) {
        ESP_LOGE(TAG, "Default format load failed");
        goto err_free_handler;
    }
    // Configure sensor power, clock, and SCCB port
    if (sc2331_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (sc2331_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != SC2331_PID) {
        ESP_LOGE(TAG, "Camera sensor is not SC2331, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    sc2331_power_off(dev);
    free(dev->priv);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_SC2331_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(sc2331_detect, ESP_CAM_SENSOR_MIPI_CSI, SC2331_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return sc2331_detect(config);
}
#endif
