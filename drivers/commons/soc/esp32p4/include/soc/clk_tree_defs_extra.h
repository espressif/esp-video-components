/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "soc/soc_caps_extra.h"
#include "soc/clk_tree_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////CAM///////////////////////////////////////////////////////////////////

/**
 * @brief Array initializer for all supported clock sources of CAM
 */
#define SOC_CAM_CLKS {SOC_MOD_CLK_PLL_F160M, SOC_MOD_CLK_XTAL, SOC_MOD_CLK_APLL}

/**
 * @brief Type of CAM clock source
 */
typedef enum {
    CAM_CLK_SRC_PLL160M = SOC_MOD_CLK_PLL_F160M, /*!< Select PLL_F160M as the source clock */
    CAM_CLK_SRC_XTAL = SOC_MOD_CLK_XTAL,         /*!< Select XTAL as the source clock */
    CAM_CLK_SRC_APLL = SOC_MOD_CLK_APLL,         /*!< Select APLL as the source clock */
    CAM_CLK_SRC_DEFAULT = SOC_MOD_CLK_PLL_F160M, /*!< Select PLL_F160M as the default choice */
} soc_periph_cam_clk_src_t;

#if SOC_MIPI_CSI_SUPPORTED
/**
 * @brief Array initializer for all supported clock sources of MIPI CSI PHY interface
 */
#define SOC_MIPI_CSI_PHY_CLKS {SOC_MOD_CLK_RC_FAST, SOC_MOD_CLK_PLL_F25M, SOC_MOD_CLK_PLL_F20M}

/**
 * @brief Type of MIPI CSI PHY clock source
 */
typedef enum {
    MIPI_CSI_PHY_CLK_SRC_RC_FAST = SOC_MOD_CLK_RC_FAST,    /*!< Select RC_FAST as MIPI CSI PHY source clock */
    MIPI_CSI_PHY_CLK_SRC_PLL_F25M = SOC_MOD_CLK_PLL_F25M,  /*!< Select PLL_F25M as MIPI CSI PHY source clock */
    MIPI_CSI_PHY_CLK_SRC_PLL_F20M = SOC_MOD_CLK_PLL_F20M,  /*!< Select PLL_F20M as MIPI CSI PHY source clock */
    MIPI_CSI_PHY_CLK_SRC_DEFAULT = SOC_MOD_CLK_PLL_F25M,   /*!< Select PLL_F25M as default clock */
} soc_periph_mipi_csi_phy_clk_src_t;

#endif

#ifdef __cplusplus
}
#endif
