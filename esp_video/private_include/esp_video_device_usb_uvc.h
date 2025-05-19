/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "esp_err.h"

esp_err_t esp_video_install_usb_uvc_driver(size_t task_stack, unsigned task_priority, int task_affinity);
esp_err_t esp_video_uninstall_usb_uvc_driver(void);
