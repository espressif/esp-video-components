/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "soc/clk_tree_defs_extra.h"

#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////CAM///////////////////////////////////////////////////////////////////

/**
 * @brief Array initializer for all supported clock sources of CAM
 */
#define SOC_CAM_CLKS {SOC_MOD_CLK_PLL_F160M, SOC_MOD_CLK_PLL_D2, SOC_MOD_CLK_XTAL}

/**
 * @brief Type of CAM clock source
 */
typedef enum {
    CAM_CLK_SRC_PLL160M = SOC_MOD_CLK_PLL_F160M, /*!< Select PLL_F160M as the source clock */
    CAM_CLK_SRC_PLL240M = SOC_MOD_CLK_PLL_D2,    /*!< Select PLL_D2 as the source clock */
    CAM_CLK_SRC_XTAL = SOC_MOD_CLK_XTAL,         /*!< Select XTAL as the source clock */
    CAM_CLK_SRC_DEFAULT = SOC_MOD_CLK_PLL_F160M, /*!< Select PLL_F160M as the default choice */
} soc_periph_cam_clk_src_t;

#ifdef __cplusplus
}
#endif
