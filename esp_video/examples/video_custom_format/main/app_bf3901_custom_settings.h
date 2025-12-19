/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include <stdint.h>
#include "esp_cam_sensor_types.h"
#include "bf3901_types.h"

#define BF3901_REG_DELAY               0xfe

#ifdef __cplusplus
extern "C" {
#endif

#define BF3901_FRAME_HEADER_SIZE 4
#define BF3901_LINE_HEADER_SIZE 6

#define G3LB5  0x05
#define RGB    0x04

#define YUV422_UYVY 0x02
#define YUV422_YUYV 0x00

static const uint8_t bf3901_frame_header_check[] = {0xff, 0xff, 0xff, 0x00};
static const uint8_t bf3901_line_header_check[] = {0xff, 0xff, 0xff, 0x40};

#if CONFIG_APP_CAMERA_BF3901_SPI_YUV422_120X160_10FPS || CONFIG_APP_CAMERA_BF3901_SPI_YUV422_YUYV_120X160_10FPS
static const esp_cam_sensor_spi_frame_info bf3901_frame_info_spi = {
    .frame_size = (120 * 2 + BF3901_LINE_HEADER_SIZE) * 160 + BF3901_FRAME_HEADER_SIZE,
    .line_size = (120 * 2 + BF3901_LINE_HEADER_SIZE),
    .frame_header_check = bf3901_frame_header_check,
    .line_header_check = bf3901_line_header_check,
    .frame_header_size = BF3901_FRAME_HEADER_SIZE,
    .line_header_size = BF3901_LINE_HEADER_SIZE,
    .frame_header_check_size = 4,
    .line_header_check_size = 4,
    .drop_frame_count = 1,
};

#if CONFIG_APP_CAMERA_BF3901_SPI_YUV422_120X160_10FPS
#include "app_bf3901_spi_1bit_24Minput_120x160_yuv422_uvyv_10fps.h"

static const esp_cam_sensor_format_t custom_format_info = {
    .name = "SPI_1bit_24Minput_YUV422_120x160_10fps",
    .format = ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY,
    .port = ESP_CAM_SENSOR_SPI,
    .xclk = 24000000,
    .width = 120,
    .height = 160,
    .regs = bf3901_spi_1bit_24Minput_120x160_yuv422_uvyv_10fps,
    .regs_size = ARRAY_SIZE(bf3901_spi_1bit_24Minput_120x160_yuv422_uvyv_10fps),
    .fps = 10,
    .isp_info = NULL,
    .spi_info = {
        .rx_lines = 1,
        .frame_info = &bf3901_frame_info_spi,
    },
    .reserved = NULL,
};
#endif

#if CONFIG_APP_CAMERA_BF3901_SPI_YUV422_YUYV_120X160_10FPS
#include "app_bf3901_spi_1bit_24Minput_120x160_yuv422_yuyv_10fps.h"

static const esp_cam_sensor_format_t custom_format_info = {
    .name = "SPI_1bit_24Minput_YUV422_YUYV_120x160_10fps",
    .format = ESP_CAM_SENSOR_PIXFORMAT_YUV422_YUYV,
    .port = ESP_CAM_SENSOR_SPI,
    .xclk = 24000000,
    .width = 120,
    .height = 160,
    .regs = bf3901_spi_1bit_24Minput_120x160_yuv422_yuyv_10fps,
    .regs_size = ARRAY_SIZE(bf3901_spi_1bit_24Minput_120x160_yuv422_yuyv_10fps),
    .fps = 10,
    .isp_info = NULL,
    .spi_info = {
        .rx_lines = 1,
        .frame_info = &bf3901_frame_info_spi,
    },
    .reserved = NULL,
};
#endif
#endif

#if CONFIG_APP_CAMERA_BF3901_SPI_RGB565_240X320_15FPS
static const esp_cam_sensor_spi_frame_info bf3901_frame_info_spi = {
    .frame_size = (240 * 2 + BF3901_LINE_HEADER_SIZE) * 320 + BF3901_FRAME_HEADER_SIZE,
    .line_size = (240 * 2 + BF3901_LINE_HEADER_SIZE),
    .frame_header_check = bf3901_frame_header_check,
    .line_header_check = bf3901_line_header_check,
    .frame_header_size = BF3901_FRAME_HEADER_SIZE,
    .line_header_size = BF3901_LINE_HEADER_SIZE,
    .frame_header_check_size = 4,
    .line_header_check_size = 4,
    .drop_frame_count = 1,
};

#include "app_bf3901_spi_1bit_24Minput_240x320_rgb565_le_15fps.h"

static const esp_cam_sensor_format_t custom_format_info = {
    .name = "SPI_1bit_24Minput_RGB565_LE_240x320_10fps",
    .format = ESP_CAM_SENSOR_PIXFORMAT_RGB565_LE,
    .port = ESP_CAM_SENSOR_SPI,
    .xclk = 24000000,
    .width = 240,
    .height = 320,
    .regs = bf3901_spi_1bit_24Minput_240x320_rgb565_le_10fps,
    .regs_size = ARRAY_SIZE(bf3901_spi_1bit_24Minput_240x320_rgb565_le_10fps),
    .fps = 10,
    .isp_info = NULL,
    .spi_info = {
        .rx_lines = 1,
        .frame_info = &bf3901_frame_info_spi,
    },
    .reserved = NULL,
};
#endif

#ifdef __cplusplus
}
#endif
