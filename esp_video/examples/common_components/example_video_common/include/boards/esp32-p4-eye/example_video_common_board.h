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
 * @brief ESP32-P4-EYE configuration
 */
#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
/**
 * @brief MIPI-CSI camera sensor configuration
 */
#define EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN               13
#define EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN               14
#define EXAMPLE_MIPI_CSI_CAM_SENSOR_RESET_PIN           26
#define EXAMPLE_MIPI_CSI_CAM_SENSOR_PWDN_PIN            12
#define EXAMPLE_MIPI_CSI_XCLK_PIN                       11
#define EXAMPLE_MIPI_CSI_XCLK_FREQ                      24000000

#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_MOTOR
#error "MIPI-CSI camera motor is not supported on ESP32-P4-EYE by default"
#endif /* CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_MOTOR */
#endif

#if CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR
#error "DVP interface camera sensor is not supported on ESP32-P4-EYE by default"
#endif /* CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR */

#if CONFIG_EXAMPLE_ENABLE_SPI_CAM_SENSOR
#error "SPI interface camera sensor is not supported on ESP32-P4-EYE by default"
#endif /* CONFIG_EXAMPLE_ENABLE_SPI_CAM_SENSOR */

#ifdef __cplusplus
}
#endif
