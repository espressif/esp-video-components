/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"
#include "sc2336_settings.h"
#include "sc2336.h"

/*
 * SC2336 camera sensor gain control.
 * Note1: The analog gain only has coarse gain, and no fine gain, so in the adjustment of analog gain.
 * Digital gain needs to replace analog fine gain for smooth transition, so as to avoid AGC oscillation.
 * Note2: the analog gain of sc2336 will be affected by temperature, it is recommended to increase Dgain first and then Again.
 */
typedef struct {
    uint8_t dgain_fine; // digital gain fine
    uint8_t dgain_coarse; // digital gain coarse
    uint8_t analog_gain;
    uint32_t totol_gain;  // total gain = analog_gain x digital_gain x 1000(To avoid decimal points, the final total_gain is multiplied by 1000.)
} sc2336_gain_t;

#define SC2336_IO_MUX_LOCK(mux)
#define SC2336_IO_MUX_UNLOCK(mux)
#define SC2336_ENABLE_OUT_XCLK(pin,clk)
#define SC2336_DISABLE_OUT_XCLK(pin)

#define SC2336_EXPOSURE_MIN     1
#define SC2336_EXPOSURE_STEP        1
#define SC2336_VTS_MAX          0x7fff // Max exposure is VTS-6

#define SC2336_FETCH_EXP_H(val)     (((val) >> 12) & 0xF)
#define SC2336_FETCH_EXP_M(val)     (((val) >> 4) & 0xFF)
#define SC2336_FETCH_EXP_L(val)     (((val) & 0xF) << 4)

#define SC2336_FETCH_DGAIN_COARSE(val)  (((val) >> 8) & 0x03)
#define SC2336_FETCH_DGAIN_FINE(val)    ((val) & 0xFF)

#define SC2336_PID         0xcb3a
#define SC2336_SENSOR_NAME "SC2336"
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))
#define SC2336_SUPPORT_NUM CONFIG_CAMERA_SC2336_MAX_SUPPORT

static const char *TAG = "sc2336";

// SC2336 Gain map format: [DIG_FINE, DIG_COARSE, ANG, GAINX1000]
static const sc2336_gain_t sc2336_gain_map[] = {
    {0x80, 0x00, 0x00, 1000},
    {0x84, 0x00, 0x00, 1031},
    {0x88, 0x00, 0x00, 1063},
    {0x8c, 0x00, 0x00, 1094},
    {0x90, 0x00, 0x00, 1125},
    {0x94, 0x00, 0x00, 1156},
    {0x98, 0x00, 0x00, 1188},
    {0x9c, 0x00, 0x00, 1219},
    {0xa0, 0x00, 0x00, 1250},
    {0xa4, 0x00, 0x00, 1281},
    {0xa8, 0x00, 0x00, 1313},
    {0xac, 0x00, 0x00, 1344},
    {0xb0, 0x00, 0x00, 1375},
    {0xb4, 0x00, 0x00, 1406},
    {0xb8, 0x00, 0x00, 1438},
    {0xbc, 0x00, 0x00, 1469},
    {0xc0, 0x00, 0x00, 1500},
    {0xc4, 0x00, 0x00, 1531},
    {0xc8, 0x00, 0x00, 1563},
    {0xcc, 0x00, 0x00, 1594},
    {0xd0, 0x00, 0x00, 1625},
    {0xd4, 0x00, 0x00, 1656},
    {0xd8, 0x00, 0x00, 1688},
    {0xdc, 0x00, 0x00, 1719},
    {0xe0, 0x00, 0x00, 1750},
    {0xe4, 0x00, 0x00, 1781},
    {0xe8, 0x00, 0x00, 1813},
    {0xec, 0x00, 0x00, 1844},
    {0xf0, 0x00, 0x00, 1875},
    {0xf4, 0x00, 0x00, 1906},
    {0xf8, 0x00, 0x00, 1938},
    {0xfc, 0x00, 0x00, 1969},
    // 2X
    {0x80, 0x01, 0x00, 2000},
    {0x84, 0x01, 0x00, 2063},
    {0x88, 0x01, 0x00, 2125},
    {0x8c, 0x01, 0x00, 2188},
    {0x90, 0x01, 0x00, 2250},
    {0x94, 0x01, 0x00, 2313},
    {0x98, 0x01, 0x00, 2375},
    {0x9c, 0x01, 0x00, 2438},
    {0xa0, 0x01, 0x00, 2500},
    {0xa4, 0x01, 0x00, 2563},
    {0xa8, 0x01, 0x00, 2625},
    {0xac, 0x01, 0x00, 2688},
    {0xb0, 0x01, 0x00, 2750},
    {0xb4, 0x01, 0x00, 2813},
    {0xb8, 0x01, 0x00, 2875},
    {0xbc, 0x01, 0x00, 2938},
    {0xc0, 0x01, 0x00, 3000},
    {0xc4, 0x01, 0x00, 3063},
    {0xc8, 0x01, 0x00, 3125},
    {0xcc, 0x01, 0x00, 3188},
    {0xd0, 0x01, 0x00, 3250},
    {0xd4, 0x01, 0x00, 3313},
    {0xd8, 0x01, 0x00, 3375},
    {0xdc, 0x01, 0x00, 3438},
    {0xe0, 0x01, 0x00, 3500},
    {0xe4, 0x01, 0x00, 3563},
    {0xe8, 0x01, 0x00, 3625},
    {0xec, 0x01, 0x00, 3688},
    {0xf0, 0x01, 0x00, 3750},
    {0xf4, 0x01, 0x00, 3813},
    {0xf8, 0x01, 0x00, 3875},
    {0xfc, 0x01, 0x00, 3938},
    // 4X
    {0x80, 0x01, 0x08, 4000},
    {0x84, 0x01, 0x08, 4126},
    {0x88, 0x01, 0x08, 4250},
    {0x8c, 0x01, 0x08, 4376},
    {0x90, 0x01, 0x08, 4500},
    {0x94, 0x01, 0x08, 4626},
    {0x98, 0x01, 0x08, 4750},
    {0x9c, 0x01, 0x08, 4876},
    {0xa0, 0x01, 0x08, 5000},
    {0xa4, 0x01, 0x08, 5126},
    {0xa8, 0x01, 0x08, 5250},
    {0xac, 0x01, 0x08, 5376},
    {0xb0, 0x01, 0x08, 5500},
    {0xb4, 0x01, 0x08, 5626},
    {0xb8, 0x01, 0x08, 5750},
    {0xbc, 0x01, 0x08, 5876},
    {0xc0, 0x01, 0x08, 6000},
    {0xc4, 0x01, 0x08, 6126},
    {0xc8, 0x01, 0x08, 6250},
    {0xcc, 0x01, 0x08, 6376},
    {0xd0, 0x01, 0x08, 6500},
    {0xd4, 0x01, 0x08, 6626},
    {0xd8, 0x01, 0x08, 6750},
    {0xdc, 0x01, 0x08, 6876},
    {0xe0, 0x01, 0x08, 7000},
    {0xe4, 0x01, 0x08, 7126},
    {0xe8, 0x01, 0x08, 7250},
    {0xec, 0x01, 0x08, 7376},
    {0xf0, 0x01, 0x08, 7500},
    {0xf4, 0x01, 0x08, 7626},
    {0xf8, 0x01, 0x08, 7750},
    {0xfc, 0x01, 0x08, 7876},
    // 8X
    {0x80, 0x01, 0x09, 8000},
    {0x84, 0x01, 0x09, 8252},
    {0x88, 0x01, 0x09, 8500},
    {0x8c, 0x01, 0x09, 8752},
    {0x90, 0x01, 0x09, 9000},
    {0x94, 0x01, 0x09, 9252},
    {0x98, 0x01, 0x09, 9500},
    {0x9c, 0x01, 0x09, 9752},
    {0xa0, 0x01, 0x09, 10000},
    {0xa4, 0x01, 0x09, 10252},
    {0xa8, 0x01, 0x09, 10500},
    {0xac, 0x01, 0x09, 10752},
    {0xb0, 0x01, 0x09, 11000},
    {0xb4, 0x01, 0x09, 11252},
    {0xb8, 0x01, 0x09, 11500},
    {0xbc, 0x01, 0x09, 11752},
    {0xc0, 0x01, 0x09, 12000},
    {0xc4, 0x01, 0x09, 12252},
    {0xc8, 0x01, 0x09, 12500},
    {0xcc, 0x01, 0x09, 12752},
    {0xd0, 0x01, 0x09, 13000},
    {0xd4, 0x01, 0x09, 13252},
    {0xd8, 0x01, 0x09, 13500},
    {0xdc, 0x01, 0x09, 13752},
    {0xe0, 0x01, 0x09, 14000},
    {0xe4, 0x01, 0x09, 14252},
    {0xe8, 0x01, 0x09, 14500},
    {0xec, 0x01, 0x09, 14752},
    {0xf0, 0x01, 0x09, 15000},
    {0xf4, 0x01, 0x09, 15252},
    {0xf8, 0x01, 0x09, 15500},
    {0xfc, 0x01, 0x09, 15752},
    // 16X
    {0x80, 0x01, 0x0b, 16000},
    {0x84, 0x01, 0x0b, 16504},
    {0x88, 0x01, 0x0b, 17000},
    {0x8c, 0x01, 0x0b, 17504},
    {0x90, 0x01, 0x0b, 18000},
    {0x94, 0x01, 0x0b, 18504},
    {0x98, 0x01, 0x0b, 19000},
    {0x9c, 0x01, 0x0b, 19504},
    {0xa0, 0x01, 0x0b, 20000},
    {0xa4, 0x01, 0x0b, 20504},
    {0xa8, 0x01, 0x0b, 21000},
    {0xac, 0x01, 0x0b, 21504},
    {0xb0, 0x01, 0x0b, 22000},
    {0xb4, 0x01, 0x0b, 22504},
    {0xb8, 0x01, 0x0b, 23000},
    {0xbc, 0x01, 0x0b, 23504},
    {0xc0, 0x01, 0x0b, 24000},
    {0xc4, 0x01, 0x0b, 24504},
    {0xc8, 0x01, 0x0b, 25000},
    {0xcc, 0x01, 0x0b, 25504},
    {0xd0, 0x01, 0x0b, 26000},
    {0xd4, 0x01, 0x0b, 26504},
    {0xd8, 0x01, 0x0b, 27000},
    {0xdc, 0x01, 0x0b, 27504},
    {0xe0, 0x01, 0x0b, 28000},
    {0xe4, 0x01, 0x0b, 28504},
    {0xe8, 0x01, 0x0b, 29000},
    {0xec, 0x01, 0x0b, 29504},
    {0xf0, 0x01, 0x0b, 30000},
    {0xf4, 0x01, 0x0b, 30504},
    {0xf8, 0x01, 0x0b, 31000},
    {0xfc, 0x01, 0x0b, 31504},
    //32x
    {0x80, 0x01, 0x0f, 32000},
    {0x84, 0x01, 0x0f, 33008},
    {0x88, 0x01, 0x0f, 34000},
    {0x8c, 0x01, 0x0f, 35008},
    {0x90, 0x01, 0x0f, 36000},
    {0x94, 0x01, 0x0f, 37008},
    {0x98, 0x01, 0x0f, 38000},
    {0x9c, 0x01, 0x0f, 39008},
    {0xa0, 0x01, 0x0f, 40000},
    {0xa4, 0x01, 0x0f, 41008},
    {0xa8, 0x01, 0x0f, 42000},
    {0xac, 0x01, 0x0f, 43008},
    {0xb0, 0x01, 0x0f, 44000},
    {0xb4, 0x01, 0x0f, 45008},
    {0xb8, 0x01, 0x0f, 46000},
    {0xbc, 0x01, 0x0f, 47008},
    {0xc0, 0x01, 0x0f, 48000},
    {0xc4, 0x01, 0x0f, 49008},
    {0xc8, 0x01, 0x0f, 50000},
    {0xcc, 0x01, 0x0f, 51008},
    {0xd0, 0x01, 0x0f, 52000},
    {0xd4, 0x01, 0x0f, 53008},
    {0xd8, 0x01, 0x0f, 54000},
    {0xdc, 0x01, 0x0f, 55008},
    {0xe0, 0x01, 0x0f, 56000},
    {0xe4, 0x01, 0x0f, 57008},
    {0xe8, 0x01, 0x0f, 58000},
    {0xec, 0x01, 0x0f, 59008},
    {0xf0, 0x01, 0x0f, 60000},
    {0xf4, 0x01, 0x0f, 61008},
    {0xf8, 0x01, 0x0f, 62000},
    {0xfc, 0x01, 0x0f, 63008},
    //64x
    {0x80, 0x01, 0x1f, 64000},
    {0x84, 0x01, 0x1f, 66016},
    {0x88, 0x01, 0x1f, 68000},
    {0x8c, 0x01, 0x1f, 70016},
    {0x90, 0x01, 0x1f, 72000},
    {0x94, 0x01, 0x1f, 74016},
    {0x98, 0x01, 0x1f, 76000},
    {0x9c, 0x01, 0x1f, 78016},
    {0xa0, 0x01, 0x1f, 80000},
    {0xa4, 0x01, 0x1f, 82016},
    {0xa8, 0x01, 0x1f, 84000},
    {0xac, 0x01, 0x1f, 86016},
    {0xb0, 0x01, 0x1f, 88000},
    {0xb4, 0x01, 0x1f, 90016},
    {0xb8, 0x01, 0x1f, 92000},
    {0xbc, 0x01, 0x1f, 94016},
    {0xc0, 0x01, 0x1f, 96000},
    {0xc4, 0x01, 0x1f, 98016},
    {0xc8, 0x01, 0x1f, 100000},
    {0xcc, 0x01, 0x1f, 102016},
    {0xd0, 0x01, 0x1f, 104000},
    {0xd4, 0x01, 0x1f, 106016},
    {0xd8, 0x01, 0x1f, 108000},
    {0xdc, 0x01, 0x1f, 110016},
    {0xe0, 0x01, 0x1f, 112000},
    {0xe4, 0x01, 0x1f, 114016},
    {0xe8, 0x01, 0x1f, 116000},
    {0xec, 0x01, 0x1f, 118016},
    {0xf0, 0x01, 0x1f, 120000},
    {0xf4, 0x01, 0x1f, 122016},
    {0xf8, 0x01, 0x1f, 124000},
    {0xfc, 0x01, 0x1f, 126016},
};

static const esp_cam_sensor_isp_info_t sc2336_isp_info[] = {
    /* For MIPI */
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 81000000,
            .vts = 1500,
            .hts = 1800,
            .gain_def = sc2336_gain_map[0].totol_gain, // depend on {0x3e06, 0x3e07, 0x3e09}, since these registers are not set in format reg_list, the default values ​​are used here.
            .exp_def = 0x5d6, // depend on {0x3e00, 0x3e01, 0x3e02}, see format_reg_list to get the default value.
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 81000000,
            .vts = 1800,
            .hts = 900,
            .gain_def = sc2336_gain_map[0].totol_gain,
            .exp_def = 0x37e,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        },
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 81000000,
            .vts = 1800,
            .hts = 750,
            .gain_def = sc2336_gain_map[0].totol_gain,
            .exp_def = 0x2e8,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        },
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 81000000,
            .vts = 1125,
            .hts = 1200,
            .gain_def = sc2336_gain_map[0].totol_gain,
            .exp_def = 0x4af,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        },
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 66000000,
            .vts = 2250,
            .hts = 1200,
            .gain_def = sc2336_gain_map[0].totol_gain,
            .exp_def = 0x4af,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        },
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 81000000,
            .vts = 2250,
            .hts = 1200,
            .gain_def = sc2336_gain_map[0].totol_gain,
            .exp_def = 0x4aa,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        },
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 49500000,
            .vts = 2200,
            .hts = 750,
            .gain_def = sc2336_gain_map[0].totol_gain,
            .exp_def = 0x3e2,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 67200000,
            .vts = 1000,
            .hts = 2240,
            .gain_def = sc2336_gain_map[0].totol_gain,
            .exp_def = 0x207,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 84000000,
            .vts = 1250,
            .hts = 2240,
            .gain_def = sc2336_gain_map[0].totol_gain,
            .exp_def = 0x4dc,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 84000000,
            .vts = 1250,
            .hts = 2240,
            .gain_def = sc2336_gain_map[0].totol_gain,
            .exp_def = 0x4dc,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 84000000,
            .vts = 1250,
            .hts = 2240,
            .gain_def = sc2336_gain_map[0].totol_gain,
            .exp_def = 0x4dc,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 84000000,
            .vts = 1000,
            .hts = 2400,
            .gain_def = sc2336_gain_map[0].totol_gain,
            .exp_def = 0x3e2,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
    /* For DVP */
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 42000000,
            .vts = 525,
            .hts = 1600,
            .gain_def = sc2336_gain_map[0].totol_gain,
            .exp_def = 0x219,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
};

static const esp_cam_sensor_format_t sc2336_format_info[] = {
    /* For MIPI */
    {
        .name = "MIPI_2lane_24Minput_RAW10_1280x720_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1280,
        .height = 720,
        .regs = init_reglist_MIPI_2lane_720p_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_720p_30fps),
        .fps = 30,
        .isp_info = &sc2336_isp_info[0],
        .mipi_info = {
            .mipi_clk = 405000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_RAW10_1280x720_50fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1280,
        .height = 720,
        .regs = init_reglist_MIPI_2lane_720p_50fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_720p_50fps),
        .fps = 50,
        .isp_info = &sc2336_isp_info[1],
        .mipi_info = {
            .mipi_clk = 405000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_RAW10_1280x720_60fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1280,
        .height = 720,
        .regs = init_reglist_MIPI_2lane_720p_60fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_720p_60fps),
        .fps = 60,
        .isp_info = &sc2336_isp_info[2],
        .mipi_info = {
            .mipi_clk = 405000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_1lane_24Minput_RAW10_1920x1080_25fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1920,
        .height = 1080,
        .regs = init_reglist_MIPI_1lane_1080p_25fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_1lane_1080p_25fps),
        .fps = 25,
        .isp_info = &sc2336_isp_info[3],
        .mipi_info = {
            .mipi_clk = 660000000,
            .lane_num = 1,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_RAW10_1920x1080_25fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1920,
        .height = 1080,
        .regs = init_reglist_MIPI_2lane_1080p_25fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_1080p_25fps),
        .fps = 25,
        .isp_info = &sc2336_isp_info[4],
        .mipi_info = {
            .mipi_clk = 330000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_RAW10_1920x1080_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1920,
        .height = 1080,
        .regs = init_reglist_MIPI_2lane_1080p_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_1080p_30fps),
        .fps = 30,
        .isp_info = &sc2336_isp_info[5],
        .mipi_info = {
            .mipi_clk = 405000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_RAW10_800*800_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 800,
        .height = 800,
        .regs = init_reglist_MIPI_2lane_10bit_800x800_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_10bit_800x800_30fps),
        .fps = 30,
        .isp_info = &sc2336_isp_info[6],
        .mipi_info = {
            .mipi_clk = 336000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_RAW10_640*480_50fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 640,
        .height = 480,
        .regs = init_reglist_MIPI_2lane_10bit_640x480_50fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_10bit_640x480_50fps),
        .fps = 50,
        .isp_info = &sc2336_isp_info[7],
        .mipi_info = {
            .mipi_clk = 210000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_RAW8_1920*1080_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1920,
        .height = 1080,
        .regs = init_reglist_MIPI_2lane_1080p_raw8_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_1080p_raw8_30fps),
        .fps = 30,
        .isp_info = &sc2336_isp_info[8],
        .mipi_info = {
            .mipi_clk = 336000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_RAW8_1280*720_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1280,
        .height = 720,
        .regs = init_reglist_MIPI_2lane_720p_raw8_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_720p_raw8_30fps),
        .fps = 30,
        .isp_info = &sc2336_isp_info[9],
        .mipi_info = {
            .mipi_clk = 336000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_RAW8_800*800_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 800,
        .height = 800,
        .regs = init_reglist_MIPI_2lane_800x800_raw8_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_800x800_raw8_30fps),
        .fps = 30,
        .isp_info = &sc2336_isp_info[10],
        .mipi_info = {
            .mipi_clk = 336000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_RAW8_1024*600_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1024,
        .height = 600,
        .regs = init_reglist_MIPI_2lane_1024x600_raw8_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_1024x600_raw8_30fps),
        .fps = 30,
        .isp_info = &sc2336_isp_info[11],
        .mipi_info = {
            .mipi_clk = 288000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    /* For DVP */
    {
        .name = "DVP_8bits_24Minput_RAW10_1280*720_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 24000000,
        .width = 1280,
        .height = 720,
        .regs = init_reglist_DVP_720p_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_DVP_720p_30fps),
        .fps = 30,
        .isp_info = &sc2336_isp_info[12],
        .mipi_info = {0},
        .reserved = NULL,
    },
};

static esp_err_t sc2336_read(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a16v8(sccb_handle, reg, read_buf);
}

static esp_err_t sc2336_write(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t data)
{
    return esp_sccb_transmit_reg_a16v8(sccb_handle, reg, data);
}

/* write a array of registers  */
static esp_err_t sc2336_write_array(esp_sccb_io_handle_t sccb_handle, sc2336_reginfo_t *regarray)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && regarray[i].reg != SC2336_REG_END) {
        if (regarray[i].reg != SC2336_REG_DELAY) {
            ret = sc2336_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    return ret;
}

static esp_err_t sc2336_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_data = 0;

    ret = sc2336_read(sccb_handle, reg, &reg_data);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (ret & ~mask) | ((value << offset) & mask);
    ret = sc2336_write(sccb_handle, reg, value);
    return ret;
}

static esp_err_t sc2336_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    return sc2336_set_reg_bits(dev->sccb_handle, 0x4501, 3, 1, enable ? 0x01 : 0x00);
}

static esp_err_t sc2336_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return ESP_OK;
}

static esp_err_t sc2336_soft_reset(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = sc2336_set_reg_bits(dev->sccb_handle, 0x0103, 0, 1, 0x01);
    delay_ms(5);
    return ret;
}

static esp_err_t sc2336_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t pid_h, pid_l;

    ret = sc2336_read(dev->sccb_handle, SC2336_REG_SENSOR_ID_H, &pid_h);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = sc2336_read(dev->sccb_handle, SC2336_REG_SENSOR_ID_L, &pid_l);
    if (ret != ESP_OK) {
        return ret;
    }
    id->pid = (pid_h << 8) | pid_l;

    return ret;
}

static esp_err_t sc2336_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;
    ret = sc2336_write(dev->sccb_handle, SC2336_REG_SLEEP_MODE, enable ? 0x01 : 0x00);

    dev->stream_status = enable;
    ESP_LOGD(TAG, "Stream=%d", enable);
    return ret;
}

static esp_err_t sc2336_set_mirror(esp_cam_sensor_device_t *dev, int enable)
{
    return sc2336_set_reg_bits(dev->sccb_handle, 0x3221, 1, 2,  enable ? 0x03 : 0x00);
}

static esp_err_t sc2336_set_vflip(esp_cam_sensor_device_t *dev, int enable)
{
    return sc2336_set_reg_bits(dev->sccb_handle, 0x3221, 5, 2, enable ? 0x03 : 0x00);
}

static esp_err_t sc2336_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    esp_err_t ret = ESP_OK;
    switch (qdesc->id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = 2;
        qdesc->number.maximum = dev->cur_format->isp_info->isp_v1_info.vts - 6; // max = VTS-6 = height+vblank-6, so when update vblank, exposure_max must be updated
        qdesc->number.step = 1;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.exp_def;
        break;
    // Todo, define menu type to get &sc2336_gain_map
    case ESP_CAM_SENSOR_GAIN:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION;
        qdesc->number.minimum = 1;
        qdesc->number.maximum = 63;
        qdesc->number.step = 1;
        // qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.gain_def;
        qdesc->default_value = sc2336_gain_map[0].totol_gain; // use gain or gain_map element？
        break;
    default: {
        ESP_LOGE(TAG, "id=%"PRIx32" is not supported", qdesc->id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }
    return ret;
}

static esp_err_t sc2336_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t sc2336_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;

    switch (id) {
    case ESP_CAM_SENSOR_VFLIP: {
        int *value = (int *)arg;

        ret = sc2336_set_vflip(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        int *value = (int *)arg;

        ret = sc2336_set_mirror(dev, *value);
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

static esp_err_t sc2336_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);

    formats->count = ARRAY_SIZE(sc2336_format_info);
    formats->format_array = &sc2336_format_info[0];
    return ESP_OK;
}

static esp_err_t sc2336_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_raw = 1;
    return 0;
}

static esp_err_t sc2336_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);

    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        if (dev->sensor_port != ESP_CAM_SENSOR_DVP) {
            format = &sc2336_format_info[CONFIG_CAMERA_SC2336_MIPI_IF_FORMAT_INDEX_DAFAULT];
        } else {
            format = &sc2336_format_info[CONFIG_CAMERA_SC2336_DVP_IF_FORMAT_INDEX_DAFAULT];
        }
    }

    ret = sc2336_write_array(dev->sccb_handle, (sc2336_reginfo_t *)format->regs);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set format regs fail");
        return ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT;
    }

    dev->cur_format = format;

    return ret;
}

static esp_err_t sc2336_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
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

static esp_err_t sc2336_set_gain(esp_cam_sensor_device_t *dev, sc2336_gain_t *packaged_gain)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, packaged_gain);
    esp_err_t ret = ESP_FAIL;

    ESP_LOGD(TAG, "dgain_fine %" PRIx8 ", dgain_coarse %" PRIx8 ", again_coarse %" PRIx8, packaged_gain->dgain_fine, packaged_gain->dgain_coarse, packaged_gain->analog_gain);
    ret = sc2336_write(dev->sccb_handle, SC2336_REG_DIG_FINE_GAIN, packaged_gain->dgain_fine);
    ret |= sc2336_write(dev->sccb_handle, SC2336_REG_DIG_COARSE_GAIN, packaged_gain->dgain_coarse);
    ret |= sc2336_write(dev->sccb_handle, SC2336_REG_ANG_GAIN, packaged_gain->analog_gain);
    return ret;
}

static esp_err_t sc2336_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    uint32_t ctrl_val = 0;
    esp_cam_sensor_reg_val_t *sensor_reg;
    SC2336_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = sc2336_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = sc2336_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = sc2336_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = sc2336_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = sc2336_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = sc2336_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_S_EXPOSURE:
        ctrl_val = *(uint32_t *)arg;
        ESP_LOGD(TAG, "set exposure 0x%" PRIx32, ctrl_val);
        /* 4 least significant bits of expsoure are fractional part */
        ret = sc2336_write(dev->sccb_handle,
                           SC2336_REG_SHUTTER_TIME_H,
                           SC2336_FETCH_EXP_H(ctrl_val));
        ret |= sc2336_write(dev->sccb_handle,
                            SC2336_REG_SHUTTER_TIME_M,
                            SC2336_FETCH_EXP_M(ctrl_val));
        ret |= sc2336_write(dev->sccb_handle,
                            SC2336_REG_SHUTTER_TIME_L,
                            SC2336_FETCH_EXP_L(ctrl_val));
        break;
    case ESP_CAM_SENSOR_IOC_S_AGAIN:
        ctrl_val = *(uint32_t *)arg;
        ESP_LOGD(TAG, "set ana gain 0x%" PRIx32, ctrl_val);
        ret = sc2336_write(dev->sccb_handle,
                           SC2336_REG_ANG_GAIN,
                           ctrl_val);
        break;
    case ESP_CAM_SENSOR_IOC_S_DGAIN:
        ctrl_val = *(uint32_t *)arg;
        ESP_LOGD(TAG, "set dig gain 0x%" PRIx32, ctrl_val);
        ret = sc2336_write(dev->sccb_handle,
                           SC2336_REG_DIG_COARSE_GAIN,
                           SC2336_FETCH_DGAIN_COARSE(ctrl_val));
        ret |= sc2336_write(dev->sccb_handle,
                            SC2336_REG_DIG_FINE_GAIN,
                            SC2336_FETCH_DGAIN_FINE(ctrl_val));
        break;
    case ESP_CAM_SENSOR_IOC_S_GAIN:
        ret = sc2336_set_gain(dev, (sc2336_gain_t *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = sc2336_get_sensor_id(dev, arg);
        break;
    default:
        break;
    }

    SC2336_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t sc2336_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        SC2336_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
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

static esp_err_t sc2336_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        SC2336_DISABLE_OUT_XCLK(dev->xclk_pin);
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

static esp_err_t sc2336_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del sc2336 (%p)", dev);
    if (dev) {
        free(dev);
        dev = NULL;
    }

    return ESP_OK;
}

static const esp_cam_sensor_ops_t sc2336_ops = {
    .query_para_desc = sc2336_query_para_desc,
    .get_para_value = sc2336_get_para_value,
    .set_para_value = sc2336_set_para_value,
    .query_support_formats = sc2336_query_support_formats,
    .query_support_capability = sc2336_query_support_capability,
    .set_format = sc2336_set_format,
    .get_format = sc2336_get_format,
    .priv_ioctl = sc2336_priv_ioctl,
    .del = sc2336_delete
};

esp_cam_sensor_device_t *sc2336_detect(esp_cam_sensor_config_t *config)
{
    esp_cam_sensor_device_t *dev = NULL;
    if (config == NULL) {
        return NULL;
    }

    dev = calloc(1, sizeof(esp_cam_sensor_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    dev->name = (char *)SC2336_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &sc2336_ops;
    if (config->sensor_port != ESP_CAM_SENSOR_DVP) {
        dev->cur_format = &sc2336_format_info[CONFIG_CAMERA_SC2336_MIPI_IF_FORMAT_INDEX_DAFAULT];
    } else {
        dev->cur_format = &sc2336_format_info[CONFIG_CAMERA_SC2336_DVP_IF_FORMAT_INDEX_DAFAULT];
    }

    // Configure sensor power, clock, and SCCB port
    if (sc2336_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (sc2336_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != SC2336_PID) {
        ESP_LOGE(TAG, "Camera sensor is not SC2336, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    sc2336_power_off(dev);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_SC2336_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(sc2336_detect, ESP_CAM_SENSOR_MIPI_CSI, SC2336_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return sc2336_detect(config);
}
#endif

#if CONFIG_CAMERA_SC2336_AUTO_DETECT_DVP_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(sc2336_detect, ESP_CAM_SENSOR_DVP, SC2336_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_DVP;
    return sc2336_detect(config);
}
#endif
