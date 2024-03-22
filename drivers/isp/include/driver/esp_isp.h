/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_color_formats.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SOC_ISP_DVP_DATA_SIG_NUM 16

/**
 * @brief ISP Input Source
 */
typedef enum {
    ISP_INPUT_DATA_SOURCE_CSI,       ///< Input data from CSI
    ISP_INPUT_DATA_SOURCE_DVP,       ///< Input data from DVP
    ISP_INPUT_DATA_SOURCE_DWGDMA,    ///< Input data from DW-GDMA
} isp_input_data_source_t;

/**
 * @brief ISP Color Type
 */
typedef enum {
    ISP_COLOR_RAW8   = PIXFORMAT_RAW8,   ///< RAW8
    ISP_COLOR_RAW10  = PIXFORMAT_RAW10,  ///< RAW10
    ISP_COLOR_RAW12  = PIXFORMAT_RAW12,  ///< RAW12
    ISP_COLOR_RGB888 = PIXFORMAT_RGB888, ///< RGB888
    ISP_COLOR_RGB565 = PIXFORMAT_RGB565, ///< RGB565
    ISP_COLOR_YUV422 = PIXFORMAT_YUV422, ///< YUV422
    ISP_COLOR_YUV420 = PIXFORMAT_YUV420, ///< YUV420
} isp_color_t;

typedef void *esp_isp_processor_t;
typedef int isp_clk_src_t;

/**
 * @brief ISP configurations
 */
typedef struct {
    isp_clk_src_t clk_src;                        ///< Clock source
    uint32_t clk_hz;                              ///< Clock frequency in Hz
    isp_input_data_source_t input_data_source;    ///< Input data source
    isp_color_t input_data_color_type;            ///< Input color type
    isp_color_t output_data_color_type;           ///< Output color type
    bool has_line_start_packet;                   ///< Enable line start packet
    bool has_line_end_packet;                     ///< Enable line end packet
    uint32_t h_res;                               ///< Input horizontal resolution, i.e. the number of pixels in a line
    uint32_t v_res;                               ///< Input vertical resolution, i.e. the number of lines in a frame
    size_t data_width;                            ///< Number of data lines
    int data_io[SOC_ISP_DVP_DATA_SIG_NUM];        ///< ISP DVP data-in IO numbers
    int pclk_io;                                  ///< ISP DVP pclk IO numbers
    int hsync_io;                                 ///< ISP DVP hsync IO numbers
    int vsync_io;                                 ///< ISP DVP vsync IO numbers
    int de_io;                                    ///< ISP DVP href IO numbers
    struct {
        uint32_t pclk_invert: 1;                  ///< The pclk is inverted
        uint32_t hsync_invert: 1;                 ///< The hsync signal is inverted
        uint32_t vsync_invert: 1;                 ///< The vsync signal is inverted
        uint32_t de_invert: 1;                    ///< The de signal is inverted
    } io_flags;                                   ///< ISP DVP IO flags
} esp_isp_processor_cfg_t;

static inline esp_err_t esp_isp_del_processor(esp_isp_processor_t *proc)
{
    return ESP_OK;
}

static inline esp_err_t esp_isp_enable(esp_isp_processor_t *proc)
{
    return ESP_OK;
}

static inline esp_err_t esp_isp_disable(esp_isp_processor_t *proc)
{
    return ESP_OK;
}

/**
 * @brief New an ISP processor
 *
 * @param[in]  proc_config  Pointer to ISP config. Refer to ``esp_isp_processor_cfg_t``.
 * @param[out] ret_proc     Processor handle
 *
 * @return
 *         - ESP_OK                On success
 *         - ESP_ERR_INVALID_ARG   If the combination of arguments is invalid.
 *         - ESP_ERR_NOT_FOUND     No free interrupt found with the specified flags
 *         - ESP_ERR_NOT_SUPPORTED Not supported mode
 *         - ESP_ERR_NO_MEM        If out of memory
 */
esp_err_t esp_isp_new_processor(const esp_isp_processor_cfg_t *proc_config, esp_isp_processor_t *ret_proc);

#ifdef __cplusplus
}
#endif
