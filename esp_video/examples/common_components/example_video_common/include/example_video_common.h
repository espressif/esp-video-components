/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "linux/videodev2.h"
#include "esp_video_device.h"
#include "esp_video_init.h"
#include "esp_video_ioctl.h"
#include "example_video_common_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MIPI-CSI camera sensor common configuration
 */
#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
#define EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR              1
#define EXAMPLE_MIPI_CSI_SCCB_I2C_PORT                  CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_PORT
#define EXAMPLE_MIPI_CSI_SCCB_I2C_FREQ                  CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_FREQ

#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_MOTOR
#define EXAMPLE_ENABLE_MIPI_CSI_CAM_MOTOR              1
#define EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_PORT       CONFIG_EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_PORT
#define EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_FREQ       CONFIG_EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_FREQ
#endif /* CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_MOTOR */
#endif /* CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR */

/**
 * @brief DVP camera sensor common configuration
 */
#if CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR
#define EXAMPLE_ENABLE_DVP_CAM_SENSOR                   1
#define EXAMPLE_DVP_SCCB_I2C_PORT                       CONFIG_EXAMPLE_DVP_SCCB_I2C_PORT
#define EXAMPLE_DVP_SCCB_I2C_FREQ                       CONFIG_EXAMPLE_DVP_SCCB_I2C_FREQ

/**
 * @brief DVP clock frequency configuration
 */
#define EXAMPLE_DVP_XCLK_FREQ                           CONFIG_EXAMPLE_DVP_XCLK_FREQ
#endif /* CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR */

/**
 * @brief Example camera device path configuration
 */
#if EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
#define EXAMPLE_CAM_DEV_PATH                            ESP_VIDEO_MIPI_CSI_DEVICE_NAME
#elif EXAMPLE_ENABLE_DVP_CAM_SENSOR
#define EXAMPLE_CAM_DEV_PATH                            ESP_VIDEO_DVP_DEVICE_NAME
#endif /* CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR */

/**
 * @brief Initialize the video system
 *
 * @return ESP_OK on success or other value on failure
 */
esp_err_t example_video_init(void);

/**
 * @brief Deinitialize the video system
 *
 * @return ESP_OK on success or other value on failure
 */
esp_err_t example_video_deinit(void);

#ifdef __cplusplus
}
#endif
