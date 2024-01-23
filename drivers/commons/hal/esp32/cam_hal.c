/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hal/cam_hal.h"
#include "esp_attr.h"

#define i2s_ll_rx_disable_mono(i2s)             { (i2s)->conf.rx_mono = 0; }
#define i2s_ll_timing_clear(i2s)                { (i2s)->timing.val = 0; }
#define i2s_ll_timing_enable_rx_dsync(i2s)      { (i2s)->timing.rx_dsync_sw = 1; }
#define i2s_ll_fifo_set_mode(i2s, v)            { (i2s)->fifo_conf.rx_fifo_mod = (v); }
#define i2s_ll_intr_get_val(i2s)                ( (i2s)->int_st.val )
#define i2s_ll_ahbm_reset(i2s)                  \
{                                               \
    (i2s)->lc_conf.ahbm_fifo_rst = 1;           \
    (i2s)->lc_conf.ahbm_fifo_rst = 0;           \
    (i2s)->lc_conf.ahbm_rst = 1;                \
    (i2s)->lc_conf.ahbm_rst = 0;                \
}
#define i2s_ll_sample_set_mode(i2s, v)          \
{                                               \
    (i2s)->fifo_conf.rx_fifo_mod = v;           \
    (i2s)->sample_rate_conf.rx_bits_mod = 0;    \
}

#define i2s_ll_get_rx_dma_next_desc_addr(i2s)   ((i2s)->in_link_dscr_bf0)

/**
 * Todo: AEG-1175
 */
#define ESP32_I2S_CAM_SAMPLE_MODE 3

typedef union {
    struct {
        uint32_t sample2: 8;
        uint32_t unused2: 8;
        uint32_t sample1: 8;
        uint32_t unused1: 8;
    };
    uint32_t val;
} dma_elem_t;

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
    hal->hw = port == 0 ? &I2S0 : &I2S1;

    i2s_ll_rx_reset(hal->hw);
    i2s_ll_rx_reset_fifo(hal->hw);
    i2s_ll_ahbm_reset(hal->hw);

    i2s_ll_rx_set_slave_mod(hal->hw, true);
    i2s_ll_rx_enable_right_first(hal->hw, false);
    i2s_ll_rx_enable_msb_right(hal->hw, false);
    i2s_ll_rx_enable_msb_shift(hal->hw, false);
    i2s_ll_rx_disable_mono(hal->hw);
    i2s_ll_rx_set_ws_width(hal->hw, 8);

    i2s_ll_enable_lcd(hal->hw, true);
    i2s_ll_enable_camera(hal->hw, true);

    i2s_ll_set_raw_mclk_div(hal->hw, 2, 0, 0);
    i2s_ll_enable_dma(hal->hw, true);
    i2s_ll_rx_force_enable_fifo_mod(hal->hw, true);
    i2s_ll_rx_select_pdm_slot(hal->hw, I2S_PDM_SLOT_RIGHT);

    i2s_ll_sample_set_mode(hal->hw, ESP32_I2S_CAM_SAMPLE_MODE);

    i2s_ll_timing_clear(hal->hw);
    i2s_ll_timing_enable_rx_dsync(hal->hw);
}

/**
 * @brief De-initialize CAM hardware
 *
 * @param hal CAM object data pointer
 *
 * @return None
 */
void cam_hal_deinit(cam_hal_context_t *hal)
{
}

/**
 * @brief Start CAM to receive frame data, and active driver to send "CAM_EVENT_DATA_RECVED" event
 *
 * @param hal    CAM object data pointer
 * @param dma_desc CAM DMA description pointer address
 * @param size   CAM DMA receive size to trigger interrupt
 *
 * @return None
 */
void cam_hal_start_streaming(cam_hal_context_t *hal, uint32_t dma_desc, size_t size)
{
    uint32_t link_addr = dma_desc & 0xfffff;

    i2s_ll_rx_reset(hal->hw);
    i2s_ll_rx_reset_fifo(hal->hw);

    i2s_ll_ahbm_reset(hal->hw);

    i2s_ll_clear_intr_status(hal->hw, I2S_LL_RX_EVENT_MASK);
    i2s_ll_rx_enable_intr(hal->hw);

    i2s_ll_rx_set_eof_num(hal->hw, size);
    i2s_ll_rx_start_link(hal->hw, link_addr);

    i2s_ll_rx_start(hal->hw);
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
    i2s_ll_rx_stop(hal->hw);

    i2s_ll_rx_disable_intr(hal->hw);
    i2s_ll_clear_intr_status(hal->hw, I2S_LL_RX_EVENT_MASK);

    i2s_ll_tx_stop_link(hal->hw);
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
#if ESP32_I2S_CAM_SAMPLE_MODE == 0
    const dma_elem_t *dma_el = (const dma_elem_t *)src;
    size_t elements = size / sizeof(dma_elem_t);
    size_t end = elements / 4;

    for (size_t i = 0; i < end; ++i) {
        dst[0] = dma_el[0].sample1;//y0
        dst[1] = dma_el[0].sample2;//u
        dst[2] = dma_el[1].sample1;//y1
        dst[3] = dma_el[1].sample2;//v

        dst[4] = dma_el[2].sample1;//y0
        dst[5] = dma_el[2].sample2;//u
        dst[6] = dma_el[3].sample1;//y1
        dst[7] = dma_el[3].sample2;//v
        dma_el += 4;
        dst += 8;
    }
#elif ESP32_I2S_CAM_SAMPLE_MODE == 1
    const dma_elem_t *dma_el = (const dma_elem_t *)src;
    size_t elements = size / sizeof(dma_elem_t);
    size_t end = elements / 4;

    for (size_t i = 0; i < end; ++i) {
        dst[0] = dma_el[0].sample1;//y0
        dst[1] = dma_el[0].sample2;//u
        dst[2] = dma_el[1].sample1;//y1
        dst[3] = dma_el[1].sample2;//v

        dst[4] = dma_el[2].sample1;//y0
        dst[5] = dma_el[2].sample2;//u
        dst[6] = dma_el[3].sample1;//y1
        dst[7] = dma_el[3].sample2;//v
        dma_el += 4;
        dst += 8;
    }
#elif ESP32_I2S_CAM_SAMPLE_MODE == 3
    const dma_elem_t *dma_el = (const dma_elem_t *)src;
    size_t elements = size / sizeof(dma_elem_t);
    size_t end = elements / 8;

    for (size_t i = 0; i < end; ++i) {
        dst[0] = dma_el[0].sample1;//y0
        dst[1] = dma_el[1].sample1;//u
        dst[2] = dma_el[2].sample1;//y1
        dst[3] = dma_el[3].sample1;//v

        dst[4] = dma_el[4].sample1;//y0
        dst[5] = dma_el[5].sample1;//u
        dst[6] = dma_el[6].sample1;//y1
        dst[7] = dma_el[7].sample1;//v
        dma_el += 8;
        dst += 8;
    }

    if ((elements & 0x7) != 0) {
        dst[0] = dma_el[0].sample1;//y0
        dst[1] = dma_el[1].sample1;//u
        dst[2] = dma_el[2].sample1;//y1
        dst[3] = dma_el[2].sample2;//v
        elements += 4;
    }
#endif
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
#if ESP32_I2S_CAM_SAMPLE_MODE == 1
    return 2;
#elif ESP32_I2S_CAM_SAMPLE_MODE == 0 || ESP32_I2S_CAM_SAMPLE_MODE == 3
    return 4;
#else
    return 0;
#endif
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
    return i2s_ll_intr_get_val(hal->hw);
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
    i2s_ll_clear_intr_status(hal->hw, status);
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

/**
 * @brief Get next DMA description address
 *
 * @param hal CAM object data pointer
 *
 * @return Next DMA description address
 */
uint32_t cam_hal_get_next_dma_desc_addr(cam_hal_context_t *hal)
{
    return i2s_ll_get_rx_dma_next_desc_addr(hal->hw);
}
