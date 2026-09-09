/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_video_init.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Wrap esp_video_init_with_flags().
 */
esp_err_t esp_video_wrap_init_with_flags(const esp_video_init_config_t *config, uint32_t flags);

/**
 * @brief Wrap esp_video_deinit_with_flags().
 */
esp_err_t esp_video_wrap_deinit_with_flags(uint32_t flags);

/**
 * @brief Wrap esp_video_init().
 */
esp_err_t esp_video_wrap_init(const esp_video_init_config_t *config);

/**
 * @brief Wrap esp_video_deinit().
 */
esp_err_t esp_video_wrap_deinit(void);

#ifdef __cplusplus
}
#endif
