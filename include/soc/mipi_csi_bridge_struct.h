// Copyright 2017-2018 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef _SOC_MIPI_CSI_BRIDGE_STRUCT_H_
#define _SOC_MIPI_CSI_BRIDGE_STRUCT_H_

#include "inttypes.h"
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
            uint32_t csi_en                        :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } en;
    union {
        struct {
            uint32_t dma_burst_len                 :    12;
            uint32_t dma_cfg_upd_by_blk            :    1;  /*1: reg_dma_burst_len & reg_dma_burst_len will be updated by dma block finish 0: updated by frame*/
            uint32_t reserved13                    :    3;
            uint32_t dma_force_rd_status           :    1;
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } dma_req_cfg;
    union {
        struct {
            uint32_t csi_buf_afull_thrd            :    14;
            uint32_t reserved14                    :    2;
            uint32_t csi_buf_depth                 :    14;
            uint32_t reserved30                    :    2;
        };
        uint32_t val;
    } buf_flow_ctl;
    union {
        struct {
            uint32_t data_type_min                 :    6;
            uint32_t reserved6                     :    2;
            uint32_t data_type_max                 :    6;
            uint32_t reserved14                    :    18;
        };
        uint32_t val;
    } data_type_cfg;
    union {
        struct {
            uint32_t vadr_num                      :    12;
            uint32_t hadr_num                      :    12;
            uint32_t reserved24                    :    6;
            uint32_t has_hsync_e                   :    1;
            uint32_t vadr_num_check                :    1;
        };
        uint32_t val;
    } frame_cfg;
    union {
        struct {
            uint32_t yuv_endine_mode               :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } yuv_endine_mode;
    uint32_t reserved_1c;
    union {
        struct {
            uint32_t vadr_num_gt                   :    1;
            uint32_t vadr_num_lt                   :    1;
            uint32_t discard                       :    1;
            uint32_t csi_buf_overrun               :    1;
            uint32_t csi_async_fifo_ovf            :    1;
            uint32_t dma_cfg_has_updated           :    1;
            uint32_t reserved6                     :    26;
        };
        uint32_t val;
    } frame_int_raw;
    union {
        struct {
            uint32_t vadr_num_gt_real              :    1;
            uint32_t vadr_num_lt_real              :    1;
            uint32_t discard                       :    1;
            uint32_t csi_buf_overrun               :    1;
            uint32_t csi_async_fifo_ovf            :    1;
            uint32_t dma_cfg_has_updated           :    1;
            uint32_t reserved6                     :    26;
        };
        uint32_t val;
    } frame_int_clr;
    union {
        struct {
            uint32_t vadr_num_gt                   :    1;
            uint32_t vadr_num_lt                   :    1;
            uint32_t discard                       :    1;
            uint32_t csi_buf_overrun               :    1;
            uint32_t csi_async_fifo_ovf            :    1;
            uint32_t dma_cfg_has_updated           :    1;
            uint32_t reserved6                     :    26;
        };
        uint32_t val;
    } frame_int_st;
    union {
        struct {
            uint32_t vadr_num_gt                   :    1;
            uint32_t vadr_num_lt                   :    1;
            uint32_t discard                       :    1;
            uint32_t csi_buf_overrun               :    1;
            uint32_t csi_async_fifo_ovf            :    1;
            uint32_t dma_cfg_has_updated           :    1;
            uint32_t reserved6                     :    26;
        };
        uint32_t val;
    } frame_int_ena;
    union {
        struct {
            uint32_t dma_req_interval              :    16;
            uint32_t reserved16                    :    16;
        };
        uint32_t val;
    } dma_req_interval;
    union {
        struct {
            uint32_t dmablk_size                   :    13;  /*the number of reg_dma_burst_len in a block*/
            uint32_t reserved13                    :    19;
        };
        uint32_t val;
    } dmablk_size;
    union {
        struct {
            uint32_t csi_mem_aux_ctrl              :    14;
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
            uint32_t csi_enableclk                 :    1;
            uint32_t csi_cfg_clk_en                :    1;
            uint32_t loopbk_test_en                :    1;  /* for phy test by loopack dsi phy to csi phy*/
            uint32_t reserved3                     :    29;
        };
        uint32_t val;
    } host_ctrl;
    union {
        struct {
            uint32_t csi_bridge_mem_clk_force_on   :    1;  /*csi bridge memory clock gating force on*/
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } mem_ctrl;
} mipi_csi_bridge_dev_t;
extern mipi_csi_bridge_dev_t MIPI_CSI_BRIDGE;
#ifdef __cplusplus
}
#endif



#endif /*_SOC_MIPI_CSI_BRIDGE_STRUCT_H_ */
