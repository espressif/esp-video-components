/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif


#ifdef CONFIG_ESP_VIDEO_LOG_ERROR
#define ESP_VIDEO_LOGE(_fmt, ...)   ESP_LOGE(TAG, "%s(%d): "_fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define ESP_VIDEO_LOGE(_fmt, ...)   if (TAG) { }
#endif

#ifdef CONFIG_ESP_VIDEO_LOG_INFO
#define ESP_VIDEO_LOGI(_fmt, ...)   ESP_LOGI(TAG, "%s(%d): "_fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define ESP_VIDEO_LOGI(_fmt, ...)   if (TAG) { }
#endif

#ifdef CONFIG_ESP_VIDEO_LOG_DEBUG
#define ESP_VIDEO_LOGD(_fmt, ...)   ESP_LOGD(TAG, "%s(%d): "_fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define ESP_VIDEO_LOGD(_fmt, ...)   if (TAG) { }
#endif

#ifdef __cplusplus
}
#endif
