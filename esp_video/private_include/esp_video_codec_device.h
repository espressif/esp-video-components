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
 * @brief Create H.264 video device
 *
 * @param hw_codec true: hardware H.264, false: software H.264(has not supported)
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
#ifdef CONFIG_ESP_VIDEO_ENABLE_H264_VIDEO_DEVICE
esp_err_t esp_video_create_h264_video_device(bool hw_codec);
#endif

#ifdef __cplusplus
}
#endif
