/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_cam_sensor.h"
#include "driver/bitscrambler.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Video swap byte object
 */
typedef struct esp_video_swap_byte {
    bitscrambler_handle_t bs;               /*!< Bitscrambler handle */
} esp_video_swap_byte_t;

/**
 * @brief Create video swap byte
 *
 * @return Swapping byte pointer if success or NULL if failed
 */
esp_video_swap_byte_t *esp_video_swap_byte_create(void);

/**
 * @brief Start video swap byte
 *
 * @param swap_byte    Video swap byte object pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_swap_byte_start(esp_video_swap_byte_t *swap_byte);

/**
 * @brief Free video swap byte
 *
 * @param swap byte    Video swap byte object pointer
 *
 * @return None
 */
void esp_video_swap_byte_free(esp_video_swap_byte_t *swap_byte);

#ifdef __cplusplus
}
#endif
