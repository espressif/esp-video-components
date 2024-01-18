/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hal/mipi_csi_hal.h"
#include "hal/log.h"

#define MIPI_CSI_CLK_GROUP_DEFAULT (0)

static const char TAG[] = "mipi_csi_hal";

static void mipi_csi_hal_host_phy_init(mipi_csi_hal_context_t *hal, const mipi_csi_hal_config_t *config)
{
    int csi_lane_rate = config->mipi_clk;

    // Clear PHY from reset.
    mipi_csi_phy_ll_clear_from_reset(hal->host_dev);

    esp_rom_delay_us(1000);

    // Release D-PHY test codes from reset.
    mipi_csi_phy_ll_update_test_ctrl1(hal->host_dev, 0, 0);   // phy_testen  = 0; phy_testdin = 8'd0;
    mipi_csi_phy_ll_update_test_ctrl0(hal->host_dev, 0, 1);   // phy_testclk = 0; phy_testclr = 1;
    // Release phy_testclr.
    mipi_csi_phy_ll_update_test_ctrl0(hal->host_dev, 0, 0);   // phy_testclk = 0; phy_testclr = 0;

    // Config the D-PHY frequency range.
    uint8_t hs_freq = 0x1A;
    const struct {
        uint32_t freq;     // upper margin of frequency range
        uint8_t hs_freq;   // hsfreqrange
        uint8_t vco_range; // vcorange
    } pll_ranges[] = {
        {90, 0x00, 0x00},
        {100, 0x10, 0x00},
        {110, 0x20, 0x00},
        {130, 0x01, 0x00},
        {140, 0x11, 0x00},
        {150, 0x21, 0x00},
        {170, 0x02, 0x00},
        {180, 0x12, 0x00},
        {200, 0x22, 0x00},
        {220, 0x03, 0x01},
        {240, 0x13, 0x01},
        {250, 0x23, 0x01},
        {270, 0x04, 0x01},
        {300, 0x14, 0x01},
        {330, 0x05, 0x02},
        {360, 0x15, 0x02},
        {400, 0x25, 0x02},
        {450, 0x06, 0x02},
        {500, 0x16, 0x02},
        {550, 0x07, 0x03},
        {600, 0x17, 0x03},
        {650, 0x08, 0x03},
        {700, 0x18, 0x03},
        {750, 0x09, 0x04},
        {800, 0x19, 0x04},
        {850, 0x29, 0x04},
        {900, 0x39, 0x04},
        {950, 0x0A, 0x05},
        {1000, 0x1A, 0x05},
        {1050, 0x2A, 0x05},
        {1100, 0x3A, 0x05},
        {1150, 0x0B, 0x06},
        {1200, 0x1B, 0x06},
        {1250, 0x2B, 0x06},
        {1300, 0x3B, 0x06},
        {1350, 0x0C, 0x07},
        {1400, 0x1C, 0x07},
        {1450, 0x2C, 0x07},
        {1500, 0x3C, 0x07}
    };

    for (int x = 0; ; x++) {
        if (pll_ranges[x].freq == 1500) {
            hs_freq = pll_ranges[x].hs_freq;
            break;
        }

        if ((csi_lane_rate >= pll_ranges[x].freq * 1000000) && (csi_lane_rate < pll_ranges[x + 1].freq * 1000000)) {
            hs_freq = pll_ranges[x].hs_freq;
            break;
        }
    }

    // Configure D-PHY frequency range (Max is 1.5GHz).
    mipi_csi_phy_ll_write_control(hal->host_dev, 0x44, hs_freq << 1);
    HAL_LOGD(TAG, "CSI-DPHY lane_rate: %d Hz, hs_freq: 0x%x, lane_num: 0x%x", csi_lane_rate, hs_freq, config->lanes_num);
    mipi_csi_phy_ll_enable_shutdown_input(hal->host_dev, 0);
    mipi_csi_phy_ll_enable_reset_output(hal->host_dev, 0);
    mipi_csi_host_ll_enable_reset_output(hal->host_dev, 0);

    // Configure the host controller.
    // Configure the number of active lanes.
    mipi_csi_host_ll_set_active_lanes_num(hal->host_dev, config->lanes_num);

    // Configure VCX.
    mipi_csi_host_ll_enable_vc_channel_extension(hal->host_dev, 0x0);

    // Set Scrambler.
    mipi_csi_host_ll_enable_scrambling(hal->host_dev, 0x0);
}

static void mipi_csi_hal_brg_init(mipi_csi_hal_context_t *hal, const mipi_csi_hal_config_t *config)
{
    // Set Frame size.
    mipi_csi_brg_ll_set_intput_data_h_pixel_num(hal->bridge_dev, config->frame_width);
    mipi_csi_brg_ll_set_intput_data_v_row_num(hal->bridge_dev, config->frame_height);

    mipi_csi_brg_ll_set_flow_ctl_buf_afull_thrd(hal->bridge_dev, 960);

    mipi_csi_brg_ll_set_burst_len(hal->bridge_dev, 256);

    HAL_LOGD(TAG, "has_hsync_e: 0x%x", MIPI_CSI_BRIDGE.frame_cfg.has_hsync_e);

    HAL_LOGD(TAG, "csi data_type_min: 0x%x, data_type_max: 0x%x, dma_req_interval: %d", MIPI_CSI_BRIDGE.data_type_cfg.data_type_min, MIPI_CSI_BRIDGE.data_type_cfg.data_type_max, MIPI_CSI_BRIDGE.dma_req_interval.dma_req_interval);

    mipi_csi_brg_ll_set_data_type_min(hal->bridge_dev, 0x12);

    mipi_csi_brg_ll_set_byte_endian(hal->bridge_dev, 0x0);
    mipi_csi_brg_ll_set_bit_endian(hal->bridge_dev, 0x0);

    // Enable CSI Bridge.
    mipi_csi_brg_ll_enable(hal->bridge_dev, true);
}

void mipi_csi_hal_init(mipi_csi_hal_context_t *hal, const mipi_csi_hal_config_t *config)
{
    mipi_csi_hal_host_phy_init(hal, config);
    mipi_csi_hal_brg_init(hal, config);
}
