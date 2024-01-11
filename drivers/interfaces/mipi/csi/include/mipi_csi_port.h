/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "sys/time.h"
#include "driver/ledc.h"
#include "esp_color_formats.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief MIPI-CSI lane num
 */
typedef enum {
    LANE_NUM_1 = 1,
    LANE_NUM_2,
    LANE_NUM_MAX
} mipi_csi_lane_num_t;

/**
* @brief Configuration structure for camera initialization
*/
typedef struct {
    uint32_t frame_width;
    uint32_t frame_height;
    mipi_csi_lane_num_t lane_num;       /*!< Lane num, 1~(LANE_NUM_MAX - 1)*/
    int mipi_clk_freq_hz;               /*!< Frequency of MIPI CLK, in Hz. CSI-Host <-> ISP <-> CSI-Bridge*/
    // mipi_csi_pin_config;             /*!< For P4, the pin is fixed*/
    pixformat_t in_format;                /*!< Format of esp32 received data from the sensor, mainly for sensor config */
    pixformat_t out_format;               /*!< Format of esp32 bridge output data, ISP module can convert input format to this format */
    bool isp_enable;
} mipi_csi_port_config_t;

#ifdef __cplusplus
}
#endif
