/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h> /* For NULL declaration */
#include <stdint.h>
#include <stdbool.h>

#include "soc/lcd_cam_reg.h"
#include "soc/lcd_cam_struct.h"
#include "soc/system_struct.h"

#include "hal/assert.h"
#include "hal/cam_types.h"
#include "hal/misc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAM_LL_GET_HW(id) (((id) == 0) ? (&LCD_CAM) : NULL)

#define CAM_LL_CLK_FRAC_DIV_N_MAX  256 // CAM_CLK = CAM_CLK_S / (N + b/a), the N register is 8 bit-width
#define CAM_LL_CLK_FRAC_DIV_AB_MAX 64  // CAM_CLK = CAM_CLK_S / (N + b/a), the a/b register is 6 bit-width

/**
 * @brief Enable or disable the bus clock for the CAM module
 *
 * @param set_bit True to set bit, false to clear bit
 */
static inline void cam_ll_enable_bus_clock(int group_id, bool enable)
{
    (void)group_id;
    SYSTEM.perip_clk_en1.lcd_cam_clk_en = enable;
}

/// use a macro to wrap the function, force the caller to use it in a critical section
/// the critical section needs to declare the __DECLARE_RCC_RC_ATOMIC_ENV variable in advance
#define cam_ll_enable_bus_clock(...) (void)__DECLARE_RCC_RC_ATOMIC_ENV; cam_ll_enable_bus_clock(__VA_ARGS__)

/**
 * @brief Reset the CAM module
 */
static inline void cam_ll_reset_register(int group_id)
{
    (void)group_id;
    SYSTEM.perip_rst_en1.lcd_cam_rst = 0x01;
    SYSTEM.perip_rst_en1.lcd_cam_rst = 0x00;
}

/// use a macro to wrap the function, force the caller to use it in a critical section
/// the critical section needs to declare the __DECLARE_RCC_RC_ATOMIC_ENV variable in advance
#define cam_ll_reset_register(...) (void)__DECLARE_RCC_RC_ATOMIC_ENV; cam_ll_reset_register(__VA_ARGS__)

/**
 * @brief Enable clock gating
 *
 * @param dev CAM register base address
 * @param en True to enable, False to disable
 */
static inline void cam_ll_enable_clock(lcd_cam_dev_t *dev, bool en)
{
    (void)dev;
    (void)en;
}

/**
 * @brief Select clock source for CAM peripheral
 *
 * @param dev CAM register base address
 * @param src Clock source
 */
static inline void cam_ll_select_clk_src(lcd_cam_dev_t *dev, cam_clock_source_t src)
{
    switch (src) {
    case CAM_CLK_SRC_PLL160M:
        dev->cam_ctrl.cam_clk_sel = 3;
        break;
    case CAM_CLK_SRC_PLL240M:
        dev->cam_ctrl.cam_clk_sel = 2;
        break;
    case CAM_CLK_SRC_XTAL:
        dev->cam_ctrl.cam_clk_sel = 1;
        break;
    default:
        // disable CAM clock source
        dev->cam_ctrl.cam_clk_sel = 0;
        HAL_ASSERT(false);
        break;
    }
}

/**
 * @brief Set clock coefficient of CAM peripheral
 *
 * @param dev CAM register base address
 * @param div_num Integer part of the divider
 * @param div_a denominator of the divider
 * @param div_b numerator of the divider
 */
__attribute__((always_inline))
static inline void cam_ll_set_group_clock_coeff(lcd_cam_dev_t *dev, int div_num, int div_a, int div_b)
{
    // cam_clk = module_clock_src / (div_num + div_b / div_a)
    HAL_ASSERT(div_num >= 2 && div_num <= CAM_LL_CLK_FRAC_DIV_N_MAX);
    // dic_num == 0 means CAM_LL_CLK_FRAC_DIV_N_MAX divider in hardware
    if (div_num >= CAM_LL_CLK_FRAC_DIV_N_MAX) {
        div_num = 0;
    }
    HAL_FORCE_MODIFY_U32_REG_FIELD(dev->cam_ctrl, cam_clkm_div_num, div_num);
    dev->cam_ctrl.cam_clkm_div_a = div_a;
    dev->cam_ctrl.cam_clkm_div_b = div_b;
}

/**
 * @brief Enable stop signal for CAM peripheral
 *
 * @param dev CAM register base address
 * @param en True to stop when GDMA Rx FIFO is full, False to not stop
 */
static inline void cam_ll_enable_stop_signal(lcd_cam_dev_t *dev, bool en)
{
    dev->cam_ctrl.cam_stop_en = en;
}

/**
 * @brief Set vsync filter threshold value
 *
 * @param dev CAM register base address
 * @param value Filter threshold value for CAM_VSYNC_SIGNAL, range [0, 7]
 */
static inline void cam_ll_set_vsync_filter_thres(lcd_cam_dev_t *dev, uint32_t value)
{
    dev->cam_ctrl.cam_vsync_filter_thres = value;
}

/**
 * @brief Enable to generate LCD_CAM_CAM_HS_INT
 *
 * @param dev CAM register base address
 * @param en True to enable to generate LCD_CAM_CAM_HS_INT, False to disable
 */
static inline void cam_ll_enable_line_int(lcd_cam_dev_t *dev, bool en)
{
    dev->cam_ctrl.cam_line_int_en = en;
}

/**
 * @brief Enable CAM_VSYNC to generate in_suc_eof
 *
 * @param dev CAM register base address
 * @param en True to enable CAM_VSYNC to generate in_suc_eof, False to use LCD_CAM_CAM_REC_DATA_BYTELEN to control in_suc_eof
 */
static inline void cam_ll_enable_vsync_generate_eof(lcd_cam_dev_t *dev, bool en)
{
    dev->cam_ctrl.cam_vs_eof_en = en;
}

/**
 * @brief Enable to swap every two 8-bit input data
 *
 * @param dev CAM register base address
 * @param en True to enable invert, False to disable invert
 */
static inline void cam_ll_enable_8bits_data_invert(lcd_cam_dev_t *dev, bool en)
{
    dev->cam_rgb_yuv.cam_conv_8bits_data_inv = en;
}

/**
 * @brief Enable YUV-RGB converter
 *
 * @param dev CAM register base address
 * @param en True to enable converter, False to disable converter
 */
static inline void cam_ll_enable_rgb_yuv_convert(lcd_cam_dev_t *dev, bool en)
{
    dev->cam_rgb_yuv.cam_conv_bypass = en;
}

/**
 * @brief Set convert data line width
 *
 * @param dev CAM register base address
 * @param width data line width (8 or 16)
 */
static inline void cam_ll_set_convert_data_width(lcd_cam_dev_t *dev, uint32_t width)
{
    HAL_ASSERT(width == 8 || width == 16);
    dev->cam_rgb_yuv.cam_conv_mode_8bits_on = (width == 8) ? 1 : 0;
}

/**
 * @brief Set the color range of input data
 *
 * @param dev CAM register base address
 * @param range Color range
 */
static inline void cam_ll_set_input_color_range(lcd_cam_dev_t *dev, cam_color_range_t range)
{
    if (range == CAM_COLOR_RANGE_LIMIT) {
        dev->cam_rgb_yuv.cam_conv_data_in_mode = 0;
    } else if (range == CAM_COLOR_RANGE_FULL) {
        dev->cam_rgb_yuv.cam_conv_data_in_mode = 1;
    }
}

/**
 * @brief Set the color range of output data
 *
 * @param dev CAM register base address
 * @param range Color range
 */
static inline void cam_ll_set_output_color_range(lcd_cam_dev_t *dev, cam_color_range_t range)
{
    if (range == CAM_COLOR_RANGE_LIMIT) {
        dev->cam_rgb_yuv.cam_conv_data_out_mode = 0;
    } else if (range == CAM_COLOR_RANGE_FULL) {
        dev->cam_rgb_yuv.cam_conv_data_out_mode = 1;
    }
}

/**
 * @brief Set YUV conversion standard
 *
 * @param dev CAM register base address
 * @param std YUV conversion standard
 */
static inline void cam_ll_set_yuv_convert_std(lcd_cam_dev_t *dev, cam_yuv_conv_std_t std)
{
    if (std == CAM_YUV_CONV_STD_BT601) {
        dev->cam_rgb_yuv.cam_conv_protocol_mode = 0;
    } else if (std == CAM_YUV_CONV_STD_BT709) {
        dev->cam_rgb_yuv.cam_conv_protocol_mode = 1;
    }
}

/**
 * @brief Set the converter mode: RGB565 to YUV
 *
 * @param dev CAM register base address
 * @param yuv_sample YUV sample mode
 */
static inline void cam_ll_set_convert_mode_rgb_to_yuv(lcd_cam_dev_t *dev, cam_yuv_sample_t yuv_sample)
{
    dev->cam_rgb_yuv.cam_conv_trans_mode = 1;
    dev->cam_rgb_yuv.cam_conv_yuv2yuv_mode = 3;
    switch (yuv_sample) {
    case CAM_YUV_SAMPLE_422:
        dev->cam_rgb_yuv.cam_conv_yuv_mode = 0;
        break;
    case CAM_YUV_SAMPLE_420:
        dev->cam_rgb_yuv.cam_conv_yuv_mode = 1;
        break;
    case CAM_YUV_SAMPLE_411:
        dev->cam_rgb_yuv.cam_conv_yuv_mode = 2;
        break;
    default:
        abort();
    }
}

/**
 * @brief Set the converter mode: YUV to RGB565
 *
 * @param dev CAM register base address
 * @param yuv_sample YUV sample mode
 */
static inline void cam_ll_set_convert_mode_yuv_to_rgb(lcd_cam_dev_t *dev, cam_yuv_sample_t yuv_sample)
{
    dev->cam_rgb_yuv.cam_conv_trans_mode = 0;
    dev->cam_rgb_yuv.cam_conv_yuv2yuv_mode = 3;
    switch (yuv_sample) {
    case CAM_YUV_SAMPLE_422:
        dev->cam_rgb_yuv.cam_conv_yuv_mode = 0;
        break;
    case CAM_YUV_SAMPLE_420:
        dev->cam_rgb_yuv.cam_conv_yuv_mode = 1;
        break;
    case CAM_YUV_SAMPLE_411:
        dev->cam_rgb_yuv.cam_conv_yuv_mode = 2;
        break;
    default:
        abort();
    }
}

/**
 * @brief Set the converter mode: YUV to YUV
 *
 * @param dev CAM register base address
 * @param src_sample Source YUV sample mode
 * @param dst_sample Destination YUV sample mode
 */
static inline void cam_ll_set_convert_mode_yuv_to_yuv(lcd_cam_dev_t *dev, cam_yuv_sample_t src_sample, cam_yuv_sample_t dst_sample)
{
    HAL_ASSERT(src_sample != dst_sample);
    dev->cam_rgb_yuv.cam_conv_trans_mode = 1;
    switch (src_sample) {
    case CAM_YUV_SAMPLE_422:
        dev->cam_rgb_yuv.cam_conv_yuv_mode = 0;
        break;
    case CAM_YUV_SAMPLE_420:
        dev->cam_rgb_yuv.cam_conv_yuv_mode = 1;
        break;
    case CAM_YUV_SAMPLE_411:
        dev->cam_rgb_yuv.cam_conv_yuv_mode = 2;
        break;
    default:
        abort();
    }
    switch (dst_sample) {
    case CAM_YUV_SAMPLE_422:
        dev->cam_rgb_yuv.cam_conv_yuv2yuv_mode = 0;
        break;
    case CAM_YUV_SAMPLE_420:
        dev->cam_rgb_yuv.cam_conv_yuv2yuv_mode = 1;
        break;
    case CAM_YUV_SAMPLE_411:
        dev->cam_rgb_yuv.cam_conv_yuv2yuv_mode = 2;
        break;
    default:
        abort();
    }
}

/**
 * @brief Set camera received data byte length
 *
 * @param dev CAM register base address
 * @param length received data byte length, range [0, 0xFFFF]
 */
static inline void cam_ll_set_rec_data_bytelen(lcd_cam_dev_t *dev, uint32_t length)
{
    HAL_FORCE_MODIFY_U32_REG_FIELD(dev->cam_ctrl1, cam_rec_data_bytelen, length);
}

/**
 * @brief Set line number to trigger interrupt
 *
 * @param dev CAM register base address
 * @param number line number to trigger interrupt, range [0, 0x3F]
 */
static inline void cam_ll_set_line_int_num(lcd_cam_dev_t *dev, uint32_t number)
{
    dev->cam_ctrl1.cam_line_int_num = number;
}

/**
 * @brief Whether to invert the input signal CAM_PCLK
 *
 * @param dev CAM register base address
 * @param en True to invert, False to not invert
 */
static inline void cam_ll_enable_invert_clk(lcd_cam_dev_t *dev, bool en)
{
    dev->cam_ctrl1.cam_clk_inv = en;
}

/**
 * @brief Enable CAM_VSYNC filter function
 *
 * @param dev CAM register base address
 * @param en True to enable, False to bypass
 */
static inline void cam_ll_enable_vsync_filter(lcd_cam_dev_t *dev, bool en)
{
    dev->cam_ctrl1.cam_vsync_filter_en = en;
}

/**
 * @brief Set data read stride, i.e., number of bytes the CAM reads from the DMA in each step
 *
 * @param dev CAM register base address
 * @param stride data stride size
 */
static inline void cam_ll_set_dma_read_stride(lcd_cam_dev_t *dev, uint32_t stride)
{
    switch (stride) {
    case 8:
        dev->cam_ctrl1.cam_2byte_en = 0;
        break;
    case 16:
        dev->cam_ctrl1.cam_2byte_en = 1;
        break;
    default:
        abort();
        break;
    }
}

/**
 * @brief Whether to invert CAM_DE
 *
 * @param dev CAM register base address
 * @param en True to invert, False to not invert
 */
static inline void cam_ll_enable_invert_de(lcd_cam_dev_t *dev, bool en)
{
    dev->cam_ctrl1.cam_de_inv = en;
}

/**
 * @brief Whether to invert CAM_HSYNC
 *
 * @param dev CAM register base address
 * @param en True to invert, False to not invert
 */
static inline void cam_ll_enable_invert_hsync(lcd_cam_dev_t *dev, bool en)
{
    dev->cam_ctrl1.cam_hsync_inv = en;
}

/**
 * @brief Whether to invert CAM_VSYNC
 *
 * @param dev CAM register base address
 * @param en True to invert, False to not invert
 */
static inline void cam_ll_enable_invert_vsync(lcd_cam_dev_t *dev, bool en)
{
    dev->cam_ctrl1.cam_vsync_inv = en;
}

/**
 * @brief Set the mode to control the input control signals
 *
 * @param dev CAM register base address
 * @param mode 1: Input control signals are CAM_DE and CAM_HSYNC; 0: Input control signals are CAM_DE and CAM_VSYNC
 */
static inline void cam_ll_set_vh_de_mode(lcd_cam_dev_t *dev, uint32_t mode)
{
    dev->cam_ctrl1.cam_vh_de_mode_en = mode;
}

/**
 * @brief Set the wire width of CAM output
 *
 * @param dev CAM register base address
 * @param width CAM output wire width
 */
static inline void cam_ll_set_data_wire_width(lcd_cam_dev_t *dev, uint32_t width)
{
    // data line width is same as data stride that set in `cam_ll_set_dma_read_stride`
}

/**
 * @brief Start the CAM transaction
 *
 * @param dev CAM register base address
 */
__attribute__((always_inline))
static inline void cam_ll_start(lcd_cam_dev_t *dev)
{
    dev->cam_ctrl.cam_update = 1; // update parameters before start transaction
    dev->cam_ctrl1.cam_start = 1;
}

/**
 * @brief Stop the CAM transaction
 *
 * @param dev CAM register base address
 */
__attribute__((always_inline))
static inline void cam_ll_stop(lcd_cam_dev_t *dev)
{
    dev->cam_ctrl1.cam_start = 0;
    dev->cam_ctrl.cam_update = 1; // self clear
}

/**
 * @brief Reset CAM TX controller and RGB/YUV converter
 *
 * @param dev CAM register base address
 */
static inline void cam_ll_reset(lcd_cam_dev_t *dev)
{
    dev->cam_ctrl.cam_reset = 1; // self clear
}

/**
 * @brief Whether to reverse the data bit order
 *
 * @note It acts before the YUV-RGB converter
 *
 * @param dev CAM register base address
 * @param en True to reverse, False to not reverse
 */
__attribute__((always_inline))
static inline void cam_ll_reverse_dma_data_bit_order(lcd_cam_dev_t *dev, bool en)
{
    dev->cam_ctrl.cam_bit_order = en;
}

/**
 * @brief Whether to reverse the output data bit order
 *
 * @note ESP32S3 doesn't support to reverse the data bit after the YUV-RGB converter
 *
 * @param dev CAM register base address
 * @param en True to reverse, False to not reverse
 */
static inline void cam_ll_reverse_wire_bit_order(lcd_cam_dev_t *dev, bool en)
{
    (void)dev;
    (void)en;
}

/**
 * @brief Whether to swap adjacent two bytes
 *
 * @note This acts before the YUV-RGB converter, mainly to change the data endian.
 *       {B1,B0},{B3,B2} => {B0,B1}{B2,B3}
 *
 * @param dev CAM register base address
 * @param en True to swap the byte order, False to not swap
 */
__attribute__((always_inline))
static inline void cam_ll_swap_dma_data_byte_order(lcd_cam_dev_t *dev, bool en)
{
    dev->cam_ctrl.cam_byte_order = en;
}

/**
 * @brief Reset camera module
 *
 * @param dev CAM register base address
 */
__attribute__((always_inline))
static inline void cam_ll_reset(lcd_cam_dev_t *dev)
{
    dev->cam_ctrl1.cam_reset = 1; // self clear
}

/**
 * @brief Reset Async RX FIFO
 *
 * @param dev CAM register base address
 */
__attribute__((always_inline))
static inline void cam_ll_fifo_reset(lcd_cam_dev_t *dev)
{
    dev->cam_ctrl1.cam_afifo_reset = 1; // self clear
}

/**
 * @brief Enable/disable interrupt by mask
 *
 * @param dev CAM register base address
 * @param mask Interrupt mask
 * @param en True to enable interrupt, False to disable interrupt
 */
static inline void cam_ll_enable_interrupt(lcd_cam_dev_t *dev, uint32_t mask, bool en)
{
    if (en) {
        dev->lc_dma_int_ena.val |= mask & 0x0c;
    } else {
        dev->lc_dma_int_ena.val &= ~(mask & 0x0c);
    }
}

/**
 * @brief Get interrupt status value
 *
 * @param dev CAM register base address
 * @return Interrupt status value
 */
__attribute__((always_inline))
static inline uint32_t cam_ll_get_interrupt_status(lcd_cam_dev_t *dev)
{
    return dev->lc_dma_int_st.val & 0x0c;
}

/**
 * @brief Clear interrupt status by mask
 *
 * @param dev CAM register base address
 * @param mask Interupt status mask
 */
__attribute__((always_inline))
static inline void cam_ll_clear_interrupt_status(lcd_cam_dev_t *dev, uint32_t mask)
{
    dev->lc_dma_int_clr.val = mask & 0x0c;
}

/**
 * @brief Get address of interrupt status register address
 *
 * @param dev CAM register base address
 * @return Interrupt status register address
 */
static inline volatile void *cam_ll_get_interrupt_status_reg(lcd_cam_dev_t *dev)
{
    return &dev->lc_dma_int_st;
}

#ifdef __cplusplus
}
#endif