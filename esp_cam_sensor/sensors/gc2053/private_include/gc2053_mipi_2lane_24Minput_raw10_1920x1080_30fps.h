/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gc2053_types.h"
#include "gc2053_regs.h"
/* window_size = 1920*1080 mipi@2lane
 * mclk = 24 MHz, mipi_clk = 594 Mbps
 * pixel_line_total = 2200, line_frame_total = 1125
 * row_time = 29.629 us, frame_rate = 30 fps
 *
 * Each entry below preserves the per-bit comments from the factory
 * "release_V1_GC2053_MIPI_2lane_base24M_30fps_20181206.txt" so future
 * debugging can read the bit meaning directly here. */
static const gc2053_reginfo_t init_reglist_MIPI_2lane_24Minput_RAW10_1920x1080_30fps[] = {
    /****system****/
    {0xfe, 0x80},
    {0xfe, 0x80},

    {0xfe, 0x00},
    {0xf2, 0x00}, /* [1]I2C_open_ena [0]pwd_dn */
    {0xf3, 0x00}, /* 0f//00 [3]Sdata_pad_io [2:0]Ssync_pad_io */
    {0xf4, 0x36}, /* [6:4]pll_ldo_set */
    {0xf5, 0xc0}, /* [7]soc_mclk_enable [6]pll_ldo_en [5:4]cp_clk_sel [3:0]cp_clk_div */
    {0xf6, 0x44}, /* [7:3]wpllclk_div [2:0]refmp_div */
    {0xf7, 0x01}, /* [7]refdiv2d5_en [6]refdiv1d5_en [5:4]scaler_mode [3]refmp_enb [1]div2en [0]pllmp_en */
    {0xf8, 0x63}, /* 2c// [7:0]pllmp_div */
    {0xf9, 0x40}, /* [7:3]rpllclk_div [2:1]pllmp_prediv [0]analog_pwc */
    {0xfc, 0x8e},

    /****CISCTL & ANALOG (page 0)****/
    {0xfe, 0x00},
    {0x87, 0x18}, /* [6]aec_delay_mode */
    {0xee, 0x30}, /* [5:4]dwen_sramen */
    {0xd0, 0xb7}, /* ramp_en */
    {0x03, 0x02},
    {0x04, 0xa3},
    {0x05, 0x04}, /* 05 */
    {0x06, 0x4c}, /* 2a//60// [11:0]hb */
    {0x07, 0x00},
    {0x08, 0x11}, /* 19 */
    {0x09, 0x00},
    {0x0a, 0x02}, /* cisctl row start */
    {0x0b, 0x00},
    {0x0c, 0x02}, /* cisctl col start */
    {0x0d, 0x04},
    {0x0e, 0x40},
    {0x12, 0xe2}, /* vsync_ahead_mode */
    {0x13, 0x16},
    {0x19, 0x0a}, /* ad_pipe_num */
    {0x21, 0x1c}, /* eqc1fc_eqc2fc_sw */
    {0x28, 0x0a}, /* 16//eqc2_c2clpen_sw */
    {0x29, 0x24}, /* eq_post_width */
    {0x2b, 0x04}, /* c2clpen --eqc2 */
    {0x32, 0xf8}, /* [5]txh_en ->avdd28 */
    {0x37, 0x03}, /* [3:2]eqc2sel=0 */
    {0x39, 0x15}, /* 17//[3:0]rsgl */
    {0x41, 0x04}, /* VTS[13:8] for 30fps mipi */
    {0x42, 0x65}, /* VTS[7:0]  for 30fps mipi */
    {0x43, 0x07}, /* vclamp */
    {0x44, 0x40}, /* 0e//post_tx_width */
    {0x46, 0x0b}, /* txh---3.2v */
    {0x4b, 0x20}, /* rst_tx_width */
    {0x4e, 0x08}, /* 12//ramp_t1_width */
    {0x55, 0x20}, /* read_tx_width_pp */
    {0x66, 0x05}, /* 18//stspd_width_r1 */
    {0x67, 0x05}, /* 40//5//stspd_width_r */
    {0x77, 0x01}, /* dacin offset x31 */
    {0x78, 0x00}, /* dacin offset */
    {0x7c, 0x93}, /* [1:0] co1comp */
    {0x8c, 0x12}, /* 12 ramp_t1_ref */
    {0x8d, 0x92}, /* 90 */
    {0x90, 0x00}, /* use frame length to change fps */
    {0x9d, 0x10},
    {0xce, 0x7c}, /* 70//78//[4:2]c1isel */
    {0xd2, 0x41}, /* [5:3]c2clamp */
    {0xd3, 0xdc}, /* ec//0x39[7]=0,0xd3[3]=1 rsgh=vref */
    {0xe6, 0x50}, /* ramps offset */

    /*gain*/
    {0xb6, 0xc0},
    {0xb0, 0x70},
    {0xb1, 0x01},
    {0xb2, 0x00},
    {0xb3, 0x00},
    {0xb4, 0x00},
    {0xb8, 0x01},
    {0xb9, 0x00},

    /*blk*/
    {0x26, 0x30}, /* 23//[4]写0，全n mode */
    {0xfe, 0x01},
    {0x40, 0x23},
    {0x55, 0x07},
    {0x60, 0x40}, /* [7:0]WB_offset */
    {0xfe, 0x04},
    /* White balance channel ratios.
     * Previous attempt (R=0xC0, B=0xC0, G=0x60) over-compensated:
     * with bayer demosaic at object edges, the high R/B ratios amplified
     * the demosaic interpolation error and produced a "thermal image"
     * false-colour look at high-contrast boundaries.
     * Tone the ratio back closer to 1:1:1:1 and let the ISP clean up. */
    {0x14, 0x10}, /* g1 ratio */
    {0x15, 0x90}, /* r ratio  (mild lift) */
    {0x16, 0x90}, /* b ratio  (mild lift) */
    {0x17, 0x10}, /* g2 ratio */

    /*window (page 1)*/
    {0xfe, 0x01},
    {0x92, 0x00}, /* win y1 */
    {0x94, 0x03}, /* win x1 */
    {0x95, 0x04},
    {0x96, 0x38}, /* [10:0]out_height */
    {0x97, 0x07},
    {0x98, 0x80}, /* [11:0]out_width */

    /*ISP (page 1)*/
    /* ISP / CCM tuning for GC2053 on P4 ISP (DPC and BFF off, BFF SRAM
     * mode default, DD off -> chip outputs unprocessed RAW; the P4 ISP
     * does the colour work). 0x07..0x0a are the four signed 8-bit
     * channel gains in the order R / Gr / Gb / B. Encoding is
     *     coeff = 1.0 + (signed_value) / 128
     * so 0x00 == identity (1.0), 0x40 == +0.5 (1.5), 0x80 == 0.0 (off),
     * 0xA6 == -90/128 -> 0.297, etc. Channel-equal values keep the
     * sensor's native colour; an imbalance shows up as a colour cast
     * (e.g. R/B smaller than G -> greenish, G smaller -> magenta).
     * f400224 left these at ~0.30 which made R/B weaker than G and
     * produced a green cast. 0x00 in all four slots restores identity.
     * 0x0b/0x0c are post-CCM sharpen strength; release V1 ships them
     * very low (0x01 / 0x84) to avoid halos. 0x0f is the post-CCM gain,
     * 0x50 is the AE target luma, 0x89 is the ISP control reg.
     * The 0x3c line in the gamma block is intentionally NOT written -
     * the power-on default is required for image to appear. */
    {0xfe, 0x01},
    {0x01, 0x00}, /* DPC off, no noise blend, no center filter         */
    {0x02, 0x00}, /* BFF SRAM off                                     */
    {0x04, 0x00}, /* DD (digital divider) off                          */
    {0x07, 0x00}, /* R  channel gain = 1.0  (was 0xa6 -> 0.297)        */
    {0x08, 0x00}, /* Gr channel gain = 1.0  (was 0xa9 -> 0.320)        */
    {0x09, 0x00}, /* Gb channel gain = 1.0  (was 0xa8 -> 0.313)        */
    {0x0a, 0x00}, /* B  channel gain = 1.0  (was 0xa7 -> 0.305)        */
    {0x0b, 0x01}, /* sharpen M-coeff weak (release V1; was 0xff)      */
    {0x0c, 0x84}, /* sharpen H-coeff weak (release V1; was 0xff)      */
    {0x0f, 0x00}, /* post-CCM gain (no extra gain)                    */
    {0x50, 0x1c}, /* AE target luma = 113                             */
    {0x89, 0x03}, /* ISP control                                      */
    {0xfe, 0x04},
    {0x28, 0x86},
    {0x29, 0x86},
    {0x2a, 0x86},
    {0x2b, 0x68},
    {0x2c, 0x68},
    {0x2d, 0x68},
    {0x2e, 0x68},
    {0x2f, 0x68},
    {0x30, 0x4f},
    {0x31, 0x68},
    {0x32, 0x67},
    {0x33, 0x66},
    {0x34, 0x66},
    {0x35, 0x66},
    {0x36, 0x66},
    {0x37, 0x66},
    {0x38, 0x62},
    {0x39, 0x62},
    {0x3a, 0x62},
    {0x3b, 0x62},
    {0x3c, 0x62},
    {0x3d, 0x62},
    {0x3e, 0x62},
    {0x3f, 0x62},

    /****DVP & MIPI****/
    {0xfe, 0x01},
    {0x9a, 0x06}, /* [5]OUT_gate_mode [4]hsync_delay_half_pclk [3]data_delay_half_pclk [2]vsync_polarity [1]hsync_polarity [0]pclk_out_polarity */
    {0xfe, 0x00},
    {0x7b, 0x2a}, /* [7:6]updn [5:4]drv_high_data [3:2]drv_low_data [1:0]drv_pclk */
    {0x23, 0x2d}, /* [3]rst_rc [2:1]drv_sync [0]pwd_rc */
    {0xfe, 0x03},
    {0x01, 0x27}, /* 20//27 [6:5]clkctr [2]phy-lane1_en [1]phy-lane0_en [0]phy_clk_en */
    {0x02, 0x56}, /* [7:6]data1ctr [5:4]data0ctr [3:0]mipi_diff */
    {0x03, 0x8e}, /* Linux driver value: [7]clklane_p2s_sel [6:5]data0hs_ph [4]data0_delay1s [3]clkdelay1s [2]mipi_en [1:0]clkhs_ph */
    {0x12, 0x80},
    {0x13, 0x07},
    {0x15, 0x12}, /* [1:0]clk_lane_mode */
    {0xfe, 0x00},
    /* 0x3e global enable:
     *   bit7 = lane_ena       (1)
     *   bit6 = DVPBUF_ena     (0)
     *   bit5 = ULPEna         (0)
     *   bit4 = MIPI_ena       (1)  <- MIPI enable
     *   bit3 = mipi_set_auto_disable (0)
     *   bit2 = RAW8_mode      (0)
     *   bit1 = line_sync_mode (0)
     *   bit0 = double_lane_en (1)  <- 2-lane
     * -> 0x91 = 1001_0001
     * The factory value is correct; the earlier "no MIPI" symptom was
     * caused by sensor clearing 0x3e when the sleep bit toggled, which
     * is fixed by re-asserting 0x3e in gc2053_set_stream. */
    {0x3e, 0x11},

    {GC2053_REG_END, 0x00},
};
