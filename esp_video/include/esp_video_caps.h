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

#if CONFIG_IDF_TARGET_ESP32P4
/**
 * @brief ISP video device configuration
 */
#if CONFIG_ESP32P4_REV_MIN_FULL >= 100
#if CONFIG_SOC_ISP_LSC_SUPPORTED
#define ESP_VIDEO_ISP_DEVICE_LSC    1       /*!< ISP video device enable LSC */
#endif /* CONFIG_SOC_ISP_LSC_SUPPORTED */
#endif /* CONFIG_ESP32P4_REV_MIN_FULL >= 100 */

/**
 * @brief ISP video device configuration
 */
#if CONFIG_ESP32P4_REV_MIN_FULL >= 300
#if CONFIG_SOC_ISP_WBG_SUPPORTED
#define ESP_VIDEO_ISP_DEVICE_WBG    1       /*!< ISP video device enable WBG */
#define ESP_VIDEO_ISP_WBG_DEC_BITS  8       /*!< WBG Decimal part */
#endif /* CONFIG_SOC_ISP_WBG_SUPPORTED */

#if CONFIG_SOC_ISP_BLC_SUPPORTED
#define ESP_VIDEO_ISP_DEVICE_BLC    1       /*!< ISP video device enable BLC */
#endif /* CONFIG_SOC_ISP_BLC_SUPPORTED */

#if CONFIG_SOC_ISP_CROP_SUPPORTED
#define ESP_VIDEO_ISP_DEVICE_CROP   1       /*!< ISP video device enable crop */
#endif /* CONFIG_SOC_ISP_CROP_SUPPORTED */

#if CONFIG_SOC_JPEG_CODEC_SUPPORTED
#define ESP_VIDEO_JPEG_DEVICE_YUV420    1   /*!< JPEG video device supports YUV420 format */
#define ESP_VIDEO_JPEG_DEVICE_YUV444    1   /*!< JPEG video device supports YUV444 format */
#endif /* CONFIG_SOC_JPEG_CODEC_SUPPORTED */
#endif /* CONFIG_ESP32P4_REV_MIN_FULL >= 300 */
#endif /* CONFIG_IDF_TARGET_ESP32P4 */

#ifdef __cplusplus
}
#endif
