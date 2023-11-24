/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// The LL layer for MIPI-CSI register operations

#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include "esp_attr.h"
#include "hal/misc.h"
#include "hal/assert.h"
#include "soc/mipi_csi_bridge_struct.h"
#include "soc/mipi_csi_host_struct.h"

#ifdef __cplusplus
extern "C" {
#endif

// CSI Bridge
#define MIPI_CSI_BRIDGE_LL_GET_HW(num) (((num) == 0) ? (&MIPI_CSI_BRIDGE) : NULL)

// CSI Host = Host_Controler + D-PHY
#define MIPI_CSI_HOST_LL_GET_HW(num)   (((num) == 0) ? (&MIPI_CSI_HOST) : NULL)

/* --------------------------------------------------------
 * @brief  : Write register "CSI2_HOST_PHY_TEST_CTRL0".
 * @params : <phy_testclk>
 *           <phy_testclr>
 * @ret    : void
 **/
static inline void mipi_csi_ll_update_phy_test_ctrl0(mipi_csi_host_dev_t *dev, uint32_t phy_testclk, uint32_t phy_testclr)
{
    dev->phy_test_ctrl0.val = (0x00000000 | ((phy_testclk << 1) & 0x00000002) | ((phy_testclr << 0) & 0x00000001));
    // MIPI_CSI_HOST.phy_test_ctrl0.val = (0x00000000 | ((phy_testclk << 1) & 0x00000002) | ((phy_testclr << 0) & 0x00000001));
}

/* --------------------------------------------------------
 * @brief  : Write register "CSI2_HOST_PHY_TEST_CTRL1".
 * @params : <phy_testen>
 *           <phy_testdin>
 * @ret    : void
 **/
static inline void mipi_csi_ll_update_phy_test_ctrl1(mipi_csi_host_dev_t *dev, uint32_t phy_testen, uint32_t phy_testdin)
{
    dev->phy_test_ctrl1.val = (0x00000000 | ((phy_testen << 16) & 0x00010000) | ((phy_testdin << 0) & 0x000000ff));
    // MIPI_CSI_HOST.phy_test_ctrl1.val = (0x00000000 | ((phy_testen << 16) & 0x00010000) | ((phy_testdin << 0) & 0x000000ff));
}

static inline void mipi_csi_ll_dphy_write_control(mipi_csi_host_dev_t *dev, uint32_t testcode, uint32_t testwrite)
{
    dev->phy_test_ctrl1.val = 0x00010000 | testcode;
    dev->phy_test_ctrl0.val = 0x00000002;
    dev->phy_test_ctrl0.val = 0x00000000;
    dev->phy_test_ctrl1.val = 0x00000000 | testwrite;
    dev->phy_test_ctrl0.val = 0x00000002;
    dev->phy_test_ctrl0.val = 0x00000000;
}

static inline void mipi_csi_ll_clear_phy_from_reset(mipi_csi_host_dev_t *dev)
{
    dev->phy_shutdownz.phy_shutdownz = 0; // shutdown input, active low
    dev->dphy_rstz.dphy_rstz = 0; // phy reset output, active low
    dev->csi2_resetn.csi2_resetn = 0; // host reset output, active low
}

static inline void mipi_csi_ll_shutdown_input(mipi_csi_host_dev_t *dev, bool shutdown_en)
{
    dev->phy_shutdownz.phy_shutdownz = !shutdown_en; // shutdown input, active low
}

static inline void mipi_csi_ll_phy_reset_output(mipi_csi_host_dev_t *dev, bool reset_en)
{
    dev->dphy_rstz.dphy_rstz = !reset_en; // phy reset output, active low
}

static inline void mipi_csi_ll_host_reset_output(mipi_csi_host_dev_t *dev, bool reset_en)
{
    dev->csi2_resetn.csi2_resetn = !reset_en; // host reset output, active low
}

static inline void mipi_csi_ll_set_active_lanes_num(mipi_csi_host_dev_t *dev, int lanes_num)
{
    dev->n_lanes.val = lanes_num - 1;
}

static inline void mipi_csi_ll_set_vc_channel_extension(mipi_csi_host_dev_t *dev, bool en)
{
    dev->vc_extension.val = !en; // 0 is enable
}

// enable data de-scrambling on the controller side
static inline void mipi_csi_ll_set_scrambling(mipi_csi_host_dev_t *dev, bool en)
{
    dev->scrambling.val = en; // 0 is enable
}

static inline void mipi_csi_ll_bridge_set_frame_size(mipi_csi_bridge_dev_t *dev, size_t frame_width, size_t frame_height)
{
    dev->frame_cfg.hadr_num = frame_width;
    dev->frame_cfg.vadr_num = frame_height;
}

static inline void mipi_csi_ll_bridge_set_flow_ctl_buf_afull_thrd(mipi_csi_bridge_dev_t *dev, size_t afull_thrd)
{
    dev->buf_flow_ctl.csi_buf_afull_thrd = afull_thrd;
}

static inline void mipi_csi_ll_bridge_set_req_dma_burst_len(mipi_csi_bridge_dev_t *dev, size_t len)
{
    dev->dma_req_cfg.dma_burst_len = len;
}

static inline void mipi_csi_ll_bridge_set_has_hsync(mipi_csi_bridge_dev_t *dev, bool en)
{
    dev->frame_cfg.has_hsync_e = en;
}

static inline void mipi_csi_ll_bridge_set_data_type_min(mipi_csi_bridge_dev_t *dev, uint16_t type_min)
{
    dev->data_type_cfg.data_type_min = type_min;
}

static inline void mipi_csi_ll_bridge_set_data_type_max(mipi_csi_bridge_dev_t *dev, uint16_t type_max)
{
    dev->data_type_cfg.data_type_max = type_max;
}

static inline void mipi_csi_ll_bridge_set_dma_req_interval(mipi_csi_bridge_dev_t *dev, uint16_t interval)
{
    dev->dma_req_interval.dma_req_interval = interval;
}

static inline void mipi_csi_ll_bridge_set_yuv_endine_mode(mipi_csi_bridge_dev_t *dev, bool mode)
{
    dev->yuv_endine_mode.yuv_endine_mode = mode;
}

static inline void mipi_csi_ll_bridge_enable(mipi_csi_bridge_dev_t *dev, bool en)
{
    dev->en.val = en;
}

#ifdef __cplusplus
}
#endif