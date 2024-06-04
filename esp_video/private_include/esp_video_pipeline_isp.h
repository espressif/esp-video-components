/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ISP initialization configuration
 */
typedef struct esp_video_isp_config {
    const char *isp_dev;                /*!< ISP video device name */
    const char *cam_dev;                /*!< Camera interface video device name, such as "/dev/video0"(MIPI-CSI) */
    int ipa_nums;                       /*!< IPA numbers */
    const char **ipa_names;             /*!< IPA name array */
} esp_video_isp_config_t;

/**
 * @brief Initialize and start ISP system module.
 *
 * @param config ISP configuration
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_isp_pipeline_init(const esp_video_isp_config_t *config);

#ifdef __cplusplus
}
#endif
