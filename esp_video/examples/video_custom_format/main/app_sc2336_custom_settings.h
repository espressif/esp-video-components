/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include <stdint.h>
#include "esp_cam_sensor_types.h"
#include "sc2336_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_APP_CAMERA_SC2336_MIPI_RAW8_800X800_30FPS
/* If you want to use the baseboard's ISP, provide ISP info */
static const esp_cam_sensor_isp_info_t custom_fmt_isp_info = {
    .isp_v1_info = {
        .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
        .pclk = 84000000,
        .vts = 1250,
        .hts = 2240,
        .gain_def = 0,
        .exp_def = 0x4dc,
        .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
    }
};

#include "app_sc2336_mipi_2lane_24Minput_800x800_raw8_30fps.h"

/*Provides the description of the initializer list.
 *Note that the description of the format must be `static const` type */
static const esp_cam_sensor_format_t custom_format_info = {
    .name = "MIPI_2lane_24Minput_RAW8_800x800_30fps",
    .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
    .port = ESP_CAM_SENSOR_MIPI_CSI,
    .xclk = 24000000,
    .width = 800,
    .height = 800,
    .regs = sc2336_mipi_2lane_24Minput_800x800_raw8_30fps,
    .regs_size = ARRAY_SIZE(sc2336_mipi_2lane_24Minput_800x800_raw8_30fps),
    .fps = 30,
    .isp_info = &custom_fmt_isp_info,
    .mipi_info = {
        .mipi_clk = 336000000,
        .lane_num = 2,
        .line_sync_en = false,
    },
    .reserved = NULL,
};

#endif

#ifdef __cplusplus
}
#endif
