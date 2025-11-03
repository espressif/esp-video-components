/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ISP video device configuration
 */
#if CONFIG_SOC_ISP_LSC_SUPPORTED && (CONFIG_ESP32P4_REV_MIN_FULL >= 100)
#define ESP_VIDEO_ISP_DEVICE_LSC    1       /*!< ISP video device enable LSC */
#endif

/**
 * @brief ISP video device configuration
 */
#if CONFIG_SOC_ISP_WBG_SUPPORTED
#define ESP_VIDEO_ISP_DEVICE_WBG    1       /*!< ISP video device enable WBG */
#define ESP_VIDEO_ISP_WBG_DEC_BITS  8       /*!< WBG Decimal part */
#endif


#ifdef __cplusplus
}
#endif
