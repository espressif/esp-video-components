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
#ifndef _SOC_GDMA_STRUCT_H_
#define _SOC_GDMA_STRUCT_H_


#ifdef __cplusplus
extern "C" {
#endif

typedef volatile struct {
    uint32_t id0;
    uint32_t id1;
    uint32_t compver0;
    uint32_t compver1;
    union {
        struct {
            uint32_t dmac_en                       :    1;
            uint32_t int_en                        :    1;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } cfg0;
    uint32_t cfg1;
    union {
        struct {
            uint32_t ch1_en                        :    1;
            uint32_t ch2_en                        :    1;
            uint32_t ch3_en                        :    1;
            uint32_t ch4_en                        :    1;
            uint32_t reserved4                     :    4;
            uint32_t ch1_en_we                     :    1;
            uint32_t ch2_en_we                     :    1;
            uint32_t ch3_en_we                     :    1;
            uint32_t ch4_en_we                     :    1;
            uint32_t reserved12                    :    4;
            uint32_t ch1_susp                      :    1;
            uint32_t ch2_susp                      :    1;
            uint32_t ch3_susp                      :    1;
            uint32_t ch4_susp                      :    1;
            uint32_t reserved20                    :    4;
            uint32_t ch1_susp_we                   :    1;
            uint32_t ch2_susp_we                   :    1;
            uint32_t ch3_susp_we                   :    1;
            uint32_t ch4_susp_we                   :    1;
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } chen0;
    union {
        struct {
            uint32_t ch1_abort                     :    1;
            uint32_t ch2_abort                     :    1;
            uint32_t ch3_abort                     :    1;
            uint32_t ch4_abort                     :    1;
            uint32_t reserved4                     :    4;
            uint32_t ch1_abort_we                  :    1;
            uint32_t ch2_abort_we                  :    1;
            uint32_t ch3_abort_we                  :    1;
            uint32_t ch4_abort_we                  :    1;
            uint32_t reserved12                    :    4;
            uint32_t reserved16                    :    16;
        };
        uint32_t val;
    } chen1;
    uint32_t reserved_20;
    uint32_t reserved_24;
    uint32_t reserved_28;
    uint32_t reserved_2c;
    union {
        struct {
            uint32_t ch1                           :    1;
            uint32_t ch2                           :    1;
            uint32_t ch3                           :    1;
            uint32_t ch4                           :    1;
            uint32_t reserved4                     :    4;
            uint32_t reserved8                     :    8;
            uint32_t commonreg                     :    1;
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } int_st0;
    uint32_t int_st1;
    union {
        struct {
            uint32_t slvif_commonreg_dec_err       :    1;
            uint32_t slvif_commonreg_wr2ro_err     :    1;
            uint32_t slvif_commonreg_rd2wo_err     :    1;
            uint32_t slvif_commonreg_wronhold_err  :    1;
            uint32_t reserved4                     :    3;
            uint32_t slvif_commonreg_wrparity_err  :    1;
            uint32_t slvif_undefinedreg_dec_err    :    1;
            uint32_t mxif1_rch0_eccprot_correrr    :    1;
            uint32_t mxif1_rch0_eccprot_uncorrerr  :    1;
            uint32_t mxif1_rch1_eccprot_correrr    :    1;
            uint32_t mxif1_rch1_eccprot_uncorrerr  :    1;
            uint32_t mxif1_bch_eccprot_correrr     :    1;
            uint32_t mxif1_bch_eccprot_uncorrerr   :    1;
            uint32_t mxif2_rch0_eccprot_correrr    :    1;
            uint32_t mxif2_rch0_eccprot_uncorrerr  :    1;
            uint32_t mxif2_rch1_eccprot_correrr    :    1;
            uint32_t mxif2_rch1_eccprot_uncorrerr  :    1;
            uint32_t mxif2_bch_eccprot_correrr     :    1;
            uint32_t mxif2_bch_eccprot_uncorrerr   :    1;
            uint32_t reserved21                    :    11;
        };
        uint32_t val;
    } common_int_clr0;
    uint32_t common_int_clr1;
    union {
        struct {
            uint32_t slvif_commonreg_dec_err       :    1;
            uint32_t slvif_commonreg_wr2ro_err     :    1;
            uint32_t slvif_commonreg_rd2wo_err     :    1;
            uint32_t slvif_commonreg_wronhold_err  :    1;
            uint32_t reserved4                     :    3;
            uint32_t slvif_commonreg_wrparity_err  :    1;
            uint32_t slvif_undefinedreg_dec_err    :    1;
            uint32_t mxif1_rch0_eccprot_correrr    :    1;
            uint32_t mxif1_rch0_eccprot_uncorrerr  :    1;
            uint32_t mxif1_rch1_eccprot_correrr    :    1;
            uint32_t mxif1_rch1_eccprot_uncorrerr  :    1;
            uint32_t mxif1_bch_eccprot_correrr     :    1;
            uint32_t mxif1_bch_eccprot_uncorrerr   :    1;
            uint32_t mxif2_rch0_eccprot_correrr    :    1;
            uint32_t mxif2_rch0_eccprot_uncorrerr  :    1;
            uint32_t mxif2_rch1_eccprot_correrr    :    1;
            uint32_t mxif2_rch1_eccprot_uncorrerr  :    1;
            uint32_t mxif2_bch_eccprot_correrr     :    1;
            uint32_t mxif2_bch_eccprot_uncorrerr   :    1;
            uint32_t reserved21                    :    11;
        };
        uint32_t val;
    } common_int_st_ena0;
    uint32_t common_int_st_ena1;
    union {
        struct {
            uint32_t slvif_commonreg_dec_err       :    1;
            uint32_t slvif_commonreg_wr2ro_err     :    1;
            uint32_t slvif_commonreg_rd2wo_err     :    1;
            uint32_t slvif_commonreg_wronhold_err  :    1;
            uint32_t reserved4                     :    3;
            uint32_t slvif_commonreg_wrparity_err  :    1;
            uint32_t slvif_undefinedreg_dec_err    :    1;
            uint32_t mxif1_rch0_eccprot_correrr    :    1;
            uint32_t mxif1_rch0_eccprot_uncorrerr  :    1;
            uint32_t mxif1_rch1_eccprot_correrr    :    1;
            uint32_t mxif1_rch1_eccprot_uncorrerr  :    1;
            uint32_t mxif1_bch_eccprot_correrr     :    1;
            uint32_t mxif1_bch_eccprot_uncorrerr   :    1;
            uint32_t mxif2_rch0_eccprot_correrr    :    1;
            uint32_t mxif2_rch0_eccprot_uncorrerr  :    1;
            uint32_t mxif2_rch1_eccprot_correrr    :    1;
            uint32_t mxif2_rch1_eccprot_uncorrerr  :    1;
            uint32_t mxif2_bch_eccprot_correrr     :    1;
            uint32_t mxif2_bch_eccprot_uncorrerr   :    1;
            uint32_t reserved21                    :    11;
        };
        uint32_t val;
    } common_int_sig_ena0;
    uint32_t common_int_sig_ena1;
    union {
        struct {
            uint32_t slvif_commonreg_dec_err       :    1;
            uint32_t slvif_commonreg_wr2ro_err     :    1;
            uint32_t slvif_commonreg_rd2wo_err     :    1;
            uint32_t slvif_commonreg_wronhold_err  :    1;
            uint32_t reserved4                     :    3;
            uint32_t slvif_commonreg_wrparity_err  :    1;
            uint32_t slvif_undefinedreg_dec_err    :    1;
            uint32_t mxif1_rch0_eccprot_correrr    :    1;
            uint32_t mxif1_rch0_eccprot_uncorrerr  :    1;
            uint32_t mxif1_rch1_eccprot_correrr    :    1;
            uint32_t mxif1_rch1_eccprot_uncorrerr  :    1;
            uint32_t mxif1_bch_eccprot_correrr     :    1;
            uint32_t mxif1_bch_eccprot_uncorrerr   :    1;
            uint32_t mxif2_rch0_eccprot_correrr    :    1;
            uint32_t mxif2_rch0_eccprot_uncorrerr  :    1;
            uint32_t mxif2_rch1_eccprot_correrr    :    1;
            uint32_t mxif2_rch1_eccprot_uncorrerr  :    1;
            uint32_t mxif2_bch_eccprot_correrr     :    1;
            uint32_t mxif2_bch_eccprot_uncorrerr   :    1;
            uint32_t reserved21                    :    11;
        };
        uint32_t val;
    } common_int_st0;
    uint32_t common_int_st1;
    union {
        struct {
            uint32_t dmac_rst                      :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } reset0;
    uint32_t reset1;
    union {
        struct {
            uint32_t gbl_cslp_en                   :    1;
            uint32_t chnl_cslp_en                  :    1;
            uint32_t sbiu_cslp_en                  :    1;
            uint32_t mxif_cslp_en                  :    1;
            uint32_t reserved4                     :    28;
        };
        uint32_t val;
    } lowpower_cfg0;
    union {
        struct {
            uint32_t glch_lpdly                    :    8;
            uint32_t sbiu_lpdly                    :    8;
            uint32_t mxif_lpdly                    :    8;
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } lowpower_cfg1;
    uint32_t reserved_68;
    uint32_t reserved_6c;
    uint32_t reserved_70;
    uint32_t reserved_74;
    uint32_t reserved_78;
    uint32_t reserved_7c;
    uint32_t reserved_80;
    uint32_t reserved_84;
    uint32_t reserved_88;
    uint32_t reserved_8c;
    uint32_t reserved_90;
    uint32_t reserved_94;
    uint32_t reserved_98;
    uint32_t reserved_9c;
    uint32_t reserved_a0;
    uint32_t reserved_a4;
    uint32_t reserved_a8;
    uint32_t reserved_ac;
    uint32_t reserved_b0;
    uint32_t reserved_b4;
    uint32_t reserved_b8;
    uint32_t reserved_bc;
    uint32_t reserved_c0;
    uint32_t reserved_c4;
    uint32_t reserved_c8;
    uint32_t reserved_cc;
    uint32_t reserved_d0;
    uint32_t reserved_d4;
    uint32_t reserved_d8;
    uint32_t reserved_dc;
    uint32_t reserved_e0;
    uint32_t reserved_e4;
    uint32_t reserved_e8;
    uint32_t reserved_ec;
    uint32_t reserved_f0;
    uint32_t reserved_f4;
    uint32_t reserved_f8;
    uint32_t reserved_fc;
    struct {
        uint32_t sar0;
        uint32_t sar1;
        uint32_t dar0;
        uint32_t dar1;
        union {
            struct {
                uint32_t block_ts                      :    22;
                uint32_t reserved22                    :    10;
            };
            uint32_t val;
        } block_ts0;
        uint32_t block_ts1;
        union {
            struct {
                uint32_t sms                           :    1;
                uint32_t reserved1                     :    1;
                uint32_t dms                           :    1;
                uint32_t reserved3                     :    1;
                uint32_t sinc                          :    1;
                uint32_t reserved5                     :    1;
                uint32_t dinc                          :    1;
                uint32_t reserved7                     :    1;
                uint32_t src_tr_width                  :    3;
                uint32_t dst_tr_width                  :    3;
                uint32_t src_msize                     :    4;
                uint32_t dst_msize                     :    4;
                uint32_t ar_cache                      :    4;
                uint32_t aw_cache                      :    4;
                uint32_t nonposted_lastwrite_en        :    1;
                uint32_t reserved31                    :    1;
            };
            uint32_t val;
        } ctl0;
        union {
            struct {
                uint32_t ar_prot                       :    3;
                uint32_t aw_prot                       :    3;
                uint32_t arlen_en                      :    1;
                uint32_t arlen                         :    8;
                uint32_t awlen_en                      :    1;
                uint32_t awlen                         :    8;
                uint32_t src_stat_en                   :    1;
                uint32_t dst_stat_en                   :    1;
                uint32_t ioc_blktfr                    :    1;
                uint32_t reserved59to61                :    3;
                uint32_t shadowreg_or_lli_last         :    1;
                uint32_t shadowreg_or_lli_valid        :    1;
            };
            uint32_t val;
        } ctl1;
        union {
            struct {
                uint32_t src_multblk_type              :    2;
                uint32_t dst_multblk_type              :    2;
                uint32_t reserved4                     :    14;
                uint32_t rd_uid                        :    4;
                uint32_t reserved25                    :    3;
                uint32_t wr_uid                        :    4;
                uint32_t reserved29                    :    3;
            };
            uint32_t val;
        } cfg0;
        union {
            struct {
                uint32_t tt_fc                         :    3;
                uint32_t hs_sel_src                    :    1;
                uint32_t hs_sel_dst                    :    1;
                uint32_t src_hwhs_pol                  :    1;
                uint32_t dst_hwhs_pol                  :    1;
                uint32_t src_per                       :    2;
                uint32_t reserved8                     :    2;
                uint32_t reserved11                    :    1;
                uint32_t dst_per                       :    2;
                uint32_t reserved13                    :    2;
                uint32_t reserved16                    :    1;
                uint32_t ch_prior                      :    3;
                uint32_t lock_ch                       :    1;
                uint32_t lock_ch_l                     :    2;
                uint32_t src_osr_lmt                   :    4;
                uint32_t dst_osr_lmt                   :    4;
                uint32_t reserved31                    :    1;
            };
            uint32_t val;
        } cfg1;
        union {
            struct {
                uint32_t lms                           :    1;
                uint32_t reserved1                     :    5;
                uint32_t loc0                          :    26;
            };
            uint32_t val;
        } llp0;
        uint32_t llp1;
        union {
            struct {
                uint32_t cmpltd_blk_tfr_size           :    22;
                uint32_t reserved22                    :    10;
            };
            uint32_t val;
        } status0;
        union {
            struct {
                uint32_t data_left_in_fifo             :    15;
                uint32_t reserved15                    :    17;
            };
            uint32_t val;
        } status1;
        union {
            struct {
                uint32_t swhs_req_src                  :    1;
                uint32_t swhs_req_src_we               :    1;
                uint32_t swhs_sglreq_src               :    1;
                uint32_t swhs_sglreq_src_we            :    1;
                uint32_t swhs_lst_src                  :    1;
                uint32_t swhs_lst_src_we               :    1;
                uint32_t reserved6                     :    26;
            };
            uint32_t val;
        } swhssrc0;
        uint32_t swhssrc1;
        union {
            struct {
                uint32_t swhs_req_dst                  :    1;
                uint32_t swhs_req_dst_we               :    1;
                uint32_t swhs_sglreq_dst               :    1;
                uint32_t swhs_sglreq_dst_we            :    1;
                uint32_t swhs_lst_dst                  :    1;
                uint32_t swhs_lst_dst_we               :    1;
                uint32_t reserved6                     :    26;
            };
            uint32_t val;
        } swhsdst0;
        uint32_t swhsdst1;
        union {
            struct {
                uint32_t blk_tfr_resumereq             :    1;
                uint32_t reserved1                     :    31;
            };
            uint32_t val;
        } blk_tfr_resumereq0;
        uint32_t blk_tfr_resumereq1;
        union {
            struct {
                uint32_t axi_read_id_suffix            :    1;
                uint32_t reserved1                     :    15;
                uint32_t axi_write_id_suffix           :    1;
                uint32_t reserved17                    :    15;
            };
            uint32_t val;
        } axi_id0;
        uint32_t axi_id1;
        union {
            struct {
                uint32_t axi_awqos                     :    4;
                uint32_t axi_arqos                     :    4;
                uint32_t reserved8                     :    24;
            };
            uint32_t val;
        } axi_qos0;
        uint32_t axi_qos1;
        uint32_t sstat0;
        uint32_t sstat1;
        uint32_t dstat0;
        uint32_t dstat1;
        uint32_t sstatar0;
        uint32_t sstatar1;
        uint32_t dstatar0;
        uint32_t dstatar1;
        union {
            struct {
                uint32_t block_tfr_done                :    1;
                uint32_t dma_tfr_done                  :    1;
                uint32_t reserved2                     :    1;
                uint32_t src_transcomp                 :    1;
                uint32_t dst_transcomp                 :    1;
                uint32_t src_dec_err                   :    1;
                uint32_t dst_dec_err                   :    1;
                uint32_t src_slv_err                   :    1;
                uint32_t dst_slv_err                   :    1;
                uint32_t lli_rd_dec_err                :    1;
                uint32_t lli_wr_dec_err                :    1;
                uint32_t lli_rd_slv_err                :    1;
                uint32_t lli_wr_slv_err                :    1;
                uint32_t shadowreg_or_lli_invalid_err  :    1;
                uint32_t slvif_multiblktype_err        :    1;
                uint32_t reserved15                    :    1;
                uint32_t slvif_dec_err                 :    1;
                uint32_t slvif_wr2ro_err               :    1;
                uint32_t slvif_rd2rwo_err              :    1;
                uint32_t slvif_wronchen_err            :    1;
                uint32_t slvif_shadowreg_wron_valid_err:    1;
                uint32_t slvif_wronhold_err            :    1;
                uint32_t reserved22to24                :    3;
                uint32_t slvif_wrparity_err            :    1;
                uint32_t reserved26                    :    1;
                uint32_t ch_lock_cleared               :    1;
                uint32_t ch_src_suspended              :    1;
                uint32_t ch_suspended                  :    1;
                uint32_t ch_disabled                   :    1;
                uint32_t ch_aborted                    :    1;
            };
            uint32_t val;
        } int_st_ena0;
        union {
            struct {
                uint32_t ecc_prot_chmem_correrr        :    1;
                uint32_t ecc_prot_chmem_uncorrerr      :    1;
                uint32_t ecc_prot_uidmem_correrr       :    1;
                uint32_t ecc_prot_uidmem_uncorrerr     :    1;
                uint32_t reserved4                     :    28;
            };
            uint32_t val;
        } int_st_ena1;
        union {
            struct {
                uint32_t block_tfr_done                :    1;
                uint32_t dma_tfr_done                  :    1;
                uint32_t reserved2                     :    1;
                uint32_t src_transcomp                 :    1;
                uint32_t dst_transcomp                 :    1;
                uint32_t src_dec_err                   :    1;
                uint32_t dst_dec_err                   :    1;
                uint32_t src_slv_err                   :    1;
                uint32_t dst_slv_err                   :    1;
                uint32_t lli_rd_dec_err                :    1;
                uint32_t lli_wr_dec_err                :    1;
                uint32_t lli_rd_slv_err                :    1;
                uint32_t lli_wr_slv_err                :    1;
                uint32_t shadowreg_or_lli_invalid_err  :    1;
                uint32_t slvif_multiblktype_err        :    1;
                uint32_t reserved15                    :    1;
                uint32_t slvif_dec_err                 :    1;
                uint32_t slvif_wr2ro_err               :    1;
                uint32_t slvif_rd2rwo_err              :    1;
                uint32_t slvif_wronchen_err            :    1;
                uint32_t slvif_shadowreg_wron_valid_err:    1;
                uint32_t slvif_wronhold_err            :    1;
                uint32_t reserved22to24                :    3;
                uint32_t slvif_wrparity_err            :    1;
                uint32_t reserved26                    :    1;
                uint32_t ch_lock_cleared               :    1;
                uint32_t ch_src_suspended              :    1;
                uint32_t ch_suspended                  :    1;
                uint32_t ch_disabled                   :    1;
                uint32_t ch_aborted                    :    1;
            };
            uint32_t val;
        } int_st0;
        union {
            struct {
                uint32_t ecc_prot_chmem_correrr        :    1;
                uint32_t ecc_prot_chmem_uncorrerr      :    1;
                uint32_t ecc_prot_uidmem_correrr       :    1;
                uint32_t ecc_prot_uidmem_uncorrerr     :    1;
                uint32_t reserved4                     :    28;
            };
            uint32_t val;
        } int_st1;
        union {
            struct {
                uint32_t block_tfr_done                :    1;
                uint32_t dma_tfr_done                  :    1;
                uint32_t reserved2                     :    1;
                uint32_t src_transcomp                 :    1;
                uint32_t dst_transcomp                 :    1;
                uint32_t src_dec_err                   :    1;
                uint32_t dst_dec_err                   :    1;
                uint32_t src_slv_err                   :    1;
                uint32_t dst_slv_err                   :    1;
                uint32_t lli_rd_dec_err                :    1;
                uint32_t lli_wr_dec_err                :    1;
                uint32_t lli_rd_slv_err                :    1;
                uint32_t lli_wr_slv_err                :    1;
                uint32_t shadowreg_or_lli_invalid_err  :    1;
                uint32_t slvif_multiblktype_err        :    1;
                uint32_t reserved15                    :    1;
                uint32_t slvif_dec_err                 :    1;
                uint32_t slvif_wr2ro_err               :    1;
                uint32_t slvif_rd2rwo_err              :    1;
                uint32_t slvif_wronchen_err            :    1;
                uint32_t slvif_shadowreg_wron_valid_err:    1;
                uint32_t slvif_wronhold_err            :    1;
                uint32_t reserved22                    :    3;
                uint32_t slvif_wrparity_err            :    1;
                uint32_t reserved26                    :    1;
                uint32_t ch_lock_cleared               :    1;
                uint32_t ch_src_suspended              :    1;
                uint32_t ch_suspended                  :    1;
                uint32_t ch_disabled                   :    1;
                uint32_t ch_aborted                    :    1;
            };
            uint32_t val;
        } int_sig_ena0;
        union {
            struct {
                uint32_t ecc_prot_chmem_correrr        :    1;
                uint32_t ecc_prot_chmem_uncorrerr      :    1;
                uint32_t ecc_prot_uidmem_correrr       :    1;
                uint32_t ecc_prot_uidmem_uncorrerr     :    1;
                uint32_t reserved4                     :    28;
            };
            uint32_t val;
        } int_sig_ena1;
        union {
            struct {
                uint32_t block_tfr_done                :    1;
                uint32_t dma_tfr_done                  :    1;
                uint32_t reserved2                     :    1;
                uint32_t src_transcomp                 :    1;
                uint32_t dst_transcomp                 :    1;
                uint32_t src_dec_err                   :    1;
                uint32_t dst_dec_err                   :    1;
                uint32_t src_slv_err                   :    1;
                uint32_t dst_slv_err                   :    1;
                uint32_t lli_rd_dec_err                :    1;
                uint32_t lli_wr_dec_err                :    1;
                uint32_t lli_rd_slv_err                :    1;
                uint32_t lli_wr_slv_err                :    1;
                uint32_t shadowreg_or_lli_invalid_err  :    1;
                uint32_t slvif_multiblktype_err        :    1;
                uint32_t reserved15                    :    1;
                uint32_t slvif_dec_err                 :    1;
                uint32_t slvif_wr2ro_err               :    1;
                uint32_t slvif_rd2rwo_err              :    1;
                uint32_t slvif_wronchen_err            :    1;
                uint32_t slvif_shadowreg_wron_valid_err:    1;
                uint32_t slvif_wronhold_err            :    1;
                uint32_t reserved22to24                :    3;
                uint32_t slvif_wrparity_err            :    1;
                uint32_t reserved26                    :    1;
                uint32_t ch_lock_cleared               :    1;
                uint32_t ch_src_suspended              :    1;
                uint32_t ch_suspended                  :    1;
                uint32_t ch_disabled                   :    1;
                uint32_t ch_aborted                    :    1;
            };
            uint32_t val;
        } int_clr0;
        union {
            struct {
                uint32_t ecc_prot_chmem_correrr        :    1;
                uint32_t ecc_prot_chmem_uncorrerr      :    1;
                uint32_t ecc_prot_uidmem_correrr       :    1;
                uint32_t ecc_prot_uidmem_uncorrerr     :    1;
                uint32_t reserved4                     :    28;
            };
            uint32_t val;
        } int_clr1;
        uint32_t reserved_1a0;
        uint32_t reserved_1a4;
        uint32_t reserved_1a8;
        uint32_t reserved_1ac;
        uint32_t reserved_1b0;
        uint32_t reserved_1b4;
        uint32_t reserved_1b8;
        uint32_t reserved_1bc;
        uint32_t reserved_1c0;
        uint32_t reserved_1c4;
        uint32_t reserved_1c8;
        uint32_t reserved_1cc;
        uint32_t reserved_1d0;
        uint32_t reserved_1d4;
        uint32_t reserved_1d8;
        uint32_t reserved_1dc;
        uint32_t reserved_1e0;
        uint32_t reserved_1e4;
        uint32_t reserved_1e8;
        uint32_t reserved_1ec;
        uint32_t reserved_1f0;
        uint32_t reserved_1f4;
        uint32_t reserved_1f8;
        uint32_t reserved_1fc;
    } ch[4];
} gdma_dev_t;
extern gdma_dev_t GDMA;
#ifdef __cplusplus
}
#endif



#endif /*_SOC_GDMA_STRUCT_H_ */
