/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "hal/cam_hal.h"
#include "esp_attr.h"
#include "hal/log.h"
#include "hal/assert.h"
#include "soc/soc_caps.h"
#include "esp_private/esp_clk.h"

#define CAM_HAL_CLOCK_GROUP_DEFAULT (0)
#define CAM_HAL_XCLK_CLOCK_DEFAULT  (20 * 1000 * 1000)

static void cam_hal_set_line_int_num(cam_hal_context_t *hal, uint32_t num)
{
    if (num > 0) {
        cam_ll_enable_line_int(hal->hw, 1);
        cam_ll_set_line_int_num(hal->hw, num);
    } else {
        cam_ll_enable_line_int(hal->hw, 0);
        cam_ll_set_line_int_num(hal->hw, 0);
    }
}

static void cam_hal_set_vsync_filter_num(cam_hal_context_t *hal, uint32_t num)
{
    if (num > 0) {
        cam_ll_enable_vsync_filter(hal->hw, 1);
        cam_ll_set_vsync_filter_thres(hal->hw, num);
    } else {
        cam_ll_enable_vsync_filter(hal->hw, 0);
        cam_ll_set_vsync_filter_thres(hal->hw, 0);
    }
}

/**
 * @brief Configure the CAM module to generate the specified XCLK clock
 *
 * @param hal    CAM object data pointer
 * @param xclk_freq   XCLK clock frequency
 *
 * @return 0 if the configuration was successful, non-zero if not.
 */
int cam_hal_config_xclk(cam_hal_context_t *hal, uint32_t xclk_freq)
{
    uint32_t src_clk_hz = 160000000;
    int xclk_div_num = 0;
    cam_clock_source_t clk_src = CAM_CLK_SRC_DEFAULT;
#ifdef CONFIG_CAM_DVP_CLK_SRC_PLL160M
    clk_src = CAM_CLK_SRC_PLL160M;
    src_clk_hz = 160000000;
#elif defined CONFIG_CAM_DVP_CLK_SRC_PLL240M
    clk_src = CAM_CLK_SRC_PLL240M;
    src_clk_hz = 240000000;
#elif defined CONFIG_CAM_DVP_CLK_SRC_XTAL
    clk_src = CAM_CLK_SRC_XTAL;
    src_clk_hz = esp_clk_xtal_freq();
#else
    HAL_LOGE("cam_hal", "CLK mismatch");
    return -1;
#endif

    if (src_clk_hz && !(src_clk_hz % xclk_freq)) {
        xclk_div_num = src_clk_hz / xclk_freq;
        cam_ll_set_group_clock_coeff(hal->hw, xclk_div_num, 0, 0);
        // Note1: clock sources like PLL and XTAL will be turned off in light sleep
        // Note2: xclk will be generated after select src clock
        cam_ll_select_clk_src(hal->hw, clk_src);
        return 0;
    }
    HAL_LOGE("cam_hal", "CLK config err");
    return -1;
}

/**
 * @brief Configure the CAM module output conversion function
 *
 * @note When working in YUV422 To YUV420 mode, ensure that the sensor outputs data in YUV422 format. The same is true for other modes.
 * The sensor must output data in a specified format to ensure that the converted data is normal.
 *
 * @param hal    CAM object data pointer
 * @param mode   Conversion mode
 *
 * @return 0 if the configuration was successful, non-zero if not.
 */
int cam_hal_set_conv_mode(cam_hal_context_t *hal, cam_conv_mode_t mode)
{
    uint32_t vh_de_mode = 0;
    cam_ll_get_vh_de_mode(hal->hw, &vh_de_mode);
    if (!vh_de_mode && (mode == YUV422_TO_RGB565)) {
        HAL_LOGE("cam_hal", "YUV-RGB not work on V_DE mode");
        return -1;
    }

    cam_ll_enable_rgb_yuv_convert(hal->hw, 1);
    // There is no sensor output YUV420\YUV411, And sensors that can output RGB can also output YUV, so input is YUV422 default
    switch (mode) {
    case YUV422_TO_YUV420: // YUV422->YUV420, YUV420 is a typical input format for encoder
        cam_ll_set_convert_mode_yuv_to_yuv(hal->hw, CAM_YUV_SAMPLE_422, CAM_YUV_SAMPLE_420);
        break;
    case YUV422_TO_RGB565: // YUV422->RGB565, RGB565 is a typical input format for LCD
        cam_ll_set_convert_mode_yuv_to_rgb(hal->hw, CAM_YUV_SAMPLE_422);
        break;
    default:
        HAL_LOGE("cam_hal", "conv mode mismatch");
        abort();
        break;
    }
    return 0;
}

/**
 * @brief Initialize CAM hardware
 *
 * @param hal    CAM object data pointer
 * @param port   CAM port
 *
 * @return None
 */
void cam_hal_init(cam_hal_context_t *hal, uint8_t port)
{
    memset(hal, 0, sizeof(cam_hal_context_t));

    hal->hw = CAM_LL_GET_HW(0);

    cam_ll_enable_stop_signal(hal->hw, 0);
    cam_ll_swap_dma_data_byte_order(hal->hw, 0);
    cam_ll_reverse_dma_data_bit_order(hal->hw, 0);
    cam_ll_enable_vsync_generate_eof(hal->hw, 0); // use LCD_CAM_CAM_REC_DATA_BYTELEN to control in_suc_eof

    cam_hal_set_line_int_num(hal, 0);
    cam_hal_set_vsync_filter_num(hal, 4); // Filter by LCD_CAM clock

    cam_ll_enable_invert_pclk(hal->hw, 0);
    cam_ll_set_input_data_width(hal->hw, 8);
    cam_ll_enable_invert_de(hal->hw, 0);
    cam_ll_enable_invert_vsync(hal->hw, 0);
    cam_ll_enable_invert_hsync(hal->hw, 0);
    cam_ll_set_vh_de_mode(hal->hw, 0); // Disable vh_de mode default
    cam_ll_enable_rgb_yuv_convert(hal->hw, 0); // bypass conv module default
}

/**
 * @brief De-initialize CAM hardware
 *
 * @note Stop stream before deinit
 * @param hal CAM object data pointer
 *
 * @return None
 */
void cam_hal_deinit(cam_hal_context_t *hal)
{
    // To disable xclk output
    cam_ll_select_clk_src(hal->hw, (cam_clock_source_t) -1);
    cam_ll_stop(hal->hw);
    cam_ll_reset(hal->hw);
}

/**
 * @brief Start CAM to receive frame data, and active driver to send "CAM_EVENT_DATA_RECVED" event
 *
 * @param hal    CAM object data pointer
 * @param lldesc CAM DMA description pointer
 * @param size   CAM DMA receive size to trigger interrupt
 *
 * @return None
 */
void cam_hal_start_streaming(cam_hal_context_t *hal, lldesc_t *lldesc, size_t size)
{
    cam_ll_stop(hal->hw);

    cam_ll_reset(hal->hw);
    cam_ll_fifo_reset(hal->hw);

    cam_ll_set_rec_data_bytelen(hal->hw, size - 1);

    cam_ll_start(hal->hw);
}

/**
 * @brief Disable CAM receiving frame data, and deactive driver sending "CAM_EVENT_DATA_RECVED" event
 *
 * @param hal CAM object data pointer
 *
 * @return None
 */
void cam_hal_stop_streaming(cam_hal_context_t *hal)
{
    cam_ll_stop(hal->hw);
}

/**
 * @brief CAM copy data from source memory to destination memory, this can't be called in interrupt.
 *
 * @param hal  CAM object data pointer
 * @param dst  Destination memory pointer
 * @param src  Source memory pointer
 * @param size Data size in byte
 *
 * @return CAM sample data size
 */
void cam_hal_memcpy(cam_hal_context_t *hal, uint8_t *dst, const uint8_t *src, size_t size)
{
    memcpy(dst, src, size);
}

/**
 * @brief Get CAM sample data size, ESP32 has special sample data size with different receive data format
 *
 * @param hal CAM object data pointer
 *
 * @return CAM sample data size
 */
size_t cam_hal_get_sample_data_size(cam_hal_context_t *hal)
{
    return 1;
}

/**
 * @brief Get CAM interrupt status
 *
 * @param hal CAM object data pointer
 *
 * @return CAM interrupt status
 */
uint32_t IRAM_ATTR cam_hal_get_int_status(cam_hal_context_t *hal)
{
    return cam_ll_get_interrupt_status(hal->hw);
}

/**
 * @brief Clear CAM interrupt status
 *
 * @param hal CAM object data pointer
 *
 * @return None
 */
void IRAM_ATTR cam_hal_clear_int_status(cam_hal_context_t *hal, uint32_t status)
{
    cam_ll_clear_interrupt_status(hal->hw, status);
}

/**
 * @brief Get DMA buffer align size
 *
 * @param hal CAM object data pointer
 *
 * @return DMA buffer align size
 */
uint32_t cam_hal_dma_align_size(cam_hal_context_t *hal)
{
    return 4;
}
