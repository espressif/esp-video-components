/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include "esp_err.h"
#include "esp_intr_alloc.h"
#if CONFIG_SOC_I2S_SUPPORTS_LCD_CAMERA
#include "hal/i2s_ll.h"
#elif CONFIG_SOC_LCDCAM_SUPPORTED
#include "hal/cam_ll.h"
#endif

#if CONFIG_SOC_I2S_SUPPORTS_LCD_CAMERA
typedef i2s_dev_t cam_dev_t;
#define CAM_RX_INT_MASK I2S_LL_RX_EVENT_MASK
#elif CONFIG_SOC_LCDCAM_SUPPORTED
typedef lcd_cam_dev_t cam_dev_t;
#define CAM_RX_INT_MASK BIT(2)
#else
typedef void *cam_dev_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_SOC_LCDCAM_SUPPORTED
typedef enum {
    YUV422_TO_YUV420 = 1,
    YUV422_TO_RGB565,
} cam_conv_mode_t;

typedef struct cam_conv_config_t {
    cam_conv_mode_t mode;
    cam_color_range_t range;
    cam_yuv_conv_std_t std;
} cam_conv_config_t;
#endif

/**
 * @brief CAM configuration structure
 */
typedef struct cam_hal_cfg {
    uint8_t cam_port;                        /*!< CAM port. */
#if CONFIG_SOC_LCDCAM_SUPPORTED
    uint32_t xclk_freq;                      /*!< CAM output xclk freq. */
#endif
} cam_hal_config_t;

/**
 * @brief CAM hardware interface object data
 */
typedef struct cam_hal_context {
    cam_dev_t *hw;                          /*!< Beginning address of the CAM peripheral registers. */
} cam_hal_context_t;

/**
 * @brief Initialize CAM hardware
 *
 * @param hal    CAM object data pointer
 * @param port   CAM port
 *
 * @return None
 */
void cam_hal_init(cam_hal_context_t *hal, uint8_t port);

/**
 * @brief De-initialize CAM hardware
 *
 * @param hal CAM object data pointer
 *
 * @return None
 */
void cam_hal_deinit(cam_hal_context_t *hal);

/**
 * @brief Start CAM to receive frame data, and active driver to send "CAM_EVENT_DATA_RECVED" event
 *
 * @param hal CAM object data pointer
 * @param dma_desc CAM DMA description pointer address
 * @param size   CAM DMA receive size to trigger interrupt
 *
 * @return None
 */
void cam_hal_start_streaming(cam_hal_context_t *hal, uint32_t dma_desc, size_t size);

/**
 * @brief Disable CAM receiving frame data, and deactive driver sending "CAM_EVENT_DATA_RECVED" event
 *
 * @param hal CAM object data pointer
 *
 * @return None
 */
void cam_hal_stop_streaming(cam_hal_context_t *hal);

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
void cam_hal_memcpy(cam_hal_context_t *hal, uint8_t *dst, const uint8_t *src, size_t size);

/**
 * @brief Get CAM sample data size, ESP32 has special sample data size with different receive data format
 *
 * @param hal CAM object data pointer
 *
 * @return CAM sample data size
 */
size_t cam_hal_get_sample_data_size(cam_hal_context_t *hal);

/**
 * @brief Get CAM interrupt status
 *
 * @param hal CAM object data pointer
 *
 * @return CAM interrupt status
 */
uint32_t cam_hal_get_int_status(cam_hal_context_t *hal);

/**
 * @brief Clear CAM interrupt status
 *
 * @param hal CAM object data pointer
 *
 * @return None
 */
void cam_hal_clear_int_status(cam_hal_context_t *hal, uint32_t status);

/**
 * @brief Get DMA buffer align size
 *
 * @param hal CAM object data pointer
 *
 * @return DMA buffer align size
 */
uint32_t cam_hal_dma_align_size(cam_hal_context_t *hal);

#if CONFIG_SOC_LCDCAM_SUPPORTED
/**
 * @brief Configure the CAM module to generate the specified XCLK clock
 *
 * @param hal    CAM object data pointer
 * @param xclk_freq   XCLK clock frequency
 *
 * @return 0 if the configuration was successful, non-zero if not.
 */
int cam_hal_config_xclk(cam_hal_context_t *hal, uint32_t xclk_freq);

/**
 * @brief Configure the CAM port module to generate the specified XCLK clock
 *
 * @param port        CAM port
 * @param xclk_freq   XCLK clock frequency
 *
 * @return 0 if the configuration was successful, non-zero if not.
 */
int cam_hal_config_port_xclk(int port, uint32_t xclk_freq);

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
int cam_hal_set_conv_mode(cam_hal_context_t *hal, cam_conv_mode_t mode);
#endif // CONFIG_SOC_LCDCAM_SUPPORTED

#ifdef __cplusplus
}
#endif
