/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Customized development board configuration
 */
#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
/**
 * @brief MIPI-CSI camera sensor configuration
 */
#define EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN               CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN
#define EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN               CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN
#define EXAMPLE_MIPI_CSI_CAM_SENSOR_RESET_PIN           CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_RESET_PIN
#define EXAMPLE_MIPI_CSI_CAM_SENSOR_PWDN_PIN            CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_PWDN_PIN
#define EXAMPLE_MIPI_CSI_XCLK_PIN                       CONFIG_EXAMPLE_MIPI_CSI_XCLK_PIN
#if CONFIG_EXAMPLE_MIPI_CSI_XCLK_PIN >= 0
#define EXAMPLE_MIPI_CSI_XCLK_FREQ                      CONFIG_EXAMPLE_MIPI_CSI_XCLK_FREQ
#endif /* CONFIG_EXAMPLE_MIPI_CSI_XCLK_PIN >= 0 */
#endif /* CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR */

#if CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR
/**
 * @brief DVP camera sensor configuration
 */
#define EXAMPLE_DVP_SCCB_I2C_SCL_PIN                    CONFIG_EXAMPLE_DVP_SCCB_I2C_SCL_PIN
#define EXAMPLE_DVP_SCCB_I2C_SDA_PIN                    CONFIG_EXAMPLE_DVP_SCCB_I2C_SDA_PIN
#define EXAMPLE_DVP_CAM_SENSOR_RESET_PIN                CONFIG_EXAMPLE_DVP_CAM_SENSOR_RESET_PIN
#define EXAMPLE_DVP_CAM_SENSOR_PWDN_PIN                 CONFIG_EXAMPLE_DVP_CAM_SENSOR_PWDN_PIN

/**
 * @brief DVP interface configuration
 */
#define EXAMPLE_DVP_XCLK_PIN                            CONFIG_EXAMPLE_DVP_XCLK_PIN
#define EXAMPLE_DVP_PCLK_PIN                            CONFIG_EXAMPLE_DVP_PCLK_PIN
#define EXAMPLE_DVP_VSYNC_PIN                           CONFIG_EXAMPLE_DVP_VSYNC_PIN
#define EXAMPLE_DVP_DE_PIN                              CONFIG_EXAMPLE_DVP_DE_PIN
#define EXAMPLE_DVP_D0_PIN                              CONFIG_EXAMPLE_DVP_D0_PIN
#define EXAMPLE_DVP_D1_PIN                              CONFIG_EXAMPLE_DVP_D1_PIN
#define EXAMPLE_DVP_D2_PIN                              CONFIG_EXAMPLE_DVP_D2_PIN
#define EXAMPLE_DVP_D3_PIN                              CONFIG_EXAMPLE_DVP_D3_PIN
#define EXAMPLE_DVP_D4_PIN                              CONFIG_EXAMPLE_DVP_D4_PIN
#define EXAMPLE_DVP_D5_PIN                              CONFIG_EXAMPLE_DVP_D5_PIN
#define EXAMPLE_DVP_D6_PIN                              CONFIG_EXAMPLE_DVP_D6_PIN
#define EXAMPLE_DVP_D7_PIN                              CONFIG_EXAMPLE_DVP_D7_PIN
#endif /* CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR */

#ifdef __cplusplus
}
#endif
