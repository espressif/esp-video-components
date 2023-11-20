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
    uint32_t version;
    union {
        struct {
            uint32_t shutdownz                     :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } pwr_up;
    union {
        struct {
            uint32_t tx_esc_clk_division           :    8;
            uint32_t to_clk_division               :    8;
            uint32_t reserved16                    :    16;
        };
        uint32_t val;
    } clkmgr_cfg;
    union {
        struct {
            uint32_t dpi_vcid                      :    2;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } dpi_vcid;
    union {
        struct {
            uint32_t dpi_color_coding              :    4;
            uint32_t reserved4                     :    4;
            uint32_t loosely18_en                  :    1;
            uint32_t reserved9                     :    23;
        };
        uint32_t val;
    } dpi_color_coding;
    union {
        struct {
            uint32_t dataen_active_low             :    1;
            uint32_t vsync_active_low              :    1;
            uint32_t hsync_active_low              :    1;
            uint32_t shutd_active_low              :    1;
            uint32_t colorm_active_low             :    1;
            uint32_t reserved5                     :    27;
        };
        uint32_t val;
    } dpi_cfg_pol;
    union {
        struct {
            uint32_t invact_lpcmd_time             :    8;
            uint32_t reserved8                     :    8;
            uint32_t outvact_lpcmd_time            :    8;
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } dpi_lp_cmd_tim;
    union {
        struct {
            uint32_t dbi_vcid                      :    2;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } dbi_vcid;
    union {
        struct {
            uint32_t in_dbi_conf                   :    4;
            uint32_t reserved4                     :    4;
            uint32_t out_dbi_conf                  :    4;
            uint32_t reserved12                    :    4;
            uint32_t lut_size_conf                 :    2;
            uint32_t reserved18                    :    14;
        };
        uint32_t val;
    } dbi_cfg;
    union {
        struct {
            uint32_t partitioning_en               :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } dbi_partitioning_en;
    union {
        struct {
            uint32_t wr_cmd_size                   :    16;
            uint32_t allowed_cmd_size              :    16;
        };
        uint32_t val;
    } dbi_cmdsize;
    union {
        struct {
            uint32_t eotp_tx_en                    :    1;
            uint32_t eotp_rx_en                    :    1;
            uint32_t bta_en                        :    1;
            uint32_t ecc_rx_en                     :    1;
            uint32_t crc_rx_en                     :    1;
            uint32_t eotp_tx_lp_en                 :    1;
            uint32_t reserved6                     :    26;
        };
        uint32_t val;
    } pckhdl_cfg;
    union {
        struct {
            uint32_t gen_vcid_rx                   :    2;
            uint32_t reserved2                     :    6;
            uint32_t gen_vcid_tear_auto            :    2;
            uint32_t reserved10                    :    6;
            uint32_t gen_vcid_tx_auto              :    2;
            uint32_t reserved18                    :    14;
        };
        uint32_t val;
    } gen_vcid;
    union {
        struct {
            uint32_t cmd_video_mode                :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } mode_cfg;
    union {
        struct {
            uint32_t vid_mode_type                 :    2;
            uint32_t reserved2                     :    6;
            uint32_t lp_vsa_en                     :    1;
            uint32_t lp_vbp_en                     :    1;
            uint32_t lp_vfp_en                     :    1;
            uint32_t lp_vact_en                    :    1;
            uint32_t lp_hbp_en                     :    1;
            uint32_t lp_hfp_en                     :    1;
            uint32_t frame_bta_ack_en              :    1;
            uint32_t lp_cmd_en                     :    1;
            uint32_t vpg_en                        :    1;
            uint32_t reserved17                    :    3;
            uint32_t vpg_mode                      :    1;
            uint32_t reserved21                    :    3;
            uint32_t vpg_orientation               :    1;
            uint32_t reserved25                    :    7;
        };
        uint32_t val;
    } vid_mode_cfg;
    union {
        struct {
            uint32_t vid_pkt_size                  :    14;
            uint32_t reserved14                    :    18;
        };
        uint32_t val;
    } vid_pkt_size;
    union {
        struct {
            uint32_t vid_num_chunks                :    13;
            uint32_t reserved13                    :    19;
        };
        uint32_t val;
    } vid_num_chunks;
    union {
        struct {
            uint32_t vid_null_size                 :    13;
            uint32_t reserved13                    :    19;
        };
        uint32_t val;
    } vid_null_size;
    union {
        struct {
            uint32_t vid_hsa_time                  :    12;
            uint32_t reserved12                    :    20;
        };
        uint32_t val;
    } vid_hsa_time;
    union {
        struct {
            uint32_t vid_hbp_time                  :    12;
            uint32_t reserved12                    :    20;
        };
        uint32_t val;
    } vid_hbp_time;
    union {
        struct {
            uint32_t vid_hline_time                :    15;
            uint32_t reserved15                    :    17;
        };
        uint32_t val;
    } vid_hline_time;
    union {
        struct {
            uint32_t vsa_lines                     :    10;
            uint32_t reserved10                    :    22;
        };
        uint32_t val;
    } vid_vsa_lines;
    union {
        struct {
            uint32_t vbp_lines                     :    10;
            uint32_t reserved10                    :    22;
        };
        uint32_t val;
    } vid_vbp_lines;
    union {
        struct {
            uint32_t vfp_lines                     :    10;
            uint32_t reserved10                    :    22;
        };
        uint32_t val;
    } vid_vfp_lines;
    union {
        struct {
            uint32_t v_active_lines                :    14;
            uint32_t reserved14                    :    18;
        };
        uint32_t val;
    } vid_vactive_lines;
    union {
        struct {
            uint32_t edpi_allowed_cmd_size         :    16;
            uint32_t reserved16                    :    16;
        };
        uint32_t val;
    } edpi_cmd_size;
    union {
        struct {
            uint32_t tear_fx_en                    :    1;
            uint32_t ack_rqst_en                   :    1;
            uint32_t reserved2                     :    6;
            uint32_t gen_sw_0p_tx                  :    1;
            uint32_t gen_sw_1p_tx                  :    1;
            uint32_t gen_sw_2p_tx                  :    1;
            uint32_t gen_sr_0p_tx                  :    1;
            uint32_t gen_sr_1p_tx                  :    1;
            uint32_t gen_sr_2p_tx                  :    1;
            uint32_t gen_lw_tx                     :    1;
            uint32_t reserved15                    :    1;
            uint32_t dcs_sw_0p_tx                  :    1;
            uint32_t dcs_sw_1p_tx                  :    1;
            uint32_t dcs_sr_0p_tx                  :    1;
            uint32_t dcs_lw_tx                     :    1;
            uint32_t reserved20                    :    4;
            uint32_t max_rd_pkt_size               :    1;
            uint32_t reserved25                    :    7;
        };
        uint32_t val;
    } cmd_mode_cfg;
    union {
        struct {
            uint32_t gen_dt                        :    6;
            uint32_t gen_vc                        :    2;
            uint32_t gen_wc_lsbyte                 :    8;
            uint32_t gen_wc_msbyte                 :    8;
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } gen_hdr;
    union {
        struct {
            uint32_t gen_pld_b1                    :    8;
            uint32_t gen_pld_b2                    :    8;
            uint32_t gen_pld_b3                    :    8;
            uint32_t gen_pld_b4                    :    8;
        };
        uint32_t val;
    } gen_pld_data;
    union {
        struct {
            uint32_t gen_cmd_empty                 :    1;
            uint32_t gen_cmd_full                  :    1;
            uint32_t gen_pld_w_empty               :    1;
            uint32_t gen_pld_w_full                :    1;
            uint32_t gen_pld_r_empty               :    1;
            uint32_t gen_pld_r_full                :    1;
            uint32_t gen_rd_cmd_busy               :    1;
            uint32_t reserved7                     :    1;
            uint32_t reserved8                     :    1;
            uint32_t reserved9                     :    1;
            uint32_t reserved10                    :    1;
            uint32_t reserved11                    :    1;
            uint32_t reserved12                    :    1;
            uint32_t reserved13                    :    1;
            uint32_t reserved14                    :    1;
            uint32_t reserved15                    :    1;
            uint32_t gen_buff_cmd_empty            :    1;
            uint32_t gen_buff_cmd_full             :    1;
            uint32_t gen_buff_pld_empty            :    1;
            uint32_t gen_buff_pld_full             :    1;
            uint32_t reserved20                    :    4;
            uint32_t reserved24                    :    1;
            uint32_t reserved25                    :    1;
            uint32_t reserved26                    :    1;
            uint32_t reserved27                    :    1;
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } cmd_pkt_status;
    union {
        struct {
            uint32_t lprx_to_cnt                   :    16;
            uint32_t hstx_to_cnt                   :    16;
        };
        uint32_t val;
    } to_cnt_cfg;
    union {
        struct {
            uint32_t hs_rd_to_cnt                  :    16;
            uint32_t reserved16                    :    16;
        };
        uint32_t val;
    } hs_rd_to_cnt;
    union {
        struct {
            uint32_t lp_rd_to_cnt                  :    16;
            uint32_t reserved16                    :    16;
        };
        uint32_t val;
    } lp_rd_to_cnt;
    union {
        struct {
            uint32_t hs_wr_to_cnt                  :    16;
            uint32_t reserved16                    :    8;
            uint32_t reserved24                    :    1;
            uint32_t reserved25                    :    7;
        };
        uint32_t val;
    } hs_wr_to_cnt;
    union {
        struct {
            uint32_t lp_wr_to_cnt                  :    16;
            uint32_t reserved16                    :    16;
        };
        uint32_t val;
    } lp_wr_to_cnt;
    union {
        struct {
            uint32_t bta_to_cnt                    :    16;
            uint32_t reserved16                    :    16;
        };
        uint32_t val;
    } bta_to_cnt;
    union {
        struct {
            uint32_t mode_3d                       :    2;
            uint32_t format_3d                     :    2;
            uint32_t second_vsync                  :    1;
            uint32_t right_first                   :    1;
            uint32_t reserved6                     :    10;
            uint32_t send_3d_cfg                   :    1;
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } sdf_3d;
    union {
        struct {
            uint32_t phy_txrequestclkhs            :    1;
            uint32_t auto_clklane_ctrl             :    1;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } lpclk_ctrl;
    union {
        struct {
            uint32_t phy_clklp2hs_time             :    10;
            uint32_t reserved10                    :    6;
            uint32_t phy_clkhs2lp_time             :    10;
            uint32_t reserved26                    :    6;
        };
        uint32_t val;
    } phy_tmr_lpclk_cfg;
    union {
        struct {
            uint32_t phy_lp2hs_time                :    10;
            uint32_t reserved10                    :    6;
            uint32_t phy_hs2lp_time                :    10;
            uint32_t reserved26                    :    6;
        };
        uint32_t val;
    } phy_tmr_cfg;
    union {
        struct {
            uint32_t phy_shutdownz                 :    1;
            uint32_t phy_rstz                      :    1;
            uint32_t phy_enableclk                 :    1;
            uint32_t phy_forcepll                  :    1;
            uint32_t reserved4                     :    28;
        };
        uint32_t val;
    } phy_rstz;
    union {
        struct {
            uint32_t n_lanes                       :    2;
            uint32_t reserved2                     :    6;
            uint32_t phy_stop_wait_time            :    8;
            uint32_t reserved16                    :    16;
        };
        uint32_t val;
    } phy_if_cfg;
    union {
        struct {
            uint32_t phy_txrequlpsclk              :    1;
            uint32_t phy_txexitulpsclk             :    1;
            uint32_t phy_txrequlpslan              :    1;
            uint32_t phy_txexitulpslan             :    1;
            uint32_t reserved4                     :    28;
        };
        uint32_t val;
    } phy_ulps_ctrl;
    union {
        struct {
            uint32_t phy_tx_triggers               :    4;
            uint32_t reserved4                     :    28;
        };
        uint32_t val;
    } phy_tx_triggers;
    union {
        struct {
            uint32_t phy_lock                      :    1;
            uint32_t phy_direction                 :    1;
            uint32_t phy_stopstateclklane          :    1;
            uint32_t phy_ulpsactivenotclk          :    1;
            uint32_t phy_stopstate0lane            :    1;
            uint32_t phy_ulpsactivenot0lane        :    1;  /*change*/
            uint32_t phy_rxulpsesc0lane            :    1;
            uint32_t phy_stopstate1lane            :    1;
            uint32_t phy_ulpsactivenot1lane        :    1;  /*change*/
            uint32_t reserved9                     :    1;
            uint32_t reserved10                    :    1;
            uint32_t reserved11                    :    1;
            uint32_t reserved12                    :    1;
            uint32_t reserved13                    :    19;
        };
        uint32_t val;
    } phy_status;
    union {
        struct {
            uint32_t phy_testclr                   :    1;
            uint32_t phy_testclk                   :    1;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } phy_tst_ctrl0;
    union {
        struct {
            uint32_t phy_testdin                   :    8;
            uint32_t pht_testdout                  :    8;
            uint32_t phy_testen                    :    1;
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } phy_tst_ctrl1;
    union {
        struct {
            uint32_t ack_with_err_0                :    1;
            uint32_t ack_with_err_1                :    1;
            uint32_t ack_with_err_2                :    1;
            uint32_t ack_with_err_3                :    1;
            uint32_t ack_with_err_4                :    1;
            uint32_t ack_with_err_5                :    1;
            uint32_t ack_with_err_6                :    1;
            uint32_t ack_with_err_7                :    1;
            uint32_t ack_with_err_8                :    1;
            uint32_t ack_with_err_9                :    1;
            uint32_t ack_with_err_10               :    1;
            uint32_t ack_with_err_11               :    1;
            uint32_t ack_with_err_12               :    1;
            uint32_t ack_with_err_13               :    1;
            uint32_t ack_with_err_14               :    1;
            uint32_t ack_with_err_15               :    1;
            uint32_t dphy_errors_0                 :    1;
            uint32_t dphy_errors_1                 :    1;
            uint32_t dphy_errors_2                 :    1;
            uint32_t dphy_errors_3                 :    1;
            uint32_t dphy_errors_4                 :    1;
            uint32_t reserved21                    :    11;
        };
        uint32_t val;
    } int_st0;
    union {
        struct {
            uint32_t to_hs_tx                      :    1;
            uint32_t to_lp_rx                      :    1;
            uint32_t ecc_single_err                :    1;
            uint32_t ecc_milti_err                 :    1;
            uint32_t crc_err                       :    1;
            uint32_t pkt_size_err                  :    1;
            uint32_t eopt_err                      :    1;
            uint32_t dpi_pld_wr_err                :    1;
            uint32_t gen_cmd_wr_err                :    1;
            uint32_t gen_pld_wr_err                :    1;
            uint32_t gen_pld_send_err              :    1;
            uint32_t gen_pld_rd_err                :    1;
            uint32_t gen_pld_recev_err             :    1;
            uint32_t reserved13                    :    1;
            uint32_t reserved14                    :    1;
            uint32_t reserved15                    :    1;
            uint32_t reserved16                    :    1;
            uint32_t reserved17                    :    1;
            uint32_t reserved18                    :    1;
            uint32_t dpi_buff_pld_under            :    1;
            uint32_t reserved20                    :    1;
            uint32_t reserved21                    :    11;
        };
        uint32_t val;
    } int_st1;
    union {
        struct {
            uint32_t mask_ack_with_err_0           :    1;
            uint32_t mask_ack_with_err_1           :    1;
            uint32_t mask_ack_with_err_2           :    1;
            uint32_t mask_ack_with_err_3           :    1;
            uint32_t mask_ack_with_err_4           :    1;
            uint32_t mask_ack_with_err_5           :    1;
            uint32_t mask_ack_with_err_6           :    1;
            uint32_t mask_ack_with_err_7           :    1;
            uint32_t mask_ack_with_err_8           :    1;
            uint32_t mask_ack_with_err_9           :    1;
            uint32_t mask_ack_with_err_10          :    1;
            uint32_t mask_ack_with_err_11          :    1;
            uint32_t mask_ack_with_err_12          :    1;
            uint32_t mask_ack_with_err_13          :    1;
            uint32_t mask_ack_with_err_14          :    1;
            uint32_t mask_ack_with_err_15          :    1;
            uint32_t mask_dphy_errors_0            :    1;
            uint32_t mask_dphy_errors_1            :    1;
            uint32_t mask_dphy_errors_2            :    1;
            uint32_t mask_dphy_errors_3            :    1;
            uint32_t mask_dphy_errors_4            :    1;
            uint32_t reserved21                    :    11;
        };
        uint32_t val;
    } int_msk0;
    union {
        struct {
            uint32_t mask_to_hs_tx                 :    1;
            uint32_t mask_to_lp_rx                 :    1;
            uint32_t mask_ecc_single_err           :    1;
            uint32_t mask_ecc_milti_err            :    1;
            uint32_t mask_crc_err                  :    1;
            uint32_t mask_pkt_size_err             :    1;
            uint32_t mask_eopt_err                 :    1;
            uint32_t mask_dpi_pld_wr_err           :    1;
            uint32_t mask_gen_cmd_wr_err           :    1;
            uint32_t mask_gen_pld_wr_err           :    1;
            uint32_t mask_gen_pld_send_err         :    1;
            uint32_t mask_gen_pld_rd_err           :    1;
            uint32_t mask_gen_pld_recev_err        :    1;
            uint32_t reserved13                    :    1;
            uint32_t reserved14                    :    1;
            uint32_t reserved15                    :    1;
            uint32_t reserved16                    :    1;
            uint32_t reserved17                    :    1;
            uint32_t reserved18                    :    1;
            uint32_t mask_dpi_buff_pld_under       :    1;
            uint32_t reserved20                    :    1;
            uint32_t reserved21                    :    11;
        };
        uint32_t val;
    } int_msk1;
    union {
        struct {
            uint32_t txskewcalhs                   :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } phy_cal;
    uint32_t reserved_d0;
    uint32_t reserved_d4;
    union {
        struct {
            uint32_t force_ack_with_err_0          :    1;
            uint32_t force_ack_with_err_1          :    1;
            uint32_t force_ack_with_err_2          :    1;
            uint32_t force_ack_with_err_3          :    1;
            uint32_t force_ack_with_err_4          :    1;
            uint32_t force_ack_with_err_5          :    1;
            uint32_t force_ack_with_err_6          :    1;
            uint32_t force_ack_with_err_7          :    1;
            uint32_t force_ack_with_err_8          :    1;
            uint32_t force_ack_with_err_9          :    1;
            uint32_t force_ack_with_err_10         :    1;
            uint32_t force_ack_with_err_11         :    1;
            uint32_t force_ack_with_err_12         :    1;
            uint32_t force_ack_with_err_13         :    1;
            uint32_t force_ack_with_err_14         :    1;
            uint32_t force_ack_with_err_15         :    1;
            uint32_t force_dphy_errors_0           :    1;
            uint32_t force_dphy_errors_1           :    1;
            uint32_t force_dphy_errors_2           :    1;
            uint32_t force_dphy_errors_3           :    1;
            uint32_t force_dphy_errors_4           :    1;
            uint32_t reserved21                    :    11;
        };
        uint32_t val;
    } int_force0;
    union {
        struct {
            uint32_t force_to_hs_tx                :    1;
            uint32_t force_to_lp_rx                :    1;
            uint32_t force_ecc_single_err          :    1;
            uint32_t force_ecc_milti_err           :    1;
            uint32_t force_crc_err                 :    1;
            uint32_t force_pkt_size_err            :    1;
            uint32_t force_eopt_err                :    1;
            uint32_t force_dpi_pld_wr_err          :    1;
            uint32_t force_gen_cmd_wr_err          :    1;
            uint32_t force_gen_pld_wr_err          :    1;
            uint32_t force_gen_pld_send_err        :    1;
            uint32_t force_gen_pld_rd_err          :    1;
            uint32_t force_gen_pld_recev_err       :    1;
            uint32_t reserved13                    :    1;
            uint32_t reserved14                    :    1;
            uint32_t reserved15                    :    1;
            uint32_t reserved16                    :    1;
            uint32_t reserved17                    :    1;
            uint32_t reserved18                    :    1;
            uint32_t force_dpi_buff_pld_under      :    1;
            uint32_t reserved20                    :    1;
            uint32_t reserved21                    :    11;
        };
        uint32_t val;
    } int_force1;
    uint32_t reserved_e0;
    uint32_t reserved_e4;
    uint32_t reserved_e8;
    uint32_t reserved_ec;
    union {
        struct {
            uint32_t compression_mode              :    1;
            uint32_t reserved1                     :    7;
            uint32_t compress_algo                 :    2;
            uint32_t reserved10                    :    6;
            uint32_t pps_sel                       :    2;
            uint32_t reserved18                    :    14;
        };
        uint32_t val;
    } dsc_parameter;
    union {
        struct {
            uint32_t max_rd_time                   :    15;
            uint32_t reserved15                    :    17;
        };
        uint32_t val;
    } phy_tmr_rd_cfg;
    uint32_t reserved_f8;
    uint32_t reserved_fc;
    union {
        struct {
            uint32_t vid_shadow_en                 :    1;
            uint32_t reserved1                     :    7;
            uint32_t vid_shadow_req                :    1;
            uint32_t reserved9                     :    7;
            uint32_t vid_shadow_pin_req            :    1;
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } vid_shadow_ctrl;
    uint32_t reserved_104;
    uint32_t reserved_108;
    union {
        struct {
            uint32_t dpi_vcid                      :    2;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } dpi_vcid_act;
    union {
        struct {
            uint32_t dpi_color_coding              :    4;
            uint32_t reserved4                     :    4;
            uint32_t loosely18_en                  :    1;
            uint32_t reserved9                     :    23;
        };
        uint32_t val;
    } dpi_color_coding_act;
    uint32_t reserved_114;
    union {
        struct {
            uint32_t invact_lpcmd_time             :    8;
            uint32_t reserved8                     :    8;
            uint32_t outvact_lpcmd_time            :    8;
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } dpi_lp_cmd_tim_act;
    union {
        struct {
            uint32_t hw_tear_effect_on             :    1;
            uint32_t hw_tear_effect_gen            :    1;
            uint32_t reserved2                     :    2;
            uint32_t hw_set_scan_line              :    1;
            uint32_t reserved5                     :    11;
            uint32_t scan_line_parameter           :    16;
        };
        uint32_t val;
    } edpi_te_hw_cfg;
    uint32_t reserved_120;
    uint32_t reserved_124;
    uint32_t reserved_128;
    uint32_t reserved_12c;
    uint32_t reserved_130;
    uint32_t reserved_134;
    union {
        struct {
            uint32_t vid_mode_type                 :    2;
            uint32_t lp_vsa_en                     :    1;
            uint32_t lp_vbp_en                     :    1;
            uint32_t lp_vfp_en                     :    1;
            uint32_t lp_vact_en                    :    1;
            uint32_t lp_hbp_en                     :    1;
            uint32_t lp_hfp_en                     :    1;
            uint32_t frame_bta_ack_en              :    1;
            uint32_t lp_cmd_en                     :    1;
            uint32_t reserved10                    :    22;
        };
        uint32_t val;
    } vid_mode_cfg_act;
    union {
        struct {
            uint32_t vid_pkt_size                  :    14;
            uint32_t reserved14                    :    18;
        };
        uint32_t val;
    } vid_pkt_size_act;
    union {
        struct {
            uint32_t vid_num_chunks                :    13;
            uint32_t reserved13                    :    19;
        };
        uint32_t val;
    } vid_num_chunks_act;
    union {
        struct {
            uint32_t vid_null_size                 :    13;
            uint32_t reserved13                    :    19;
        };
        uint32_t val;
    } vid_null_size_act;
    union {
        struct {
            uint32_t vid_hsa_time                  :    12;
            uint32_t reserved12                    :    20;
        };
        uint32_t val;
    } vid_hsa_time_act;
    union {
        struct {
            uint32_t vid_hbp_time                  :    12;
            uint32_t reserved12                    :    20;
        };
        uint32_t val;
    } vid_hbp_time_act;
    union {
        struct {
            uint32_t vid_hline_time                :    15;
            uint32_t reserved15                    :    17;
        };
        uint32_t val;
    } vid_hline_time_act;
    union {
        struct {
            uint32_t vsa_lines                     :    10;
            uint32_t reserved10                    :    22;
        };
        uint32_t val;
    } vid_vsa_lines_act;
    union {
        struct {
            uint32_t vbp_lines                     :    10;
            uint32_t reserved10                    :    22;
        };
        uint32_t val;
    } vid_vbp_lines_act;
    union {
        struct {
            uint32_t vfp_lines                     :    10;
            uint32_t reserved10                    :    22;
        };
        uint32_t val;
    } vid_vfp_lines_act;
    union {
        struct {
            uint32_t v_active_lines                :    14;
            uint32_t reserved14                    :    18;
        };
        uint32_t val;
    } vid_vactive_lines_act;
    uint32_t reserved_164;
    union {
        struct {
            uint32_t dpi_cmd_w_empty               :    1;
            uint32_t dpi_cmd_w_full                :    1;
            uint32_t dpi_pld_w_empty               :    1;
            uint32_t dpi_pld_w_full                :    1;
            uint32_t reserved4                     :    1;
            uint32_t reserved5                     :    1;
            uint32_t reserved6                     :    1;
            uint32_t reserved7                     :    1;
            uint32_t reserved8                     :    8;
            uint32_t dpi_buff_pld_empty            :    1;
            uint32_t dpi_buff_pld_full             :    1;
            uint32_t reserved18                    :    2;
            uint32_t reserved20                    :    1;
            uint32_t reserved21                    :    1;
            uint32_t reserved22                    :    1;
            uint32_t reserved23                    :    1;
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } vid_pkt_status;
    uint32_t reserved_16c;
    uint32_t reserved_170;
    uint32_t reserved_174;
    uint32_t reserved_178;
    uint32_t reserved_17c;
    uint32_t reserved_180;
    uint32_t reserved_184;
    uint32_t reserved_188;
    uint32_t reserved_18c;
    union {
        struct {
            uint32_t mode_3d                       :    2;
            uint32_t format_3d                     :    2;
            uint32_t second_vsync                  :    1;
            uint32_t right_first                   :    1;
            uint32_t reserved6                     :    10;
            uint32_t send_3d_cfg                   :    1;
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } sdf_3d_act;
} mipi_dsi_host_dev_t;

extern mipi_dsi_host_dev_t MIPI_DSI_HOST;

#ifdef __cplusplus
}
#endif
