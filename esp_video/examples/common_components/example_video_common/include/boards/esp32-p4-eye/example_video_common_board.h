/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
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

/**
 * @brief SCCB(I2C) pre-initialized port configuration
 */
#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
#define EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP            13
#define EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP            14
#endif /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */

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
#endif /* CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR */

#if CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR
#error "DVP interface camera sensor is not supported on ESP32-P4-EYE by default"
#endif /* CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR */

#if CONFIG_EXAMPLE_ENABLE_SPI_CAM_0_SENSOR
#error "SPI interface camera sensor is not supported on ESP32-P4-EYE by default"
#endif /* CONFIG_EXAMPLE_ENABLE_SPI_CAM_0_SENSOR */

/**
 * @brief SD/MMC configuration
 */
#define EXAMPLE_SDMMC_CMD_PIN                           44
#define EXAMPLE_SDMMC_CLK_PIN                           43
#define EXAMPLE_SDMMC_D0_PIN                            39

#if CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
#define EXAMPLE_SDMMC_BUS_WIDTH_4                       1
#define EXAMPLE_SDMMC_D1_PIN                            40
#define EXAMPLE_SDMMC_D2_PIN                            41
#define EXAMPLE_SDMMC_D3_PIN                            42
#else /* CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4 */
#define EXAMPLE_SDMMC_BUS_WIDTH_1                       1
#endif /* CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4 */

/**
 * @brief SD/MMC power control configuration
 */
#define EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO             1
#define EXAMPLE_SD_PWR_CTRL_LDO_IO_ID                   4

#ifdef __cplusplus
}
#endif
