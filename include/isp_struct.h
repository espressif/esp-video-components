/*
 * SPDX-FileCopyrightText: 2017-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _SOC_ISP_STRUCT_H_
#define _SOC_ISP_STRUCT_H_


#ifdef __cplusplus
extern "C" {
#endif

typedef volatile struct {
    uint32_t ver_date;
    union {
        struct {
            uint32_t clk_en                        :    1;
            uint32_t clk_blc_en                    :    1;
            uint32_t clk_lsc_en                    :    1;
            uint32_t clk_dpc_en                    :    1;
            uint32_t clk_bf_en                     :    1;
            uint32_t clk_demosaic_en               :    1;
            uint32_t clk_median_en                 :    1;
            uint32_t clk_ccm_en                    :    1;
            uint32_t clk_gamma_en                  :    1;
            uint32_t clk_rgb2yuv_en                :    1;
            uint32_t clk_sharp_en                  :    1;
            uint32_t clk_color_en                  :    1;
            uint32_t clk_yuv2rgb_en                :    1;
            uint32_t clk_ae_en                     :    1;
            uint32_t clk_af_en                     :    1;
            uint32_t clk_awb_en                    :    1;
            uint32_t clk_hist_en                   :    1;
            uint32_t clk_mipi_idi_en               :    1;
            uint32_t reserved18                    :    14;
        };
        uint32_t val;
    } clk_en;
    union {
        struct {
            uint32_t en                            :    1;  /*isp global enable*/
            uint32_t ccm_en                        :    1;
            uint32_t dpc_check_en                  :    1;  /*  */
            uint32_t dpc_sta_en                    :    1;  /*    */
            uint32_t dpc_dyn_en                    :    1;  /*    */
            uint32_t dpc_black_en                  :    1;  /* */
            uint32_t dpc_method_sel                :    1;  /*    */
            uint32_t dpc_check_od_en               :    1;
            uint32_t dpc_en                        :    1;
            uint32_t demosaic_en                   :    1;
            uint32_t rgb2yuv_en                    :    1;
            uint32_t lsc_en                        :    1;
            uint32_t bf_en                         :    1;
            uint32_t median_en                     :    1;
            uint32_t gamma_en                      :    1;
            uint32_t ae_mode                       :    1;  /* 0: first frame  1: multi-frame*/
            uint32_t ae_en                         :    1;
            uint32_t yuv2rgb_en                    :    1;
            uint32_t sharp_en                      :    1;
            uint32_t af_en                         :    1;
            uint32_t awb_en                        :    1;
            uint32_t color_en                      :    1;
            uint32_t blc_en                        :    1;
            uint32_t hist_en                       :    1;
            uint32_t mipi_data_en                  :    1;
            uint32_t data_type                     :    2;  /*0:RAW8 1:RAW10 2:RAW12*/
            uint32_t in_src                        :    2;  /*0:CSI HOST 1:CAM 2:DMA*/
            uint32_t out_type                      :    3;  /*0: RAW8 1: YUV422 2: RGB888 3: YUV420 4: RGB565*/
        };
        uint32_t val;
    } cntl;
    union {
        struct {
            uint32_t hsync_cnt                     :    8;  /*clock cnt before hsync and after vsync and line_end*/
            uint32_t reserved8                     :    24;
        };
        uint32_t val;
    } hsync_cnt;
    union {
        struct {
            uint32_t dpc_padding_data              :    8;
            uint32_t bf_padding_data               :    8;
            uint32_t demosaic_padding_data         :    8;
            uint32_t median_padding_data           :    8;
        };
        uint32_t val;
    } padding_data;
    union {
        struct {
            uint32_t dpc_padding_mode              :    1;  /*0: use pixel in image to do padding   1: use reg_padding_data to do padding*/
            uint32_t bf_padding_mode               :    1;  /*0: use pixel in image to do padding   1: use reg_padding_data to do padding*/
            uint32_t demosaic_padding_mode         :    1;  /*0: use pixel in image to do padding   1: use reg_padding_data to do padding*/
            uint32_t median_padding_mode           :    1;  /*0: use pixel in image to do padding   1: use reg_padding_data to do padding*/
            uint32_t reserved4                     :    28;
        };
        uint32_t val;
    } padding_mode;
    union {
        struct {
            uint32_t vadr_num                      :    12;  /*image row number - 1*/
            uint32_t hadr_num                      :    12;  /*image line number - 1*/
            uint32_t reserved24                    :    3;
            uint32_t bayer_mode                    :    2;  /*00 : BG/GR    01 : GB/RG   10 : GR/BG  11 : RG/GB*/
            uint32_t hsync_start_exist             :    1;  /*line end packet exist or not*/
            uint32_t hsync_end_exist               :    1;  /*line start packet exist or not*/
            uint32_t reserved31                    :    1;
        };
        uint32_t val;
    } frame_cfg;
    union {
        struct {
            uint32_t ccm_coef_rr                   :    13;  /*color correction matrix coefficient*/
            uint32_t ccm_coef_rg                   :    13;  /*color correction matrix coefficient*/
            uint32_t reserved26                    :    6;
        };
        uint32_t val;
    } ccm_coef_reg0;
    union {
        struct {
            uint32_t ccm_coef_rb                   :    13;  /*color correction matrix coefficient*/
            uint32_t ccm_coef_gr                   :    13;  /*color correction matrix coefficient*/
            uint32_t reserved26                    :    6;
        };
        uint32_t val;
    } ccm_coef_reg1;
    union {
        struct {
            uint32_t ccm_coef_gg                   :    13;  /*color correction matrix coefficient*/
            uint32_t ccm_coef_gb                   :    13;  /*color correction matrix coefficient*/
            uint32_t reserved26                    :    6;
        };
        uint32_t val;
    } ccm_coef_reg3;
    union {
        struct {
            uint32_t ccm_coef_br                   :    13;  /*color correction matrix coefficient*/
            uint32_t ccm_coef_bg                   :    13;  /*color correction matrix coefficient*/
            uint32_t reserved26                    :    6;
        };
        uint32_t val;
    } ccm_coef_reg4;
    union {
        struct {
            uint32_t ccm_coef_bb                   :    13;  /*color correction matrix coefficient*/
            uint32_t reserved13                    :    19;
        };
        uint32_t val;
    } ccm_coef_reg5;
    union {
        struct {
            uint32_t sigma                         :    6;
            uint32_t reserved6                     :    26;
        };
        uint32_t val;
    } bf_sigma;
    uint32_t bf_gau0;
    union {
        struct {
            uint32_t gau_template1                 :    4;
            uint32_t reserved4                     :    28;
        };
        uint32_t val;
    } bf_gau1;
    union {
        struct {
            uint32_t dpc_threshold_l               :    8;  /*share register in dpc module low threshold*/
            uint32_t dpc_threshold_h               :    8;  /*share register in dpc module high threshold*/
            uint32_t dpc_factor_dark               :    6;  /*dynamic correction method 1 dark   factor*/
            uint32_t dpc_factor_brig               :    6;  /*dynamic correction method 1 bright factor */
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } dpc_conf;
    union {
        struct {
            uint32_t dpc_deadpix_cnt               :    10;  /*dead pixel count */
            uint32_t reserved10                    :    22;
        };
        uint32_t val;
    } dpc_deadpix_cnt;
    union {
        struct {
            uint32_t lut_addr                      :    12;
            uint32_t lut_num                       :    4;  /*0000:LSC LUT 0001:DPC LUT*/
            uint32_t lut_cmd                       :    1;  /*0:rd 1: wr */
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } lut_cmd;
    uint32_t lut_wdata;
    uint32_t lut_rdata;
    union {
        struct {
            uint32_t lsc_xtablesize                :    5;
            uint32_t reserved5                     :    27;
        };
        uint32_t val;
    } lsc_tablesize;
    union {
        struct {
            uint32_t data_type_err                 :    1;  /*isp only support RGB bayer data type, other type will report type_err_int*/
            uint32_t async_fifo_ovf                :    1;
            uint32_t buf_full                      :    1;
            uint32_t hvnum_setting_err             :    1;  /*hnum and vnum setting format err*/
            uint32_t data_type_setting_err         :    1;  /*setting invalid reg_data_type*/
            uint32_t mipi_hnum_unmatch             :    1;  /*hnum setting unmatch with mipi input*/
            uint32_t dpc_check_done                :    1;
            uint32_t gamma_xcoord_err              :    1;  /*it report the sum of the lengths represented by reg_gamma_x00~x0F isn't equal to 256*/
            uint32_t ae_frame_done                 :    1;  /*when single mode en, set to 1 after the calculation result is obtained or after every frame result obtained,set to 1*/
            uint32_t af_fdone                      :    1;  /*when auto_update enable, each frame done will send one int pulse when manual_update, each time when write 1 to reg_manual_update will send a int pulse when next frame done*/
            uint32_t af_env                        :    1;  /*send a int pulse when env_det function enabled and environment changes detected*/
            uint32_t awb_fdone                     :    1;  /*send a int pulse when statistic of one awb frame done*/
            uint32_t hist_fdone                    :    1;  /*send a int pulse when statistic of one frame histogram done*/
            uint32_t frame                         :    1;  /*isp frame end interrupt*/
            uint32_t blc_frame                     :    1;
            uint32_t lsc_frame                     :    1;
            uint32_t dpc_frame                     :    1;
            uint32_t bf_frame                      :    1;
            uint32_t demosaic_frame                :    1;
            uint32_t median_frame                  :    1;
            uint32_t ccm_frame                     :    1;
            uint32_t gamma_frame                   :    1;
            uint32_t rgb2yuv_frame                 :    1;
            uint32_t sharp_frame                   :    1;
            uint32_t color_frame                   :    1;
            uint32_t yuv2rgb_frame                 :    1;
            uint32_t tail_idi_frame                :    1;  /*isp_tail idi frame_end interrupt*/
            uint32_t header_idi_frame              :    1;  /*real input frame end of isp_input*/
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } int_raw;
    union {
        struct {
            uint32_t data_type_err                 :    1;
            uint32_t async_fifo_ovf                :    1;
            uint32_t buf_full                      :    1;
            uint32_t hvnum_setting_err             :    1;
            uint32_t data_type_setting_err         :    1;
            uint32_t mipi_hnum_unmatch             :    1;
            uint32_t dpc_check_done                :    1;
            uint32_t gamma_xcoord_err              :    1;
            uint32_t ae_frame_done                 :    1;
            uint32_t af_fdone                      :    1;
            uint32_t af_env                        :    1;
            uint32_t awb_fdone                     :    1;
            uint32_t hist_fdone                    :    1;
            uint32_t frame                         :    1;
            uint32_t blc_frame                     :    1;
            uint32_t lsc_frame                     :    1;
            uint32_t dpc_frame                     :    1;
            uint32_t bf_frame                      :    1;
            uint32_t demosaic_frame                :    1;
            uint32_t median_frame                  :    1;
            uint32_t ccm_frame                     :    1;
            uint32_t gamma_frame                   :    1;
            uint32_t rgb2yuv_frame                 :    1;
            uint32_t sharp_frame                   :    1;
            uint32_t color_frame                   :    1;
            uint32_t yuv2rgb_frame                 :    1;
            uint32_t tail_idi_frame                :    1;
            uint32_t header_idi_frame              :    1;
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } int_st;
    union {
        struct {
            uint32_t data_type_err                 :    1;
            uint32_t async_fifo_ovf                :    1;
            uint32_t buf_full                      :    1;
            uint32_t hvnum_setting_err             :    1;
            uint32_t data_type_setting_err         :    1;
            uint32_t mipi_hnum_unmatch             :    1;
            uint32_t dpc_check_done                :    1;
            uint32_t gamma_xcoord_err              :    1;
            uint32_t ae_frame_done                 :    1;
            uint32_t af_fdone                      :    1;
            uint32_t af_env                        :    1;
            uint32_t awb_fdone                     :    1;
            uint32_t hist_fdone                    :    1;
            uint32_t frame                         :    1;
            uint32_t blc_frame                     :    1;
            uint32_t lsc_frame                     :    1;
            uint32_t dpc_frame                     :    1;
            uint32_t bf_frame                      :    1;
            uint32_t demosaic_frame                :    1;
            uint32_t median_frame                  :    1;
            uint32_t ccm_frame                     :    1;
            uint32_t gamma_frame                   :    1;
            uint32_t rgb2yuv_frame                 :    1;
            uint32_t sharp_frame                   :    1;
            uint32_t color_frame                   :    1;
            uint32_t yuv2rgb_frame                 :    1;
            uint32_t tail_idi_frame                :    1;
            uint32_t header_idi_frame              :    1;
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } int_ena;
    union {
        struct {
            uint32_t data_type_err                 :    1;
            uint32_t async_fifo_ovf                :    1;
            uint32_t buf_full                      :    1;
            uint32_t hvnum_setting_err             :    1;
            uint32_t data_type_setting_err         :    1;
            uint32_t mipi_hnum_unmatch             :    1;
            uint32_t dpc_check_done                :    1;
            uint32_t gamma_xcoord_err              :    1;
            uint32_t ae_frame_done                 :    1;
            uint32_t af_fdone                      :    1;
            uint32_t af_env                        :    1;
            uint32_t awb_fdone                     :    1;
            uint32_t hist_fdone                    :    1;
            uint32_t frame                         :    1;
            uint32_t blc_frame                     :    1;
            uint32_t lsc_frame                     :    1;
            uint32_t dpc_frame                     :    1;
            uint32_t bf_frame                      :    1;
            uint32_t demosaic_frame                :    1;
            uint32_t median_frame                  :    1;
            uint32_t ccm_frame                     :    1;
            uint32_t gamma_frame                   :    1;
            uint32_t rgb2yuv_frame                 :    1;
            uint32_t sharp_frame                   :    1;
            uint32_t color_frame                   :    1;
            uint32_t yuv2rgb_frame                 :    1;
            uint32_t tail_idi_frame                :    1;
            uint32_t header_idi_frame              :    1;
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } int_clr;
    union {
        struct {
            uint32_t gamma_update                  :    1;  /*Indicates that gamma register configuration is complete*/
            uint32_t gamma_b_last_correct          :    1;  /*enable last segment correcction*/
            uint32_t gamma_g_last_correct          :    1;
            uint32_t gamma_r_last_correct          :    1;
            uint32_t reserved4                     :    28;
        };
        uint32_t val;
    } gamma_ctrl;
    union {
        struct {
            uint32_t gamma_r_y03                   :    8;
            uint32_t gamma_r_y02                   :    8;
            uint32_t gamma_r_y01                   :    8;
            uint32_t gamma_r_y00                   :    8;  /*Y-axis of gamma curve, ohter similar omitted*/
        };
        uint32_t val;
    } gamma_ry1;
    union {
        struct {
            uint32_t gamma_r_y07                   :    8;
            uint32_t gamma_r_y06                   :    8;
            uint32_t gamma_r_y05                   :    8;
            uint32_t gamma_r_y04                   :    8;
        };
        uint32_t val;
    } gamma_ry2;
    union {
        struct {
            uint32_t gamma_r_y0b                   :    8;
            uint32_t gamma_r_y0a                   :    8;
            uint32_t gamma_r_y09                   :    8;
            uint32_t gamma_r_y08                   :    8;
        };
        uint32_t val;
    } gamma_ry3;
    union {
        struct {
            uint32_t gamma_r_y0f                   :    8;
            uint32_t gamma_r_y0e                   :    8;
            uint32_t gamma_r_y0d                   :    8;
            uint32_t gamma_r_y0c                   :    8;
        };
        uint32_t val;
    } gamma_ry4;
    union {
        struct {
            uint32_t gamma_g_y03                   :    8;
            uint32_t gamma_g_y02                   :    8;
            uint32_t gamma_g_y01                   :    8;
            uint32_t gamma_g_y00                   :    8;
        };
        uint32_t val;
    } gamma_gy1;
    union {
        struct {
            uint32_t gamma_g_y07                   :    8;
            uint32_t gamma_g_y06                   :    8;
            uint32_t gamma_g_y05                   :    8;
            uint32_t gamma_g_y04                   :    8;
        };
        uint32_t val;
    } gamma_gy2;
    union {
        struct {
            uint32_t gamma_g_y0b                   :    8;
            uint32_t gamma_g_y0a                   :    8;
            uint32_t gamma_g_y09                   :    8;
            uint32_t gamma_g_y08                   :    8;
        };
        uint32_t val;
    } gamma_gy3;
    union {
        struct {
            uint32_t gamma_g_y0f                   :    8;
            uint32_t gamma_g_y0e                   :    8;
            uint32_t gamma_g_y0d                   :    8;
            uint32_t gamma_g_y0c                   :    8;
        };
        uint32_t val;
    } gamma_gy4;
    union {
        struct {
            uint32_t gamma_b_y03                   :    8;
            uint32_t gamma_b_y02                   :    8;
            uint32_t gamma_b_y01                   :    8;
            uint32_t gamma_b_y00                   :    8;
        };
        uint32_t val;
    } gamma_by1;
    union {
        struct {
            uint32_t gamma_b_y07                   :    8;
            uint32_t gamma_b_y06                   :    8;
            uint32_t gamma_b_y05                   :    8;
            uint32_t gamma_b_y04                   :    8;
        };
        uint32_t val;
    } gamma_by2;
    union {
        struct {
            uint32_t gamma_b_y0b                   :    8;
            uint32_t gamma_b_y0a                   :    8;
            uint32_t gamma_b_y09                   :    8;
            uint32_t gamma_b_y08                   :    8;
        };
        uint32_t val;
    } gamma_by3;
    union {
        struct {
            uint32_t gamma_b_y0f                   :    8;
            uint32_t gamma_b_y0e                   :    8;
            uint32_t gamma_b_y0d                   :    8;
            uint32_t gamma_b_y0c                   :    8;
        };
        uint32_t val;
    } gamma_by4;
    union {
        struct {
            uint32_t gamma_r_x07                   :    3;
            uint32_t gamma_r_x06                   :    3;
            uint32_t gamma_r_x05                   :    3;
            uint32_t gamma_r_x04                   :    3;
            uint32_t gamma_r_x03                   :    3;
            uint32_t gamma_r_x02                   :    3;
            uint32_t gamma_r_x01                   :    3;
            uint32_t gamma_r_x00                   :    3;  /*X-axis of gamma curve, it represents the power of the distance from the previous point, ohter similar omitted.*/
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } gamma_rx1;
    union {
        struct {
            uint32_t gamma_r_x0f                   :    3;
            uint32_t gamma_r_x0e                   :    3;
            uint32_t gamma_r_x0d                   :    3;
            uint32_t gamma_r_x0c                   :    3;
            uint32_t gamma_r_x0b                   :    3;
            uint32_t gamma_r_x0a                   :    3;
            uint32_t gamma_r_x09                   :    3;
            uint32_t gamma_r_x08                   :    3;
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } gamma_rx2;
    union {
        struct {
            uint32_t gamma_g_x07                   :    3;
            uint32_t gamma_g_x06                   :    3;
            uint32_t gamma_g_x05                   :    3;
            uint32_t gamma_g_x04                   :    3;
            uint32_t gamma_g_x03                   :    3;
            uint32_t gamma_g_x02                   :    3;
            uint32_t gamma_g_x01                   :    3;
            uint32_t gamma_g_x00                   :    3;
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } gamma_gx1;
    union {
        struct {
            uint32_t gamma_g_x0f                   :    3;
            uint32_t gamma_g_x0e                   :    3;
            uint32_t gamma_g_x0d                   :    3;
            uint32_t gamma_g_x0c                   :    3;
            uint32_t gamma_g_x0b                   :    3;
            uint32_t gamma_g_x0a                   :    3;
            uint32_t gamma_g_x09                   :    3;
            uint32_t gamma_g_x08                   :    3;
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } gamma_gx2;
    union {
        struct {
            uint32_t gamma_b_x07                   :    3;
            uint32_t gamma_b_x06                   :    3;
            uint32_t gamma_b_x05                   :    3;
            uint32_t gamma_b_x04                   :    3;
            uint32_t gamma_b_x03                   :    3;
            uint32_t gamma_b_x02                   :    3;
            uint32_t gamma_b_x01                   :    3;
            uint32_t gamma_b_x00                   :    3;
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } gamma_bx1;
    union {
        struct {
            uint32_t gamma_b_x0f                   :    3;
            uint32_t gamma_b_x0e                   :    3;
            uint32_t gamma_b_x0d                   :    3;
            uint32_t gamma_b_x0c                   :    3;
            uint32_t gamma_b_x0b                   :    3;
            uint32_t gamma_b_x0a                   :    3;
            uint32_t gamma_b_x09                   :    3;
            uint32_t gamma_b_x08                   :    3;
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } gamma_bx2;
    union {
        struct {
            uint32_t ae_x_bsize                    :    11;  /* every block x size*/
            uint32_t ae_x_start                    :    11;  /*  first block start x address*/
            uint32_t reserved22                    :    10;
        };
        uint32_t val;
    } ae_bx;
    union {
        struct {
            uint32_t ae_y_bsize                    :    11;  /* every block y size*/
            uint32_t ae_y_start                    :    11;  /*  first block start y address*/
            uint32_t reserved22                    :    10;
        };
        uint32_t val;
    } ae_by;
    union {
        struct {
            uint32_t ae_b03_mean                   :    8;  /*   block03 Y mean data*/
            uint32_t ae_b02_mean                   :    8;  /*  block02 Y mean data*/
            uint32_t ae_b01_mean                   :    8;  /* block01 Y mean data  */
            uint32_t ae_b00_mean                   :    8;  /* block00 Y mean data*/
        };
        uint32_t val;
    } ae_block_mean_0;
    union {
        struct {
            uint32_t ae_b12_mean                   :    8;  /*   block12 Y mean data*/
            uint32_t ae_b11_mean                   :    8;  /*  block11 Y mean data*/
            uint32_t ae_b10_mean                   :    8;  /* block10 Y mean data*/
            uint32_t ae_b04_mean                   :    8;  /* block04 Y mean data*/
        };
        uint32_t val;
    } ae_block_mean_1;
    union {
        struct {
            uint32_t ae_b21_mean                   :    8;  /*   block21 Y mean data*/
            uint32_t ae_b20_mean                   :    8;  /*  block20 Y mean data*/
            uint32_t ae_b14_mean                   :    8;  /* block14 Y mean data*/
            uint32_t ae_b13_mean                   :    8;  /* block13 Y mean data*/
        };
        uint32_t val;
    } ae_block_mean_2;
    union {
        struct {
            uint32_t ae_b30_mean                   :    8;  /*   block30 Y mean data*/
            uint32_t ae_b24_mean                   :    8;  /*  block24 Y mean data*/
            uint32_t ae_b23_mean                   :    8;  /* block23 Y mean data*/
            uint32_t ae_b22_mean                   :    8;  /* block22 Y mean data*/
        };
        uint32_t val;
    } ae_block_mean_3;
    union {
        struct {
            uint32_t ae_b34_mean                   :    8;  /*   block34 Y mean data*/
            uint32_t ae_b33_mean                   :    8;  /*  block33 Y mean data*/
            uint32_t ae_b32_mean                   :    8;  /* block32 Y mean data*/
            uint32_t ae_b31_mean                   :    8;  /* block31 Y mean data*/
        };
        uint32_t val;
    } ae_block_mean_4;
    union {
        struct {
            uint32_t ae_b43_mean                   :    8;  /*   block43 Y mean data*/
            uint32_t ae_b42_mean                   :    8;  /*  block42 Y mean data*/
            uint32_t ae_b41_mean                   :    8;  /* block41 Y mean data*/
            uint32_t ae_b40_mean                   :    8;  /* block40 Y mean data*/
        };
        uint32_t val;
    } ae_block_mean_5;
    union {
        struct {
            uint32_t reserved0                     :    24;
            uint32_t ae_b44_mean                   :    8;  /* block44 Y mean data*/
        };
        uint32_t val;
    } ae_block_mean_6;
    union {
        struct {
            uint32_t sharp_threshold_low           :    8;  /*sharpen threshold for detail*/
            uint32_t sharp_threshold_high          :    8;  /*sharpen threshold for edge*/
            uint32_t sharp_amount_low              :    8;  /*sharpen amount for detail*/
            uint32_t sharp_amount_high             :    8;  /*sharpen amount for edge*/
        };
        uint32_t val;
    } sharp_ctrl0;
    union {
        struct {
            uint32_t sharp_filter_coe00            :    5;  /*unsharp masking(usm) filter coefficient*/
            uint32_t sharp_filter_coe01            :    5;  /*usm filter coefficient*/
            uint32_t sharp_filter_coe02            :    5;  /*usm filter coefficient*/
            uint32_t reserved15                    :    17;
        };
        uint32_t val;
    } sharp_filter0;
    union {
        struct {
            uint32_t sharp_filter_coe10            :    5;  /*usm filter coefficient*/
            uint32_t sharp_filter_coe11            :    5;  /*usm filter coefficient*/
            uint32_t sharp_filter_coe12            :    5;  /*usm filter coefficient*/
            uint32_t reserved15                    :    17;
        };
        uint32_t val;
    } sharp_filter1;
    union {
        struct {
            uint32_t sharp_filter_coe20            :    5;  /*usm filter coefficient*/
            uint32_t sharp_filter_coe21            :    5;  /*usm filter coefficient*/
            uint32_t sharp_filter_coe22            :    5;  /*usm filter coefficient*/
            uint32_t reserved15                    :    17;
        };
        uint32_t val;
    } sharp_filter2;
    union {
        struct {
            uint32_t sharp_padding_mode            :    1;  /*sharp padding mode*/
            uint32_t sharp_padding_data            :    8;  /*sharp padding data*/
            uint32_t sharp_gradient_max            :    8;  /*sharp max gradient, refresh at the end of each frame end*/
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } sharp_ctrl1;
    union {
        struct {
            uint32_t dma_en                        :    1;  /*write 1 to triger dma to get 1 frame*/
            uint32_t dma_update_reg                :    1;  /*write 1 to update reg_dma_burst_len & reg_dma_data_type*/
            uint32_t dma_data_type                 :    6;  /*idi data type for image data*/
            uint32_t dma_burst_len                 :    12;  /*set according to dma_msize, it is the number of 64bits in a dma transfer*/
            uint32_t dma_req_interval              :    12;  /*dma req interval, 16'h1: 1 cycle, 16'h10 2 cycle ...*/
        };
        uint32_t val;
    } dma_cntl;
    union {
        struct {
            uint32_t dma_raw_num_total             :    22;  /*the number of 64bits in a frame*/
            uint32_t reserved22                    :    9;
            uint32_t dma_raw_num_total_set         :    1;  /*write 1 to update reg_dma_raw_num_total*/
        };
        uint32_t val;
    } dma_raw_data;
    union {
        struct {
            uint32_t cam_en                        :    1;  /*write 1 to start recive camera data, write 0 to disable*/
            uint32_t cam_update_reg                :    1;  /*write 1 to update ISP_CAM_CONF*/
            uint32_t cam_reset                     :    1;  /*cam clk domain reset*/
            uint32_t cam_clk_inv                   :    1;  /*invert cam clk from pad*/
            uint32_t reserved4                     :    28;
        };
        uint32_t val;
    } cam_cntl;
    union {
        struct {
            uint32_t cam_data_order                :    1;
            uint32_t cam_2byte_mode                :    1;  /* cam 2 byte mode*/
            uint32_t cam_data_type                 :    6;  /* idi data type for image data*/
            uint32_t cam_de_inv                    :    1;  /* cam data enable invert   */
            uint32_t cam_hsync_inv                 :    1;  /* cam hsync invert*/
            uint32_t cam_vsync_inv                 :    1;  /* cam vsync invert*/
            uint32_t cam_vsync_filter_thres        :    3;  /* */
            uint32_t cam_vsync_filter_en           :    1;  /* */
            uint32_t reserved15                    :    17;
        };
        uint32_t val;
    } cam_conf;
    union {
        struct {
            uint32_t af_auto_update                :    1;  /*when set to 1, will update sum and lum each frame*/
            uint32_t reserved1                     :    3;
            uint32_t af_manual_update              :    1;
            uint32_t reserved5                     :    3;  /*each write 1 to the reg will update the sum and lum once*/
            uint32_t af_env_threshold              :    4;  /*when both sum and lum changes upper than this value, consider environment changes and need to trigger a new autofocus. 4Bit fractional*/
            uint32_t reserved12                    :    4;
            uint32_t af_env_period                 :    8;
            uint32_t reserved24                    :    8;  /*environment changes detection period (frame). When set to 0, disable this function*/
        };
        uint32_t val;
    } af_ctrl0;
    union {
        struct {
            uint32_t af_thpixnum                   :    22;  /*pixnum used when calculating the autofocus threshold. Set to 0 to disable threshold calculation*/
            uint32_t reserved22                    :    10;
        };
        uint32_t val;
    } af_ctrl1;
    union {
        struct {
            uint32_t af_gen_threshold_min          :    16;  /*min threshold when use auto_threshold*/
            uint32_t af_gen_threshold_max          :    16;  /*max threshold when use auto_threshold*/
        };
        uint32_t val;
    } af_gen_th_ctrl;
    union {
        struct {
            uint32_t af_env_user_threshold_sum     :    30;  /*user setup env detect sum threshold*/
            uint32_t reserved30                    :    2;
        };
        uint32_t val;
    } af_env_user_th_sum;
    union {
        struct {
            uint32_t af_env_user_threshold_lum     :    30;  /*user setup env detect lum threshold*/
            uint32_t reserved30                    :    2;
        };
        uint32_t val;
    } af_env_user_th_lum;
    union {
        struct {
            uint32_t af_threshold                  :    16;  /*user threshold. When set to non-zero, autofocus will use this threshold*/
            uint32_t af_gen_threshold              :    16;
        };
        uint32_t val;
    } af_threshold;
    union {
        struct {
            uint32_t af_rpoint_a                   :    12;  /*left coordinate of focus window a, must >= 2*/
            uint32_t reserved12                    :    4;
            uint32_t af_lpoint_a                   :    12;  /*top coordinate of focus window a, must >= 2*/
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } af_hscale_a;
    union {
        struct {
            uint32_t af_bpoint_a                   :    12;  /*right coordinate of focus window a, must <= hnum-2*/
            uint32_t reserved12                    :    4;
            uint32_t af_tpoint_a                   :    12;  /*bottom coordinate of focus window a, must <= hnum-2*/
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } af_vscale_a;
    union {
        struct {
            uint32_t af_rpoint_b                   :    12;  /*left coordinate of focus window b, must >= 2*/
            uint32_t reserved12                    :    4;
            uint32_t af_lpoint_b                   :    12;  /*top coordinate of focus window b, must >= 2*/
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } af_hscale_b;
    union {
        struct {
            uint32_t af_bpoint_b                   :    12;  /*right coordinate of focus window b, must <= hnum-2*/
            uint32_t reserved12                    :    4;
            uint32_t af_tpoint_b                   :    12;  /*bottom coordinate of focus window b, must <= hnum-2*/
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } af_vscale_b;
    union {
        struct {
            uint32_t af_rpoint_c                   :    12;  /*left coordinate of focus window c, must >= 2*/
            uint32_t reserved12                    :    4;
            uint32_t af_lpoint_c                   :    12;  /*top coordinate of focus window c, must >= 2*/
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } af_hscale_c;
    union {
        struct {
            uint32_t af_bpoint_c                   :    12;  /*right coordinate of focus window c, must <= hnum-2*/
            uint32_t reserved12                    :    4;
            uint32_t af_tpoint_c                   :    12;  /*bottom coordinate of focus window c, must <= hnum-2*/
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } af_vscale_c;
    union {
        struct {
            uint32_t af_suma                       :    28;  /*result of image sharpness of focus window a*/
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } af_sum_a;
    union {
        struct {
            uint32_t af_sumb                       :    28;  /*result of image sharpness of focus window b*/
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } af_sum_b;
    union {
        struct {
            uint32_t af_sumc                       :    28;  /*result of image sharpness of focus window c*/
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } af_sum_c;
    union {
        struct {
            uint32_t af_luma                       :    28;  /*result of accumulation of pix light of focus window a*/
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } af_lum_a;
    union {
        struct {
            uint32_t af_lumb                       :    28;  /*result of accumulation of pix light of focus window b*/
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } af_lum_b;
    union {
        struct {
            uint32_t af_lumc                       :    28;  /*result of accumulation of pix light of focus window c*/
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } af_lum_c;
    union {
        struct {
            uint32_t awb_mode                      :    2;  /*awb algo sel. 00: none sellected. 01: sel algo0. 10: sel algo1. 11: sel both algo0 and algo1*/
            uint32_t reserved2                     :    2;
            uint32_t awb_sample                    :    1;  /*awb sample location, 0:before ccm, 1:after ccm*/
            uint32_t reserved5                     :    27;
        };
        uint32_t val;
    } awb_mode;
    union {
        struct {
            uint32_t awb_rpoint                    :    12;  /*window right coordinate*/
            uint32_t reserved12                    :    4;
            uint32_t awb_lpoint                    :    12;  /*window left coordinate*/
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } awb_hscale;
    union {
        struct {
            uint32_t awb_bpoint                    :    12;  /*window bottom coordinate*/
            uint32_t reserved12                    :    4;
            uint32_t awb_tpoint                    :    12;  /*window top coordinate*/
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } awb_vscale;
    union {
        struct {
            uint32_t awb_min_lum                   :    10;  /*lower threshold of r+g+b*/
            uint32_t reserved10                    :    6;
            uint32_t awb_max_lum                   :    10;  /*upper threshold of r+g+b*/
            uint32_t reserved26                    :    6;
        };
        uint32_t val;
    } awb_th_lum;
    union {
        struct {
            uint32_t awb_min_rg                    :    10;  /*lower threshold of r/g, 2bit integer and 8bit fraction*/
            uint32_t reserved10                    :    6;
            uint32_t awb_max_rg                    :    10;  /*upper threshold of r/g, 2bit integer and 8bit fraction*/
            uint32_t reserved26                    :    6;
        };
        uint32_t val;
    } awb_th_rg;
    union {
        struct {
            uint32_t awb_min_bg                    :    10;  /*lower threshold of b/g, 2bit integer and 8bit fraction*/
            uint32_t reserved10                    :    6;
            uint32_t awb_max_bg                    :    10;  /*upper threshold of b/g, 2bit integer and 8bit fraction*/
            uint32_t reserved26                    :    6;
        };
        uint32_t val;
    } awb_th_bg;
    union {
        struct {
            uint32_t awb1_p1_dist                  :    10;  /*max distance of rg+bg for algo1, 2bit integer and 8bit fraction*/
            uint32_t reserved10                    :    6;
            uint32_t awb1_p0_dist                  :    10;
            uint32_t reserved26                    :    6;
        };
        uint32_t val;
    } awb1_dist0;
    union {
        struct {
            uint32_t awb1_p3_dist                  :    10;
            uint32_t reserved10                    :    6;
            uint32_t awb1_p2_dist                  :    10;
            uint32_t reserved26                    :    6;
        };
        uint32_t val;
    } awb1_dist1;
    union {
        struct {
            uint32_t awb1_p5_dist                  :    10;
            uint32_t reserved10                    :    6;
            uint32_t awb1_p4_dist                  :    10;
            uint32_t reserved26                    :    6;
        };
        uint32_t val;
    } awb1_dist2;
    union {
        struct {
            uint32_t awb1_p7_dist                  :    10;
            uint32_t reserved10                    :    6;
            uint32_t awb1_p6_dist                  :    10;
            uint32_t reserved26                    :    6;
        };
        uint32_t val;
    } awb1_dist3;
    union {
        struct {
            uint32_t awb1_p0_bg                    :    10;  /*b/g of color temperature point p0, 2bit integer and 8bit fraction*/
            uint32_t reserved10                    :    6;
            uint32_t awb1_p0_rg                    :    10;  /*r/g of color temperature point p0, 2bit integer and 8bit fraction*/
            uint32_t reserved26                    :    6;
        };
        uint32_t val;
    } awb1_p0;
    union {
        struct {
            uint32_t awb1_p1_bg                    :    10;
            uint32_t reserved10                    :    6;
            uint32_t awb1_p1_rg                    :    10;
            uint32_t reserved26                    :    6;
        };
        uint32_t val;
    } awb1_p1;
    union {
        struct {
            uint32_t awb1_p2_bg                    :    10;
            uint32_t reserved10                    :    6;
            uint32_t awb1_p2_rg                    :    10;
            uint32_t reserved26                    :    6;
        };
        uint32_t val;
    } awb1_p2;
    union {
        struct {
            uint32_t awb1_p3_bg                    :    10;
            uint32_t reserved10                    :    6;
            uint32_t awb1_p3_rg                    :    10;
            uint32_t reserved26                    :    6;
        };
        uint32_t val;
    } awb1_p3;
    union {
        struct {
            uint32_t awb1_p4_bg                    :    10;
            uint32_t reserved10                    :    6;
            uint32_t awb1_p4_rg                    :    10;
            uint32_t reserved26                    :    6;
        };
        uint32_t val;
    } awb1_p4;
    union {
        struct {
            uint32_t awb1_p5_bg                    :    10;
            uint32_t reserved10                    :    6;
            uint32_t awb1_p5_rg                    :    10;
            uint32_t reserved26                    :    6;
        };
        uint32_t val;
    } awb1_p5;
    union {
        struct {
            uint32_t awb1_p6_bg                    :    10;
            uint32_t reserved10                    :    6;
            uint32_t awb1_p6_rg                    :    10;
            uint32_t reserved26                    :    6;
        };
        uint32_t val;
    } awb1_p6;
    union {
        struct {
            uint32_t awb1_p7_bg                    :    10;
            uint32_t reserved10                    :    6;
            uint32_t awb1_p7_rg                    :    10;
            uint32_t reserved26                    :    6;
        };
        uint32_t val;
    } awb1_p7;
    union {
        struct {
            uint32_t awb0_white_cnt                :    24;  /*number of white point detected of algo0*/
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } awb0_white_cnt;
    uint32_t awb0_acc_r;
    uint32_t awb0_acc_g;
    uint32_t awb0_acc_b;
    union {
        struct {
            uint32_t awb1_cnt0                     :    24;  /*number of pixels in color temperature point p0 of algo1*/
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } awb1_cnt0;
    union {
        struct {
            uint32_t awb1_cnt1                     :    24;
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } awb1_cnt1;
    union {
        struct {
            uint32_t awb1_cnt2                     :    24;
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } awb1_cnt2;
    union {
        struct {
            uint32_t awb1_cnt3                     :    24;
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } awb1_cnt3;
    union {
        struct {
            uint32_t awb1_cnt4                     :    24;
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } awb1_cnt4;
    union {
        struct {
            uint32_t awb1_cnt5                     :    24;
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } awb1_cnt5;
    union {
        struct {
            uint32_t awb1_cnt6                     :    24;
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } awb1_cnt6;
    union {
        struct {
            uint32_t awb1_cnt7                     :    24;
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } awb1_cnt7;
    uint32_t awb1_acc0_r;
    uint32_t awb1_acc0_g;
    uint32_t awb1_acc0_b;
    uint32_t awb1_acc1_r;
    uint32_t awb1_acc1_g;
    uint32_t awb1_acc1_b;
    uint32_t awb1_acc2_r;
    uint32_t awb1_acc2_g;
    uint32_t awb1_acc2_b;
    uint32_t awb1_acc3_r;
    uint32_t awb1_acc3_g;
    uint32_t awb1_acc3_b;
    uint32_t awb1_acc4_r;
    uint32_t awb1_acc4_g;
    uint32_t awb1_acc4_b;
    uint32_t awb1_acc5_r;
    uint32_t awb1_acc5_g;
    uint32_t awb1_acc5_b;
    uint32_t awb1_acc6_r;
    uint32_t awb1_acc6_g;
    uint32_t awb1_acc6_b;
    uint32_t awb1_acc7_r;
    uint32_t awb1_acc7_g;
    uint32_t awb1_acc7_b;
    union {
        struct {
            uint32_t color_saturation              :    8;
            uint32_t color_hue                     :    8;
            uint32_t color_contrast                :    8;
            uint32_t color_brightness              :    8;
        };
        uint32_t val;
    } color_ctrl;
    union {
        struct {
            uint32_t blc_r3_value                  :    8;
            uint32_t blc_r2_value                  :    8;
            uint32_t blc_r1_value                  :    8;
            uint32_t blc_r0_value                  :    8;
        };
        uint32_t val;
    } blc_value;
    union {
        struct {
            uint32_t blc_r3_stretch                :    1;
            uint32_t blc_r2_stretch                :    1;
            uint32_t blc_r1_stretch                :    1;
            uint32_t blc_r0_stretch                :    1;
            uint32_t reserved4                     :    28;
        };
        uint32_t val;
    } blc_ctrl0;
    union {
        struct {
            uint32_t blc_window_top                :    11;  /*blc average calculation window top*/
            uint32_t blc_window_left               :    11;  /*blc average calculation window left*/
            uint32_t blc_window_vnum               :    4;  /*blc average calculation window vnum*/
            uint32_t blc_window_hnum               :    4;  /*blc average calculation window hnum*/
            uint32_t blc_filter_en                 :    1;  /*enable blc average input filter*/
            uint32_t reserved31                    :    1;
        };
        uint32_t val;
    } blc_ctrl1;
    union {
        struct {
            uint32_t blc_r3_th                     :    8;
            uint32_t blc_r2_th                     :    8;
            uint32_t blc_r1_th                     :    8;
            uint32_t blc_r0_th                     :    8;
        };
        uint32_t val;
    } blc_ctrl2;
    union {
        struct {
            uint32_t blc_r3_mean                   :    8;
            uint32_t blc_r2_mean                   :    8;
            uint32_t blc_r1_mean                   :    8;
            uint32_t blc_r0_mean                   :    8;
        };
        uint32_t val;
    } blc_mean;
    union {
        struct {
            uint32_t hist_mode                     :    3;  /*statistic mode. 0: RAW_B, 1: RAW_GB, 2: RAW_GR 3: RAW_R, 4: RGB, 5:YUV_Y, 6:YUV_U, 7:YUV_V*/
            uint32_t reserved3                     :    29;
        };
        uint32_t val;
    } hist_mode;
    union {
        struct {
            uint32_t hist_coeff_b                  :    8;  /*coefficient of B when set hist_mode to RGB, sum of coeff_r and coeff_g and coeff_b should be 256*/
            uint32_t hist_coeff_g                  :    8;  /*coefficient of G when set hist_mode to RGB, sum of coeff_r and coeff_g and coeff_b should be 256*/
            uint32_t hist_coeff_r                  :    8;  /*coefficient of R when set hist_mode to RGB, sum of coeff_r and coeff_g and coeff_b should be 256*/
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } hist_coeff;
    union {
        struct {
            uint32_t hist_y_offs                   :    12;  /*y coordinate of first window*/
            uint32_t reserved12                    :    4;
            uint32_t hist_x_offs                   :    12;  /*x coordinate of first window*/
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } hist_offs;
    union {
        struct {
            uint32_t hist_y_size                   :    9;  /*y direction size of subwindow*/
            uint32_t reserved9                     :    7;
            uint32_t hist_x_size                   :    9;  /*x direction size of subwindow, should >= 18*/
            uint32_t reserved25                    :    7;
        };
        uint32_t val;
    } hist_size;
    union {
        struct {
            uint32_t hist_seg_3_4                  :    8;  /*threshold of histogram bin 3 and bin 4*/
            uint32_t hist_seg_2_3                  :    8;  /*threshold of histogram bin 2 and bin 3*/
            uint32_t hist_seg_1_2                  :    8;  /*threshold of histogram bin 1 and bin 2*/
            uint32_t hist_seg_0_1                  :    8;  /*threshold of histogram bin 0 and bin 1*/
        };
        uint32_t val;
    } hist_seg0;
    union {
        struct {
            uint32_t hist_seg_7_8                  :    8;  /*threshold of histogram bin 7 and bin 8*/
            uint32_t hist_seg_6_7                  :    8;  /*threshold of histogram bin 6 and bin 7*/
            uint32_t hist_seg_5_6                  :    8;  /*threshold of histogram bin 5 and bin 6*/
            uint32_t hist_seg_4_5                  :    8;  /*threshold of histogram bin 4 and bin 5*/
        };
        uint32_t val;
    } hist_seg1;
    union {
        struct {
            uint32_t hist_seg_11_12                :    8;  /*threshold of histogram bin 11 and bin 12*/
            uint32_t hist_seg_10_11                :    8;  /*threshold of histogram bin 10 and bin 11*/
            uint32_t hist_seg_9_10                 :    8;  /*threshold of histogram bin 9 and bin 10*/
            uint32_t hist_seg_8_9                  :    8;  /*threshold of histogram bin 8 and bin 9*/
        };
        uint32_t val;
    } hist_seg2;
    union {
        struct {
            uint32_t hist_seg_14_15                :    8;  /*threshold of histogram bin 14 and bin 15*/
            uint32_t hist_seg_13_14                :    8;  /*threshold of histogram bin 13 and bin 14*/
            uint32_t hist_seg_12_13                :    8;  /*threshold of histogram bin 12 and bin 13*/
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } hist_seg3;
    union {
        struct {
            uint32_t hist_weight_03                :    8;
            uint32_t hist_weight_02                :    8;
            uint32_t hist_weight_01                :    8;
            uint32_t hist_weight_00                :    8;  /*weight of subwindow 00 and sum of all weight should be 256*/
        };
        uint32_t val;
    } hist_weight0;
    union {
        struct {
            uint32_t hist_weight_12                :    8;
            uint32_t hist_weight_11                :    8;
            uint32_t hist_weight_10                :    8;
            uint32_t hist_weight_04                :    8;
        };
        uint32_t val;
    } hist_weight1;
    union {
        struct {
            uint32_t hist_weight_21                :    8;
            uint32_t hist_weight_20                :    8;
            uint32_t hist_weight_14                :    8;
            uint32_t hist_weight_13                :    8;
        };
        uint32_t val;
    } hist_weight2;
    union {
        struct {
            uint32_t hist_weight_30                :    8;
            uint32_t hist_weight_24                :    8;
            uint32_t hist_weight_23                :    8;
            uint32_t hist_weight_22                :    8;
        };
        uint32_t val;
    } hist_weight3;
    union {
        struct {
            uint32_t hist_weight_34                :    8;
            uint32_t hist_weight_33                :    8;
            uint32_t hist_weight_32                :    8;
            uint32_t hist_weight_31                :    8;
        };
        uint32_t val;
    } hist_weight4;
    union {
        struct {
            uint32_t hist_weight_43                :    8;
            uint32_t hist_weight_42                :    8;
            uint32_t hist_weight_41                :    8;
            uint32_t hist_weight_40                :    8;
        };
        uint32_t val;
    } hist_weight5;
    union {
        struct {
            uint32_t hist_weight_44                :    8;
            uint32_t reserved8                     :    24;
        };
        uint32_t val;
    } hist_weight6;
    union {
        struct {
            uint32_t hist_bin_0                    :    17;  /*result of histogram bin 0*/
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } hist_bin0;
    union {
        struct {
            uint32_t hist_bin_1                    :    17;
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } hist_bin1;
    union {
        struct {
            uint32_t hist_bin_2                    :    17;
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } hist_bin2;
    union {
        struct {
            uint32_t hist_bin_3                    :    17;
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } hist_bin3;
    union {
        struct {
            uint32_t hist_bin_4                    :    17;
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } hist_bin4;
    union {
        struct {
            uint32_t hist_bin_5                    :    17;
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } hist_bin5;
    union {
        struct {
            uint32_t hist_bin_6                    :    17;
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } hist_bin6;
    union {
        struct {
            uint32_t hist_bin_7                    :    17;
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } hist_bin7;
    union {
        struct {
            uint32_t hist_bin_8                    :    17;
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } hist_bin8;
    union {
        struct {
            uint32_t hist_bin_9                    :    17;
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } hist_bin9;
    union {
        struct {
            uint32_t hist_bin_10                   :    17;
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } hist_bin10;
    union {
        struct {
            uint32_t hist_bin_11                   :    17;
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } hist_bin11;
    union {
        struct {
            uint32_t hist_bin_12                   :    17;
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } hist_bin12;
    union {
        struct {
            uint32_t hist_bin_13                   :    17;
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } hist_bin13;
    union {
        struct {
            uint32_t hist_bin_14                   :    17;
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } hist_bin14;
    union {
        struct {
            uint32_t hist_bin_15                   :    17;
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } hist_bin15;
    union {
        struct {
            uint32_t header_mem_aux_ctrl           :    14;
            uint32_t reserved14                    :    2;
            uint32_t dpc_lut_mem_aux_ctrl          :    14;
            uint32_t reserved30                    :    2;
        };
        uint32_t val;
    } mem_aux_ctrl_0;
    union {
        struct {
            uint32_t lsc_lut_r_gr_mem_aux_ctrl     :    14;
            uint32_t reserved14                    :    2;
            uint32_t lsc_lut_gb_b_mem_aux_ctrl     :    14;
            uint32_t reserved30                    :    2;
        };
        uint32_t val;
    } mem_aux_ctrl_1;
    union {
        struct {
            uint32_t dpc_matrix_mem_aux_ctrl_1     :    14;
            uint32_t reserved14                    :    2;
            uint32_t dpc_matrix_mem_aux_ctrl_0     :    14;
            uint32_t reserved30                    :    2;
        };
        uint32_t val;
    } mem_aux_ctrl_2;
    union {
        struct {
            uint32_t bf_matrix_mem_aux_ctrl_1      :    14;
            uint32_t reserved14                    :    2;
            uint32_t bf_matrix_mem_aux_ctrl_0      :    14;
            uint32_t reserved30                    :    2;
        };
        uint32_t val;
    } mem_aux_ctrl_3;
    union {
        struct {
            uint32_t demosaic_matrix_mem_aux_ctrl_1:    14;
            uint32_t reserved14                    :    2;
            uint32_t demosaic_matrix_mem_aux_ctrl_0:    14;
            uint32_t reserved30                    :    2;
        };
        uint32_t val;
    } mem_aux_ctrl_4;
    union {
        struct {
            uint32_t sharp_matrix_y_mem_aux_ctrl_1 :    14;
            uint32_t reserved14                    :    2;
            uint32_t sharp_matrix_y_mem_aux_ctrl_0 :    14;
            uint32_t reserved30                    :    2;
        };
        uint32_t val;
    } mem_aux_ctrl_5;
    union {
        struct {
            uint32_t sharp_matrix_uv_mem_aux_ctrl_1:    14;
            uint32_t reserved14                    :    2;
            uint32_t sharp_matrix_uv_mem_aux_ctrl_0:    14;
            uint32_t reserved30                    :    2;
        };
        uint32_t val;
    } mem_aux_ctrl_6;
    union {
        struct {
            uint32_t yuv_mode                      :    1;
            uint32_t yuv_range                     :    1;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } yuv_format;
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
            uint32_t mem_clk_force_on              :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } mem_clk_ctrl;
    union {
        struct {
            uint32_t idi_yuv                       :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } cntl1;
} isp_dev_t;
extern isp_dev_t ISP;
#ifdef __cplusplus
}
#endif



#endif /*_SOC_ISP_STRUCT_H_ */
