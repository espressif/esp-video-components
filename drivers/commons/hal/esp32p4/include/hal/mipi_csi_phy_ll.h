/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "hal/misc.h"
#include "hal/assert.h"
#include "soc/mipi_csi_host_struct.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Write register "CSI2_HOST_PHY_TEST_CTRL0"
 *
 * @note phy_testclr, This line needs an initial high pulse after power up for analog programmability default values to be preset.
 *
 * @param dev Pointer to the CSI Host controller register base address
 * @param phy_testclk Clock to capture testdin bus.
 * @param phy_testclr When active, performs vendor specific interface initialization.
 */
static inline void mipi_csi_phy_ll_update_test_ctrl0(csi_host_dev_t *dev, uint32_t phy_testclk, uint32_t phy_testclr)
{
    dev->phy_test_ctrl0.val = (0x00000000 | ((phy_testclk << 1) & 0x00000002) | ((phy_testclr << 0) & 0x00000001));
}

/**
 * @brief Write register "CSI2_HOST_PHY_TEST_CTRL1"
 *
 * @param dev Pointer to the CSI Host controller register base address
 * @param phy_testen When asserted high, it configures an address write operation on the falling edge of testclk.
 *                   When asserted low, it configures a data write operation on the rising edge of testclk.
 * @param phy_testdin Test interface 8-bit data input for programming internal registers and accessing test functionalities.
 */
static inline void mipi_csi_phy_ll_update_test_ctrl1(csi_host_dev_t *dev, uint32_t phy_testen, uint32_t phy_testdin)
{
    dev->phy_test_ctrl1.val = (0x00000000 | ((phy_testen << 16) & 0x00010000) | ((phy_testdin << 0) & 0x000000ff));
}

/**
 * @brief Write dphy control code
 *
 * @param dev Pointer to the CSI Host controller register base address
 * @param testcode
 * @param testwrite
 */
static inline void mipi_csi_phy_ll_write_control(csi_host_dev_t *dev, uint32_t testcode, uint32_t testwrite)
{
    dev->phy_test_ctrl1.val = 0x00010000 | testcode;
    dev->phy_test_ctrl0.val = 0x00000002;
    dev->phy_test_ctrl0.val = 0x00000000;
    dev->phy_test_ctrl1.val = 0x00000000 | testwrite;
    dev->phy_test_ctrl0.val = 0x00000002;
    dev->phy_test_ctrl0.val = 0x00000000;
}

/**
 * @brief Enable dphy reset output & host reset output
 *
 * @param dev Pointer to the CSI Host controller register base address
 */
static inline void mipi_csi_phy_ll_clear_from_reset(csi_host_dev_t *dev)
{
    dev->phy_shutdownz.phy_shutdownz = 0; // shutdown input, active low
    dev->dphy_rstz.dphy_rstz = 0; // phy reset output, active low
    dev->csi2_resetn.csi2_resetn = 0; // host reset output, active low
}

/**
 * @brief Enable dphy shutdown input
 *
 * @param dev Pointer to the CSI Host controller register base address
 * @param en True to enable, False to disable
 */
static inline void mipi_csi_phy_ll_enable_shutdown_input(csi_host_dev_t *dev, bool en)
{
    dev->phy_shutdownz.phy_shutdownz = !en; // shutdown input, active low
}

/**
 * @brief Enable dphy reset output
 *
 * @param dev Pointer to the CSI Host controller register base address
 * @param en True to enable, False to disable
 */
static inline void mipi_csi_phy_ll_enable_reset_output(csi_host_dev_t *dev, bool en)
{
    dev->dphy_rstz.dphy_rstz = !en; // phy reset output, active low
}

#ifdef __cplusplus
}
#endif
