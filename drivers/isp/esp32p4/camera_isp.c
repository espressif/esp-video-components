/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <math.h>

#include "camera_isp.h"
#include "sdkconfig.h"
#include "soc/hp_sys_clkrst_struct.h"
#include "soc/isp_struct.h"
#include "esp_log.h"

/* ISP OUTPUT mode: 0: RAW8 1: YUV422 2: RGB888 3: YUV420 4: RGB565, see ISP.cntl.out_format */
#define MIPI_ISP_OUTPUT_RAW8_MODE (0)
#define MIPI_ISP_OUTPUT_YUV422_MODE (1)
#define MIPI_ISP_OUTPUT_RGB888_MODE (2)
#define MIPI_ISP_OUTPUT_YUV420_MODE (3)
#define MIPI_ISP_OUTPUT_RGB565_MODE (4)

/* ISP INPUT mode*/
#define MIPI_ISP_INPUT_RAW8_MODE (0)
#define MIPI_ISP_INPUT_RAW10_MODE (1)
#define MIPI_ISP_INPUT_RAW12_MODE (2)

static const char *TAG = "cam_isp";

static int get_isp_output_type(pixformat_t fmt)
{
    int ret = -1;
    switch (fmt) {
    case PIXFORMAT_RGB565:
        ret = MIPI_ISP_OUTPUT_RGB565_MODE;
        break;
    case PIXFORMAT_YUV422:
        ret = MIPI_ISP_OUTPUT_YUV422_MODE;
        break;
    case PIXFORMAT_YUV420:
        ret = MIPI_ISP_OUTPUT_YUV420_MODE;
        break;
    case PIXFORMAT_RGB888:
        ret = MIPI_ISP_OUTPUT_RGB888_MODE;
        break;
    case PIXFORMAT_RAW8:
        ret = MIPI_ISP_OUTPUT_RAW8_MODE;
        break;
    default:
        ESP_LOGE(TAG, "Not support output format");
        break;
    }
    return ret;
}

static int get_isp_input_type(pixformat_t fmt)
{
    int ret = -1;
    switch (fmt) {
    case PIXFORMAT_RAW8:
        ret = MIPI_ISP_INPUT_RAW8_MODE;
        break;
    case PIXFORMAT_RAW10:
        ret = MIPI_ISP_INPUT_RAW10_MODE;
        break;
    case PIXFORMAT_RAW12:
        ret = MIPI_ISP_INPUT_RAW12_MODE;
        break;
    default:
        ESP_LOGE(TAG, "Not support input format");
        break;
    }
    return ret;
}

// Can't turn ISP off even if you don't use it, just configure it to work in transparent mode.
int isp_init(uint32_t frame_width, uint32_t frame_height, pixformat_t in_format, pixformat_t out_format, bool isp_enable, void *sensor_info)
{
    int isp_out_format = get_isp_output_type(out_format);
    int isp_in_format = get_isp_input_type(in_format);

    HP_SYS_CLKRST.peri_clk_ctrl26.reg_isp_clk_div_num = (240000000 / 80000000) - 1;
    HP_SYS_CLKRST.peri_clk_ctrl25.reg_isp_clk_src_sel = 0x01;

    HP_SYS_CLKRST.peri_clk_ctrl25.reg_isp_clk_en = 0x0;
    HP_SYS_CLKRST.peri_clk_ctrl25.reg_isp_clk_en = 0x1;

    HP_SYS_CLKRST.hp_rst_en0.reg_rst_en_isp = 0x1;
    HP_SYS_CLKRST.hp_rst_en0.reg_rst_en_isp = 0x0;

    ISP.cntl.val = 0;
    ISP.cntl.ccm_en = 1;
    ISP.cntl.demosaic_en = 1;
    ISP.cntl.dpc_en = 1;
    ISP.dpc_ctrl.dpc_check_en = 0;
    ISP.dpc_ctrl.sta_en = 0;
    ISP.dpc_ctrl.dyn_en = 1;
    ISP.dpc_ctrl.dpc_black_en = 0;
    ISP.cntl.median_en = 1;
    ISP.cntl.gamma_en = 1;
    ISP.cntl.sharp_en = 1;
    ISP.cntl.isp_out_type = isp_out_format;
    ISP.yuv_format.yuv_mode = 1;
    ISP.yuv_format.yuv_range = 0;
    ISP.cntl.rgb2yuv_en = 1;
    ISP.cntl.yuv2rgb_en = esp_video_get_encoding_by_format(out_format) == color_encoding_RGB ? 1 : 0;
    ISP.cntl.color_en = 1;
    ISP.cntl.blc_en = 1;
    ISP.cntl.bf_en = 1;
    ISP.cntl.lsc_en = 0;
    ISP.cntl.isp_data_type = isp_in_format;
    ISP.cntl.isp_in_src = 0;
    ISP.cntl.mipi_data_en = 1;

    // This must be the same as the sensor configuration. See sensor_config.h.

#if CONFIG_MIPI_CSI_LINESYNC_SUPPORT
    ISP.frame_cfg.hsync_start_exist = 1;
    ISP.frame_cfg.hsync_end_exist = 1;
#else
    ISP.frame_cfg.hsync_start_exist = 0;
    ISP.frame_cfg.hsync_end_exist = 0;
#endif

    ISP.frame_cfg.bayer_mode = CONFIG_CAM_SENSOR_COLOR_BAYER_MODE;
    ISP.int_ena.val = 0;
    ISP.int_clr.val = ~0;

    /*Note: Before isp_en, need to write 1 to reg_gamma_update first,
    and then wait for the register hardware to be set to 0 for the update to complete.*/
    ISP.gamma_ctrl.gamma_update = 1;
    while (ISP.gamma_ctrl.gamma_update);

    if ((isp_in_format != -1) && isp_enable) {
        ISP.frame_cfg.hadr_num = frame_width - 1;
        ISP.frame_cfg.vadr_num = frame_height - 1;
        ISP.cntl.isp_en = 1;
    } else { // not use ISP module, use bridge fifo, 32bytes align.
        ISP.frame_cfg.hadr_num = ceil((float)(frame_width * esp_video_get_bpp_by_format(in_format)) / 32.0) - 1;
        ISP.frame_cfg.vadr_num = frame_height - 1;
        ISP.cntl.isp_en = 0;
    }

    ESP_LOGD(TAG, "h_num=%u, v_num=%u, ISP_cntl.en: %u", ISP.frame_cfg.hadr_num, ISP.frame_cfg.vadr_num, ISP.cntl.isp_en);
    return 0;
}
