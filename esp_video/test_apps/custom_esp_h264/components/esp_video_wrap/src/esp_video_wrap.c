/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_video_wrap.h"

esp_err_t esp_video_wrap_init_with_flags(const esp_video_init_config_t *config, uint32_t flags)
{
    return esp_video_init_with_flags(config, flags);
}

esp_err_t esp_video_wrap_deinit_with_flags(uint32_t flags)
{
    return esp_video_deinit_with_flags(flags);
}

esp_err_t esp_video_wrap_init(const esp_video_init_config_t *config)
{
    return esp_video_init(config);
}

esp_err_t esp_video_wrap_deinit(void)
{
    return esp_video_deinit();
}
