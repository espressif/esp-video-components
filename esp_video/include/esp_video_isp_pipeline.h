/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "sdkconfig.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER

/**
 * @brief AGC status.
 */
typedef enum esp_video_isp_pipeline_agc_status {
    ESP_VIDEO_ISP_PIPELINE_AGC_DISABLE  = 0, /**< AGC disabled */
    ESP_VIDEO_ISP_PIPELINE_AGC_ENABLE   = 1, /**< AGC enabled */
} esp_video_isp_pipeline_agc_status_t;

/**
 * @brief Set AGC status.
 *
 * @param status AGC status
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_isp_pipeline_set_agc_status(esp_video_isp_pipeline_agc_status_t status);

/**
 * @brief Get AGC status.
 *
 * @param status Pointer to store AGC status
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_isp_pipeline_get_agc_status(esp_video_isp_pipeline_agc_status_t *status);
#endif /* CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER */

#ifdef __cplusplus
}
#endif
