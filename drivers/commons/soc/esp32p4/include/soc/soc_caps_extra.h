/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "soc/soc_caps.h"

/*-------------------------- CAM CAPS ----------------------------------------*/
#define SOC_CAM_SUPPORTED                    (1)  /*!< CAM is supported */
#define SOC_CAM_SUPPORT_RGB_YUV_CONV         (1)  /*!< Support color format conversion between RGB and YUV */

/*-------------------------- MIPI CSI CAPS ----------------------------------------*/
#define SOC_MIPI_CSI_SUPPORTED               (1)  /*!< MIPI_CSI is supported */