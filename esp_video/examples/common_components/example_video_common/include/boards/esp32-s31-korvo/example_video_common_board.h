/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ESP32-S31-Korvo configuration
 */

/**
 * @brief SCCB(I2C) pre-initialized port configuration
 */
#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
#define EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP            1
#define EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP            0
#endif /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */

#if CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR
/**
 * @brief DVP camera sensor configuration
 */
#define EXAMPLE_DVP_SCCB_I2C_SCL_PIN                    1
#define EXAMPLE_DVP_SCCB_I2C_SDA_PIN                    0
#define EXAMPLE_DVP_CAM_SENSOR_RESET_PIN                -1
#define EXAMPLE_DVP_CAM_SENSOR_PWDN_PIN                 -1

/**
 * @brief DVP interface configuration
 */
#define EXAMPLE_DVP_XCLK_PIN                            55
#define EXAMPLE_DVP_PCLK_PIN                            54
#define EXAMPLE_DVP_VSYNC_PIN                           56
#define EXAMPLE_DVP_DE_PIN                              57
#define EXAMPLE_DVP_D0_PIN                              46
#define EXAMPLE_DVP_D1_PIN                              47
#define EXAMPLE_DVP_D2_PIN                              48
#define EXAMPLE_DVP_D3_PIN                              49
#define EXAMPLE_DVP_D4_PIN                              50
#define EXAMPLE_DVP_D5_PIN                              51
#define EXAMPLE_DVP_D6_PIN                              52
#define EXAMPLE_DVP_D7_PIN                              53
#endif /* CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR */

#if CONFIG_EXAMPLE_ENABLE_SPI_CAM_0_SENSOR
/**
 * @brief SPI camera sensor configuration
 */
#define EXAMPLE_SPI_CAM_0_SCCB_I2C_SCL_PIN              1
#define EXAMPLE_SPI_CAM_0_SCCB_I2C_SDA_PIN              0
#define EXAMPLE_SPI_CAM_0_SENSOR_RESET_PIN              -1
#define EXAMPLE_SPI_CAM_0_SENSOR_PWDN_PIN               -1

/**
 * @brief SPI camera sensor configuration
 */
#define EXAMPLE_SPI_CAM_0_CS_PIN                        56
#define EXAMPLE_SPI_CAM_0_SCLK_PIN                      54
#ifdef CONFIG_CAMERA_BF3901
#define EXAMPLE_SPI_CAM_0_DATA0_IO_PIN                  53
#else
#define EXAMPLE_SPI_CAM_0_DATA0_IO_PIN                  46
#endif
#define EXAMPLE_SPI_CAM_0_DATA1_IO_PIN                  -1

/**
 * @brief SPI camera sensor clock resource configuration
 */
#define EXAMPLE_SPI_CAM_0_XCLK_PIN                      55
#endif /* CONFIG_EXAMPLE_ENABLE_SPI_CAM_0_SENSOR */

/**
 * @brief SD/MMC configuration
 */
#if CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
#define EXAMPLE_SDMMC_BUS_WIDTH_4                       4
#define EXAMPLE_SDMMC_D1_PIN                            21
#define EXAMPLE_SDMMC_D2_PIN                            22
#define EXAMPLE_SDMMC_D3_PIN                            23
#else /* CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4 */
#define EXAMPLE_SDMMC_BUS_WIDTH_1                       1
#endif /* CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4 */

#define EXAMPLE_SDMMC_CMD_PIN                           25
#define EXAMPLE_SDMMC_CLK_PIN                           24
#define EXAMPLE_SDMMC_D0_PIN                            20

#ifdef __cplusplus
}
#endif
