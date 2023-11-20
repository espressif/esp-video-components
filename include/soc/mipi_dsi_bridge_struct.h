/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef volatile struct {
    union {
        struct {
            uint32_t clk_en                        :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } clk_en;
    union {
        struct {
            uint32_t dsi_en                        :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } en;
    union {
        struct {
            uint32_t dma_burst_len                 :    12;
            uint32_t reserved12                    :    20;
        };
        uint32_t val;
    } dma_req_cfg;
    union {
        struct {
            uint32_t raw_num_total                 :    22;  /*number of total pix bits/64*/
            uint32_t unalign_64bit_en              :    1;  /*set if total pix bits not a multiple of 64bits*/
            uint32_t reserved23                    :    8;
            uint32_t raw_num_total_set             :    1;
        };
        uint32_t val;
    } raw_num_cfg;
    union {
        struct {
            uint32_t credit_thrd                   :    15;
            uint32_t reserved15                    :    1;
            uint32_t credit_burst_thrd             :    15;
            uint32_t credit_reset                  :    1;
        };
        uint32_t val;
    } raw_buf_credit_ctl;
    union {
        struct {
            uint32_t raw_buf_depth                 :    14;
            uint32_t reserved14                    :    18;
        };
        uint32_t val;
    } fifo_flow_status;
    union {
        struct {
            uint32_t raw_type                      :    4;
            uint32_t dpi_config                    :    2;
            uint32_t reserved6                     :    26;
        };
        uint32_t val;
    } pixel_type;
    union {
        struct {
            uint32_t dma_block_slot                :    10;
            uint32_t dma_block_interval            :    18;
            uint32_t raw_num_total_auto_reload     :    1;
            uint32_t dma_block_interval_en         :    1;
            uint32_t reserved30                    :    2;
        };
        uint32_t val;
    } dma_block_interval;
    union {
        struct {
            uint32_t dma_req_interval              :    16;
            uint32_t reserved16                    :    16;
        };
        uint32_t val;
    } dma_req_interval;
    union {
        struct {
            uint32_t dpishutdn                     :    1;
            uint32_t dpicolorm                     :    1;
            uint32_t dpiupdatecfg                  :    1;
            uint32_t reserved3                     :    29;
        };
        uint32_t val;
    } dpi_lcd_ctl;
    union {
        struct {
            uint32_t dpi_rsv_data                  :    30;
            uint32_t reserved30                    :    2;
        };
        uint32_t val;
    } dpi_rsv_dpi_data;
    uint32_t reserved_2c;
    union {
        struct {
            uint32_t vtotal                        :    12;
            uint32_t reserved12                    :    4;
            uint32_t vdisp                         :    12;
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } dpi_v_cfg0;
    union {
        struct {
            uint32_t vbank                         :    12;
            uint32_t reserved12                    :    4;
            uint32_t vsync                         :    12;
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } dpi_v_cfg1;
    union {
        struct {
            uint32_t htotal                        :    12;
            uint32_t reserved12                    :    4;
            uint32_t hdisp                         :    12;
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } dpi_h_cfg0;
    union {
        struct {
            uint32_t hbank                         :    12;
            uint32_t reserved12                    :    4;
            uint32_t hsync                         :    12;
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } dpi_h_cfg1;
    union {
        struct {
            uint32_t dpi_en                        :    1;
            uint32_t reserved1                     :    3;
            uint32_t fifo_underrun_discard_vcnt    :    12;
            uint32_t reserved16                    :    16;
        };
        uint32_t val;
    } dpi_misc_config;
    union {
        struct {
            uint32_t dpi_config_update             :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } dpi_config_update;
    uint32_t reserved_48;
    uint32_t reserved_4c;
    union {
        struct {
            uint32_t dpi_underrun                  :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } int_ena;
    union {
        struct {
            uint32_t dpi_underrun                  :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } int_clr;
    union {
        struct {
            uint32_t dpi_underrun                  :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } int_raw;
    union {
        struct {
            uint32_t dpi_underrun                  :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } int_st;
    union {
        struct {
            uint32_t bistok                        :    1;
            uint32_t biston                        :    1;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } host_bist_ctl;
    union {
        struct {
            uint32_t tx_trigger_rev_en             :    1;
            uint32_t rx_trigger_rev_en             :    1;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } host_trigger_rev;
    union {
        struct {
            uint32_t blk_raw_num_total             :    22;  /*number of total pix bits/64*/
            uint32_t reserved22                    :    9;
            uint32_t blk_raw_num_total_set         :    1;
        };
        uint32_t val;
    } blk_raw_num_cfg;
    union {
        struct {
            uint32_t dma_frame_slot                :    10;
            uint32_t dma_frame_interval            :    18;
            uint32_t dma_multiblk_en               :    1;
            uint32_t dma_frame_interval_en         :    1;
            uint32_t reserved30                    :    2;
        };
        uint32_t val;
    } dma_frame_interval;
    union {
        struct {
            uint32_t dsi_mem_aux_ctrl              :    14;
            uint32_t reserved14                    :    18;
        };
        uint32_t val;
    } mem_aux_ctrl;
    union {
        struct {
            uint32_t rdn_eco_en                    :    1;
            uint32_t rdn_eco_result                :    1;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } rdn_eco_cs;
    uint32_t rdn_eco_low;
    uint32_t rdn_eco_high;
    union {
        struct {
            uint32_t dsi_cfg_ref_clk_en            :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } host_ctrl;
    union {
        struct {
            uint32_t dsi_bridge_mem_clk_force_on   :    1;
            uint32_t dsi_mem_clk_force_on          :    1;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } mem_clk_ctrl;
    union {
        struct {
            uint32_t dsi_dma_flow_controller       :    1;  /*0: dmac as flow ccontroller, 1:dsi_bridge as flow controller*/
            uint32_t reserved1                     :    3;
            uint32_t dma_flow_multiblk_num         :    4;  /*num of blocks when dmac as flow controller*/
            uint32_t reserved8                     :    24;
        };
        uint32_t val;
    } dma_flow_ctrl;
    union {
        struct {
            uint32_t dsi_raw_buf_almost_empty_thrd :    11;
            uint32_t reserved11                    :    21;
        };
        uint32_t val;
    } raw_buf_almost_empty_thrd;
} mipi_dsi_bridge_dev_t;

extern mipi_dsi_bridge_dev_t MIPI_DSI_BRIDGE;

#ifdef __cplusplus
}
#endif
