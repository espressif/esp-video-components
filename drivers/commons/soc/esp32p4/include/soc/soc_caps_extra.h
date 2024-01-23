/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "soc/soc_caps.h"

/*-------------------------- CAM CAPS ----------------------------------------*/
#define SOC_LCDCAM_SUPPORTED                 (1)
#define SOC_CAM_SUPPORT_RGB_YUV_CONV         (1)  /*!< Support color format conversion between RGB and YUV */
#define SOC_CAM_PERIPH_NUM                   (1)
#define SOC_CAM_DATA_WIDTH                   (16)
