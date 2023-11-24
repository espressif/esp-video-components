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
#ifndef _SOC_PPA_STRUCT_H_
#define _SOC_PPA_STRUCT_H_


#ifdef __cplusplus
extern "C" {
#endif

typedef volatile struct {
    uint32_t blend0_clut_data;
    uint32_t blend1_clut_data;
    uint32_t reserved_8;
    union {
        struct {
            uint32_t apb_fifo_mask                 :    1;  /*1'b0: fifo mode to wr/rd clut0/clut1 RAM through register PPA_SR_CLUT_DATA_REG/PPA_BLEND0_CLUT_DATA_REG/PPA_BLEND1_CLUT_DATA_REG. 1'b1: memory mode to wr/rd sr/blend0/blend1 clut RAM. The bit 11 and 10 of the waddr should be 01 to access sr clut and should be 10 to access blend0 clut and should be 11 to access blend 1 clut in memory mode.*/
            uint32_t blend0_clut_mem_rst           :    1;  /*Write 1 then write 0 to this bit to reset BLEND0 CLUT.*/
            uint32_t blend1_clut_mem_rst           :    1;  /*Write 1 then write 0 to this bit to reset BLEND1 CLUT.*/
            uint32_t blend0_clut_mem_rdaddr_rst    :    1;  /*Write 1 then write 0 to reset the read address of BLEND0 CLUT in fifo mode.*/
            uint32_t blend1_clut_mem_rdaddr_rst    :    1;  /*Write 1 then write 0 to reset the read address of BLEND1 CLUT in fifo mode.*/
            uint32_t blend_clut_mem_force_pd       :    1;  /*1: force power down BLEND CLUT memory.*/
            uint32_t blend_clut_mem_force_pu       :    1;  /*1: force power up BLEND CLUT memory.*/
            uint32_t blend_clut_mem_clk_ena        :    1;  /*1: Force clock on for BLEND CLUT memory.*/
            uint32_t reserved8                     :    24;
        };
        uint32_t val;
    } clut_conf;
    union {
        struct {
            uint32_t sr_eof                        :    1;  /*The raw interrupt bit turns to high level when scaling and rotating engine calculate one frame image.*/
            uint32_t blend_eof                     :    1;  /*The raw interrupt bit turns to high level when blending engine calculate one frame image.*/
            uint32_t sr_param_cfg_err              :    1;  /*The raw interrupt bit turns to high level when the configured scaling and rotating coefficient is wrong. User can check the reasons through register PPA_SR_PARAM_ERR_ST_REG.  */
            uint32_t reserved3                     :    29;  /*reserved*/
        };
        uint32_t val;
    } int_raw;
    union {
        struct {
            uint32_t sr_eof                        :    1;  /*The raw interrupt status bit for the PPA_SR_EOF_INT interrupt.*/
            uint32_t blend_eof                     :    1;  /*The raw interrupt status bit for the PPA_BLEND_EOF_INT interrupt.*/
            uint32_t sr_param_cfg_err              :    1;  /*The raw interrupt status bit for the PPA_SR_RX_YSCAL_ERR_INT interrupt.*/
            uint32_t reserved3                     :    29;  /*reserved*/
        };
        uint32_t val;
    } int_st;
    union {
        struct {
            uint32_t sr_eof                        :    1;  /*The interrupt enable bit for the PPA_SR_EOF_INT interrupt.*/
            uint32_t blend_eof                     :    1;  /*The interrupt enable bit for the PPA_BLEND_EOF_INT interrupt.*/
            uint32_t sr_param_cfg_err              :    1;  /*The interrupt enable bit for the PPA_SR_RX_YSCAL_ERR_INT interrupt.*/
            uint32_t reserved3                     :    29;  /*reserved*/
        };
        uint32_t val;
    } int_ena;
    union {
        struct {
            uint32_t sr_eof                        :    1;  /*Set this bit to clear the PPA_SR_EOF_INT interrupt.*/
            uint32_t blend_eof                     :    1;  /*Set this bit to clear the PPA_BLEND_EOF_INT interrupt.*/
            uint32_t sr_param_cfg_err              :    1;  /*Set this bit to clear the PPA_SR_RX_YSCAL_ERR_INT interrupt.*/
            uint32_t reserved3                     :    29;  /*reserved*/
        };
        uint32_t val;
    } int_clr;
    union {
        struct {
            uint32_t sr_rx_cm                      :    4;  /*The source image color mode for Scaling and Rotating engine Rx. 0: ARGB8888. 1: RGB888. 2: RGB565. 3: Reserved. 4: L8. 5: L4.*/
            uint32_t sr_tx_cm                      :    4;  /*The destination image color mode for Scaling and Rotating engine Tx. 0: ARGB8888. 1: RGB888. 2: RGB565. 3: Reserved.*/
            uint32_t reserved8                     :    24;  /*reserved*/
        };
        uint32_t val;
    } sr_color_mode;
    union {
        struct {
            uint32_t blend0_rx_cm                  :    4;  /*The source image color mode for background plane. 0: ARGB8888. 1: RGB888. 2: RGB565. 3: Reserved. 4: L8. 5: L4.*/
            uint32_t blend1_rx_cm                  :    4;  /*The source image color mode for foreground plane. 0: ARGB8888. 1: RGB888. 2: RGB565. 3: Reserved. 4: L8. 5: L4. 6: A8. 7: A4.*/
            uint32_t blend_tx_cm                   :    4;  /*The destination image color mode for output of blender. 0: ARGB8888. 1: RGB888. 2: RGB565. 3: Reserved..*/
            uint32_t reserved12                    :    20;  /*reserved*/
        };
        uint32_t val;
    } blend_color_mode;
    union {
        struct {
            uint32_t sr_rx_byte_swap_en            :    1;  /*Set this bit to 1  the data into Rx channel 0 would be swapped in byte. The Byte0 and Byte1 would be swapped while byte 2 and byte 3 would be swappped.*/
            uint32_t sr_rx_rgb_swap_en             :    1;  /*Set this bit to 1  the data into Rx channel 0 would be swapped in rgb. It means rgb would be swap to bgr.*/
            uint32_t sr_macro_bk_ro_bypass         :    1;  /*Set this bit to 1 to bypass the macro block order function. This function is used to improve efficient accessing external memory.*/
            uint32_t reserved3                     :    29;
        };
        uint32_t val;
    } sr_byte_order;
    union {
        struct {
            uint32_t blend0_rx_byte_swap_en        :    1;  /*Set this bit to 1  the data into Rx channel 0 would be swapped in byte. The Byte0 and Byte1 would be swapped while byte 2 and byte 3 would be swappped.*/
            uint32_t blend1_rx_byte_swap_en        :    1;  /*Set this bit to 1  the data into Rx channel 0 would be swapped in byte. The Byte0 and Byte1 would be swapped while byte 2 and byte 3 would be swappped.*/
            uint32_t blend0_rx_rgb_swap_en         :    1;  /*Set this bit to 1  the data into Rx channel 0 would be swapped in rgb. It means rgb would be swap to bgr.*/
            uint32_t blend1_rx_rgb_swap_en         :    1;  /*Set this bit to 1  the data into Rx channel 0 would be swapped in rgb. It means rgb would be swap to bgr.*/
            uint32_t reserved4                     :    28;
        };
        uint32_t val;
    } blend_byte_order;
    uint32_t reserved_30;
    union {
        struct {
            uint32_t blend_en                      :    1;  /*Set this bit to enable alpha blending. */
            uint32_t blend_bypass                  :    1;  /*Set this bit to bypass blender. Then background date would be output.*/
            uint32_t blend_fix_pixel_fill_en       :    1;  /*This bit is used to enable fix pixel filling. When this mode is enable  only Tx channel is work and the output pixel is configured by PPA_OUT_FIX_PIXEL.*/
            uint32_t blend_trans_mode_update       :    1;  /*Set this bit to update the transfer mode. Only the bit is set  the transfer mode is valid.*/
            uint32_t blend_rst                     :    1;  /*write 1 then write 0 to reset blending engine.*/
            uint32_t reserved5                     :    27;  /*The vertical width of image block that would be filled in fix pixel filling mode. The unit is pixel*/
        };
        uint32_t val;
    } blend_trans_mode;
    union {
        struct {
            uint32_t sr_rx_fix_alpha               :    8;  /*The value would replace the alpha value in received pixel for Scaling and Rotating engine when PPA_SR_RX_ALPHA_CONF_EN is enabled.*/
            uint32_t sr_rx_alpha_mod               :    2;  /*Alpha mode. 0/3: not replace alpha. 1: replace alpha with PPA_SR_FIX_ALPHA. 2: Original alpha multiply with PPA_SR_FIX_ALPHA/256. */
            uint32_t sr_rx_alpha_inv               :    1;  /*Set this bit to invert the original alpha value. When RX color mode is RGB565/RGB88. The original alpha value is 255.*/
            uint32_t reserved11                    :    21;  /*reserved*/
        };
        uint32_t val;
    } sr_fix_alpha;
    union {
        struct {
            uint32_t blend_hb                      :    14;  /*The horizontal width of image block that would be filled in fix pixel filling mode. The unit is pixel*/
            uint32_t blend_vb                      :    14;  /*The vertical width of image block that would be filled in fix pixel filling mode. The unit is pixel*/
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } blend_tx_size;
    union {
        struct {
            uint32_t blend0_rx_fix_alpha           :    8;  /*The value would replace the alpha value in received pixel for background plane of blender when PPA_BLEND0_RX_ALPHA_CONF_EN is enabled.*/
            uint32_t blend1_rx_fix_alpha           :    8;  /*The value would replace the alpha value in received pixel for foreground plane of blender when PPA_BLEND1_RX_ALPHA_CONF_EN is enabled.*/
            uint32_t blend0_rx_alpha_mod           :    2;  /*Alpha mode. 0/3: not replace alpha. 1: replace alpha with PPA_SR_FIX_ALPHA. 2: Original alpha multiply with PPA_SR_FIX_ALPHA/256. */
            uint32_t blend1_rx_alpha_mod           :    2;  /*Alpha mode. 0/3: not replace alpha. 1: replace alpha with PPA_SR_FIX_ALPHA. 2: Original alpha multiply with PPA_SR_FIX_ALPHA/256. */
            uint32_t blend0_rx_alpha_inv           :    1;  /*Set this bit to invert the original alpha value. When RX color mode is RGB565/RGB88. The original alpha value is 255.*/
            uint32_t blend1_rx_alpha_inv           :    1;  /*Set this bit to invert the original alpha value. When RX color mode is RGB565/RGB88. The original alpha value is 255.*/
            uint32_t reserved22                    :    10;  /*reserved*/
        };
        uint32_t val;
    } blend_fix_alpha;
    uint32_t reserved_44;
    union {
        struct {
            uint32_t blend1_rx_b                   :    8;  /*blue color for A4/A8 mode.*/
            uint32_t blend1_rx_g                   :    8;  /*green color for A4/A8 mode.*/
            uint32_t blend1_rx_r                   :    8;  /*red color for A4/A8 mode.*/
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } blend_rgb;
    uint32_t blend_fix_pixel;
    union {
        struct {
            uint32_t sr_scal_x_int                 :    8;  /*The integrated part of scaling coefficient in X direction.*/
            uint32_t sr_scal_x_frag                :    4;  /*The fragment part of scaling coefficient in X direction.*/
            uint32_t sr_scal_y_int                 :    8;  /*The integrated part of scaling coefficient in Y direction.*/
            uint32_t sr_scal_y_frag                :    4;  /*The fragment part of scaling coefficient in Y direction.*/
            uint32_t sr_rotate_angle               :    2;  /*The rotate angle. 0: 0 degree. 1: 90 degree. 2: 180 degree. 3: 270 degree.*/
            uint32_t scal_rotate_rst               :    1;  /*Write 1 then write 0 to this bit to reset scaling and rotating engine.*/
            uint32_t scal_rotate_start             :    1;  /*Write 1 to enable scaling and rotating engine after parameter is configured.*/
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } sr_scal_rotate;
    union {
        struct {
            uint32_t sr_mem_clk_ena                :    1;  /*Set this bit to force clock enable of scaling and rotating engine's data memory.*/
            uint32_t sr_mem_force_pd               :    1;  /*Set this bit to force power down scaling and rotating engine's data memory.*/
            uint32_t sr_mem_force_pu               :    1;  /*Set this bit to force power up scaling and rotating engine's data memory.*/
            uint32_t reserved3                     :    29;
        };
        uint32_t val;
    } sr_mem_pd;
    union {
        struct {
            uint32_t clk_en                        :    1;  /*PPA register clock gate enable signal.*/
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } conf;
    union {
        struct {
            uint32_t blend0_clut_cnt               :    9;  /*The write data counter of BLEND0 CLUT in fifo mode.*/
            uint32_t blend1_clut_cnt               :    9;  /*The write data counter of BLEND1 CLUT in fifo mode.*/
            uint32_t reserved18                    :    14;
        };
        uint32_t val;
    } clut_cnt;
    union {
        struct {
            uint32_t blend_size_diff_st            :    1;  /*1: indicate the size of two image is different.*/
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } blend_st;
    union {
        struct {
            uint32_t tx_dscr_vb_err_st             :    1;  /*The error is that the scaled VB plus the offset of Y coordinate in 2DDMA receive descriptor is larger than VA in 2DDMA receive descriptor. */
            uint32_t tx_dscr_hb_err_st             :    1;  /*The error is that the scaled HB plus the offset of X coordinate in 2DDMA receive descriptor is larger than HA in 2DDMA receive descriptor. */
            uint32_t y_rx_scal_equal_0_err_st      :    1;  /*The error is that the PPA_SR_SCAL_Y_INT and PPA_SR_CAL_Y_FRAG both are 0.*/
            uint32_t rx_dscr_vb_err_st             :    1;  /*The error is that VB in 2DDMA receive descriptor plus the offset of Y coordinate in 2DDMA transmit descriptor is larger than VA in 2DDMA transmit descriptor*/
            uint32_t ydst_len_too_samll_err_st     :    1;  /*The error is that the scaled image width is 0. For example.  when source width is 14. scaled value is 1/16. and no rotate operation. then scaled width would be 0 as the result would be floored.*/
            uint32_t ydst_len_too_large_err_st     :    1;  /*The error is that the scaled width is larger than (2^13 - 1).*/
            uint32_t x_rx_scal_equal_0_err_st      :    1;  /*The error is that the scaled image height is 0.*/
            uint32_t rx_dscr_hb_err_st             :    1;  /*The error is that the HB in 2DDMA transmit descriptor plus the offset of X coordinate in 2DDMA transmit descriptor is larger than HA in 2DDMA transmit descriptor. */
            uint32_t xdst_len_too_samll_err_st     :    1;  /*The error is that the scaled image height is 0. For example.  when source height is 14. scaled value is 1/16. and no rotate operation. then scaled height would be 0 as the result would be floored.*/
            uint32_t xdst_len_too_large_err_st     :    1;  /*The error is that the scaled image height is larger than (2^13 - 1).*/
            uint32_t reserved10                    :    22;
        };
        uint32_t val;
    } sr_param_err_st;
    union {
        struct {
            uint32_t sr_rx_dscr_sample_state       :    2;  /*Reserved.*/
            uint32_t sr_rx_scan_state              :    2;  /*Reserved.*/
            uint32_t sr_tx_dscr_sample_state       :    2;  /*Reserved.*/
            uint32_t sr_tx_scan_state              :    3;  /*Reserved.*/
            uint32_t reserved9                     :    23;
        };
        uint32_t val;
    } sr_status;
    uint32_t eco_low;
    uint32_t eco_high;
    union {
        struct {
            uint32_t rdn_result                    :    1;  /*Reserved.*/
            uint32_t rdn_ena                       :    1;  /*Reserved.*/
            uint32_t reserved2                     :    30;  /*Reserved.*/
        };
        uint32_t val;
    } eco_cell_ctrl;
    union {
        struct {
            uint32_t mem_aux_ctrl                  :    14;  /*Control signals*/
            uint32_t reserved14                    :    18;  /*Reserved.*/
        };
        uint32_t val;
    } sram_ctrl;
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
    uint32_t date;
} ppa_dev_t;
extern ppa_dev_t PPA;
#ifdef __cplusplus
}
#endif



#endif /*_SOC_PPA_STRUCT_H_ */
