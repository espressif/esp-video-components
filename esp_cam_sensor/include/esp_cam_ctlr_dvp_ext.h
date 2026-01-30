/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "sdkconfig.h"
#include "esp_idf_version.h"
#include "esp_cam_ctlr_dvp.h"
#include "esp_private/esp_cam_dvp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief This LCD_CAM DVP driver is only available for ESP32-S3 with IDF version >= 5.5.2
 */
#if CONFIG_CAM_CTRL_DVP_ENABLE
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 2))
#define ESP_CAM_CTRL_DVP_ENABLE 1
#else /* (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 2)) */
#pragma message("LCD_CAM DVP driver is not available for ESP32-S3 with IDF version < 5.5.2")
#endif /* (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 2)) */
#endif /* CONFIG_CAM_CTRL_DVP_ENABLE */

/**
 * @brief New ESP CAM DVP controller
 *
 * @param config      DVP controller configurations
 * @param ret_handle  Returned CAM controller handle
 *
 * @return
 *        - ESP_OK on success
 *        - ESP_ERR_INVALID_ARG:   Invalid argument
 *        - ESP_ERR_NO_MEM:        Out of memory
 *        - ESP_ERR_NOT_SUPPORTED: Currently not support modes or types
 *        - ESP_ERR_NOT_FOUND:     DVP is registered already
 */
#if ESP_CAM_CTRL_DVP_ENABLE
esp_err_t esp_cam_new_dvp_ctlr_ext(const esp_cam_ctlr_dvp_config_t *config, esp_cam_ctlr_handle_t *ret_handle);
#else /* ESP_CAM_CTRL_DVP_ENABLE */
#define esp_cam_new_dvp_ctlr_ext(c, rh) esp_cam_new_dvp_ctlr(c, rh)
#endif /* ESP_CAM_CTRL_DVP_ENABLE */

/**
 * @brief ESP CAM DVP initialize clock and GPIO.
 *
 * @param ctlr_id CAM DVP controller ID
 * @param clk_src CAM DVP clock source
 * @param pin     CAM DVP pin configuration
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
#if ESP_CAM_CTRL_DVP_ENABLE
esp_err_t esp_cam_ctlr_dvp_init_ext(int ctlr_id, cam_clock_source_t clk_src, const esp_cam_ctlr_dvp_pin_config_t *pin);
#else /* ESP_CAM_CTRL_DVP_ENABLE */
#define esp_cam_ctlr_dvp_init_ext(c, s, p) esp_cam_ctlr_dvp_init(c, s, p)
#endif /* ESP_CAM_CTRL_DVP_ENABLE */

#ifdef __cplusplus
}
#endif
