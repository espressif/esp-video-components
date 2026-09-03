/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * Toshiba TC358743 HDMI → MIPI CSI-2 bridge
 * References (c = chapter, p = page):
 * REF_01 - Toshiba, TC358743XBG (H2C), Functional Specification, Rev 1.1
 * REF_02 - Register sequence from drivers/media/i2c/tc358743.c in the Linux kernel.
 * REF_03 - https://github.com/jrowny/p4kvm
 *
 */
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"
#include "tc358743_settings.h"
#include "tc358743.h"
#include "tc358743_edid_27Minput_1080p_30fps.h"

typedef struct {
    bool csi_uyvy422;
    uint32_t refclk_hz;
    uint16_t pll_prd;
    uint16_t pll_fbd;
    uint16_t fifo_level;
    uint32_t lineinitcnt;
    uint32_t lptxtimecnt;
    uint32_t tclk_headercnt;
    uint32_t tclk_trailcnt;
    uint32_t ths_headercnt;
    uint32_t twakeup;
    uint32_t tclk_postcnt;
    uint32_t ths_trailcnt;
    uint32_t hstxvregcnt;
    uint8_t ddc5v_mode;
    unsigned lanes;
    bool enable_hdcp;
    uint8_t hdmi_detection_delay;
    bool hdmi_phy_auto_reset_tmds_detected;
    bool hdmi_phy_auto_reset_tmds_in_range;
    bool hdmi_phy_auto_reset_tmds_valid;
    bool hdmi_phy_auto_reset_hsync_out_of_range;
    bool hdmi_phy_auto_reset_vsync_out_of_range;
} tc358743_para_t;

struct tc358743_cam {
    tc358743_para_t tc358743_para;
};

#define TC358743_IO_MUX_LOCK(mux)
#define TC358743_IO_MUX_UNLOCK(mux)
#define TC358743_ENABLE_OUT_XCLK(pin,clk)
#define TC358743_DISABLE_OUT_XCLK(pin)
#define TC358743_ADV_DEBUG_EN (0)
#define TC358743_REFCLK_27M_HZ 27000000u
#define TC358743_1920X1080_RGB888_30FPS_MIPI_LANE_MBPS 972
#define TC358743_1920X1080_YUV422_30FPS_MIPI_LANE_MBPS 648
#define TC358743_HDMI_LOCK_WAIT_MS (6000)

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))

static const char *TAG = "tc358743";

#ifndef CONFIG_CAMERA_TC358743_MIPI_IF_FORMAT_INDEX_DEFAULT
#error "Please choose at least one format in menuconfig for TC358743"
#endif

static const uint8_t tc358743_format_default_index = CONFIG_CAMERA_TC358743_MIPI_IF_FORMAT_INDEX_DEFAULT;

static const uint8_t tc358743_format_index[] = {
#if CONFIG_CAMERA_TC358743_MIPI_RGB888_1920X1080_30FPS
    0,
#endif
#if CONFIG_CAMERA_TC358743_MIPI_YUV422_1920X1080_30FPS
    1,
#endif
};

static const esp_cam_sensor_format_t tc358743_format_info[] = {
#if CONFIG_CAMERA_TC358743_MIPI_RGB888_1920X1080_30FPS
    {
        .name = "MIPI_2lane_27Minput_RGB888_1920x1080_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RGB888,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 27000000,
        .width = 1920,
        .height = 1080,
        .regs = tc358743_mipi_2lane_27Minput_1920x1080_rgb888_30fps,
        .regs_size = ARRAY_SIZE(tc358743_mipi_2lane_27Minput_1920x1080_rgb888_30fps),
        .fps = 30,
        .isp_info = NULL,
        .mipi_info = {
            .mipi_clk = TC358743_1920X1080_RGB888_30FPS_MIPI_LANE_MBPS * (1000 * 1000),
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
#endif
#if CONFIG_CAMERA_TC358743_MIPI_YUV422_1920X1080_30FPS
    {
        .name = "MIPI_2lane_27Minput_YUV422_1920x1080_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY, // todo, fix yuv seq with csi_config.input_8bit_swap_en = true.
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 27000000,
        .width = 1920,
        .height = 1080,
        .regs = tc358743_mipi_2lane_27Minput_1920x1080_yuv422_30fps,
        .regs_size = ARRAY_SIZE(tc358743_mipi_2lane_27Minput_1920x1080_yuv422_30fps),
        .fps = 30,
        .isp_info = NULL,
        .mipi_info = {
            .mipi_clk = TC358743_1920X1080_YUV422_30FPS_MIPI_LANE_MBPS * (1000 * 1000),
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
#endif
};

static uint8_t get_tc358743_actual_format_index(void)
{
    for (int i = 0; i < ARRAY_SIZE(tc358743_format_index); i++) {
        if (tc358743_format_index[i] == tc358743_format_default_index) {
            return i;
        }
    }

    return 0;
}

static esp_err_t tc358743_read(esp_sccb_io_handle_t sccb_handle, uint16_t reg_addr, void *reg_val, size_t reg_val_size)
{
    return esp_sccb_transmit_receive_reg_a16(sccb_handle, reg_addr, reg_val, reg_val_size);
}

static esp_err_t tc358743_write(esp_sccb_io_handle_t sccb_handle, uint16_t reg_addr, const void *reg_val, size_t reg_val_size)
{
    return esp_sccb_transmit_reg_a16(sccb_handle, reg_addr, reg_val, reg_val_size);
}

static esp_err_t tc358743_wr8(esp_sccb_io_handle_t sccb_handle, uint16_t reg_addr, uint8_t reg_val)
{
    return tc358743_write(sccb_handle, reg_addr, &reg_val, 1);
}

static esp_err_t tc358743_wr16(esp_sccb_io_handle_t sccb_handle, uint16_t reg_addr, uint16_t reg_val)
{
    uint8_t data[2] = {(uint8_t)(reg_val & 0xff), (uint8_t)(reg_val >> 8)};
    return tc358743_write(sccb_handle, reg_addr, data, 2);
}

static esp_err_t tc358743_wr32(esp_sccb_io_handle_t sccb_handle, uint16_t reg_addr, uint32_t reg_val)
{
    uint8_t data[4] = {(uint8_t)(reg_val & 0xff), (uint8_t)((reg_val >> 8) & 0xff), (uint8_t)((reg_val >> 16) & 0xff),
                       (uint8_t)((reg_val >> 24) & 0xff)
                      };
    return tc358743_write(sccb_handle, reg_addr, data, 4);
}

static esp_err_t tc358743_rd8(esp_sccb_io_handle_t sccb_handle, uint16_t reg_addr, uint8_t *reg_val)
{
    return tc358743_read(sccb_handle, reg_addr, reg_val, 1);
}

static esp_err_t tc358743_rd16(esp_sccb_io_handle_t sccb_handle, uint16_t reg_addr, uint16_t *reg_val)
{
    esp_err_t ret = ESP_OK;
    uint8_t data[2] = {0};
    ret = tc358743_read(sccb_handle, reg_addr, data, 2);
    if (ret == ESP_OK) {
        *reg_val = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    }
    return ret;
}

#if TC358743_ADV_DEBUG_EN
static esp_err_t tc358743_rd32(esp_sccb_io_handle_t sccb_handle, uint16_t reg_addr, uint32_t *reg_val)
{
    esp_err_t ret = ESP_OK;
    uint8_t data[4] = {0};
    ret = tc358743_read(sccb_handle, reg_addr, data, 4);
    if (ret == ESP_OK) {
        *reg_val = (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
    }
    return ret;
}
#endif

static esp_err_t tc358743_wr16_and_or(esp_sccb_io_handle_t sccb_handle, uint16_t reg_addr, uint16_t mask, uint16_t val)
{
    esp_err_t ret = ESP_OK;
    uint16_t current_val = 0;
    ret = tc358743_rd16(sccb_handle, reg_addr, &current_val);
    if (ret == ESP_OK) {
        current_val = (current_val & mask) | val;
        ret = tc358743_wr16(sccb_handle, reg_addr, current_val);
    }
    return ret;
}

static esp_err_t tc358743_wr8_and_or(esp_sccb_io_handle_t sccb_handle, uint16_t reg_addr, uint8_t mask, uint8_t val)
{
    esp_err_t ret = ESP_OK;
    uint8_t current_val = 0;
    ret = tc358743_rd8(sccb_handle, reg_addr, &current_val);
    if (ret == ESP_OK) {
        current_val = (current_val & mask) | val;
        ret = tc358743_wr8(sccb_handle, reg_addr, current_val);
    }
    return ret;
}

static esp_err_t tc358743_reset_blocks(esp_sccb_io_handle_t sccb_handle, uint16_t mask)
{
    esp_err_t ret = ESP_OK;
    uint16_t current_val = 0;
    ret = tc358743_rd16(sccb_handle, SYSCTL, &current_val);
    if (ret == ESP_OK) {
        ret = tc358743_wr16(sccb_handle, SYSCTL, current_val | mask);
        ret |= tc358743_wr16(sccb_handle, SYSCTL, current_val & ~mask);
    }
    return ret;
}

static esp_err_t tc358743_sleep_mode(esp_sccb_io_handle_t sccb_handle, bool enable)
{
    return tc358743_wr16_and_or(sccb_handle, SYSCTL, ~MASK_SLEEP, enable ? MASK_SLEEP : 0);
}

static esp_err_t tc358743_enable_stream(esp_sccb_io_handle_t sccb_handle, bool enable)
{
    esp_err_t ret = ESP_OK;
    if (enable) {
        /* Non-continuous MIPI clock: leave TXOPTIONCNTRL as set_csi() left it (0); matches Linux. */
        ret = tc358743_wr8(sccb_handle, VI_MUTE, MASK_AUTO_MUTE);
    } else {
        ret = tc358743_wr8(sccb_handle, VI_MUTE, MASK_AUTO_MUTE | MASK_VI_MUTE);
    }
    ret |= tc358743_wr16_and_or(sccb_handle, CONFCTL, ~(MASK_VBUFEN | MASK_ABUFEN),
                                enable ? (MASK_VBUFEN | MASK_ABUFEN) : 0);
    return ret;
}

static esp_err_t tc358743_set_ref_clk(esp_sccb_io_handle_t sccb_handle, uint32_t refclk_hz)
{
    esp_err_t ret = ESP_OK;
    uint32_t sys_freq = refclk_hz / 10000;
    ret = tc358743_wr8(sccb_handle, SYS_FREQ0, sys_freq & 0x00ff);
    ret |= tc358743_wr8(sccb_handle, SYS_FREQ1, (sys_freq & 0xff00) >> 8);

    ret |= tc358743_wr8_and_or(sccb_handle, PHY_CTL0, (uint8_t)~MASK_PHY_SYSCLK_IND, (refclk_hz == 42000000) ? MASK_PHY_SYSCLK_IND : 0);

    uint16_t fh_min = refclk_hz / 100000;
    ret |= tc358743_wr8(sccb_handle, FH_MIN0, fh_min & 0x00ff);
    ret |= tc358743_wr8(sccb_handle, FH_MIN1, (fh_min & 0xff00) >> 8);

    uint16_t fh_max = (uint16_t)((fh_min * 66) / 10);
    ret |= tc358743_wr8(sccb_handle, FH_MAX0, fh_max & 0x00ff);
    ret |= tc358743_wr8(sccb_handle, FH_MAX1, (fh_max & 0xff00) >> 8);

    uint32_t lockdet_ref = refclk_hz / 100;
    ret |= tc358743_wr8(sccb_handle, LOCKDET_REF0, lockdet_ref & 0x0000ff);
    ret |= tc358743_wr8(sccb_handle, LOCKDET_REF1, (lockdet_ref & 0x00ff00) >> 8);
    ret |= tc358743_wr8(sccb_handle, LOCKDET_REF2, (lockdet_ref & 0x0f0000) >> 16);

    ret |= tc358743_wr8_and_or(sccb_handle, NCO_F0_MOD, (uint8_t)~MASK_NCO_F0_MOD, (refclk_hz == 27000000) ? MASK_NCO_F0_MOD_27MHZ : 0);

    uint32_t cec_freq = (656u * sys_freq) / 4200u;
    ret |= tc358743_wr16(sccb_handle, 0x0028, (uint16_t)cec_freq);
    ret |= tc358743_wr16(sccb_handle, 0x002a, (uint16_t)cec_freq);
    return ret;
}

static esp_err_t tc358743_set_pll(esp_sccb_io_handle_t sccb_handle, uint32_t refclk_hz, uint16_t pll_prd, uint16_t pll_fbd)
{
    esp_err_t ret = ESP_OK;
    uint16_t pllctl0 = 0;
    uint16_t pllctl1 = 0;
    ret |= tc358743_rd16(sccb_handle, PLLCTL0, &pllctl0);
    ret |= tc358743_rd16(sccb_handle, PLLCTL1, &pllctl1);
    uint16_t pllctl0_new = SET_PLL_PRD(pll_prd) | SET_PLL_FBD(pll_fbd);
    uint32_t hsck = (refclk_hz / pll_prd) * pll_fbd;

    if ((pllctl0 != pllctl0_new) || ((pllctl1 & MASK_PLL_EN) == 0)) {
        uint16_t pll_frs;
        if (hsck > 500000000) {
            pll_frs = 0x0;
        } else if (hsck > 250000000) {
            pll_frs = 0x1;
        } else if (hsck > 125000000) {
            pll_frs = 0x2;
        } else {
            pll_frs = 0x3;
        }

        ret |= tc358743_sleep_mode(sccb_handle, true);
        ret |= tc358743_wr16(sccb_handle, PLLCTL0, pllctl0_new);
        ret |= tc358743_wr16_and_or(sccb_handle, PLLCTL1, (uint16_t) ~(MASK_PLL_FRS | MASK_RESETB | MASK_PLL_EN),
                                    SET_PLL_FRS(pll_frs) | MASK_RESETB | MASK_PLL_EN);
        vTaskDelay(pdMS_TO_TICKS(1));
        ret |= tc358743_wr16_and_or(sccb_handle, PLLCTL1, (uint16_t)~MASK_CKEN, MASK_CKEN);
        ret |= tc358743_sleep_mode(sccb_handle, false);
    }
    return ret;
}

static esp_err_t tc358743_set_hdmi_hdcp(esp_sccb_io_handle_t sccb_handle, bool enable)
{
    esp_err_t ret = ESP_OK;
    if (enable) {
        return ret;
    }

    ret = tc358743_wr8_and_or(sccb_handle, HDCP_MODE, ~MASK_MANUAL_AUTHENTICATION, MASK_MANUAL_AUTHENTICATION);
    return ret;
}

static esp_err_t tc358743_set_hdmi_phy(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;
    struct tc358743_cam *cam_tc358743 = (struct tc358743_cam *)dev->priv;

    ret |= tc358743_wr8_and_or(dev->sccb_handle, PHY_EN, (uint8_t)~MASK_ENABLE_PHY, 0);
    ret |= tc358743_wr8(dev->sccb_handle, PHY_CTL1, SET_PHY_AUTO_RST1_US(1600) | SET_FREQ_RANGE_MODE_CYCLES(1));
    ret |= tc358743_wr8_and_or(dev->sccb_handle, PHY_CTL2, (uint8_t)~MASK_PHY_AUTO_RSTn,
                               (cam_tc358743->tc358743_para.hdmi_phy_auto_reset_tmds_detected ? MASK_PHY_AUTO_RST2 : 0) |
                               (cam_tc358743->tc358743_para.hdmi_phy_auto_reset_tmds_in_range ? MASK_PHY_AUTO_RST3 : 0) |
                               (cam_tc358743->tc358743_para.hdmi_phy_auto_reset_tmds_valid ? MASK_PHY_AUTO_RST4 : 0));
    ret |= tc358743_wr8(dev->sccb_handle, PHY_BIAS, 0x40);
    ret |= tc358743_wr8(dev->sccb_handle, PHY_CSQ, SET_CSQ_CNT_LEVEL(0x0a));
    ret |= tc358743_wr8(dev->sccb_handle, AVM_CTL, 45);
    ret |= tc358743_wr8_and_or(dev->sccb_handle, HDMI_DET, (uint8_t)~MASK_HDMI_DET_V, (uint8_t)(cam_tc358743->tc358743_para.hdmi_detection_delay << 4));
    ret |= tc358743_wr8_and_or(dev->sccb_handle, HV_RST, (uint8_t) ~(MASK_H_PI_RST | MASK_V_PI_RST),
                               (cam_tc358743->tc358743_para.hdmi_phy_auto_reset_hsync_out_of_range ? MASK_H_PI_RST : 0) |
                               (cam_tc358743->tc358743_para.hdmi_phy_auto_reset_vsync_out_of_range ? MASK_V_PI_RST : 0));
    ret |= tc358743_wr8_and_or(dev->sccb_handle, PHY_EN, (uint8_t)~MASK_ENABLE_PHY, MASK_ENABLE_PHY);
    return ret;
}

static esp_err_t tc358743_set_hdmi_audio(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;
    ret |= tc358743_wr8(dev->sccb_handle, FORCE_MUTE, 0x00);
    ret |= tc358743_wr8(dev->sccb_handle, AUTO_CMD0, MASK_AUTO_MUTE7 | MASK_AUTO_MUTE6 | MASK_AUTO_MUTE5 | MASK_AUTO_MUTE4 | MASK_AUTO_MUTE1 | MASK_AUTO_MUTE0);
    ret |= tc358743_wr8(dev->sccb_handle, AUTO_CMD1, MASK_AUTO_MUTE9);
    ret |= tc358743_wr8(dev->sccb_handle, AUTO_CMD2, MASK_AUTO_PLAY3 | MASK_AUTO_PLAY2);
    ret |= tc358743_wr8(dev->sccb_handle, BUFINIT_START, SET_BUFINIT_START_MS(500));
    ret |= tc358743_wr8(dev->sccb_handle, FS_MUTE, 0x00);
    ret |= tc358743_wr8(dev->sccb_handle, FS_IMODE, MASK_NLPCM_SMODE | MASK_FS_SMODE);
    ret |= tc358743_wr8(dev->sccb_handle, ACR_MODE, MASK_CTS_MODE);
    ret |= tc358743_wr8(dev->sccb_handle, ACR_MDF0, MASK_ACR_L2MDF_1976_PPM | MASK_ACR_L1MDF_976_PPM);
    ret |= tc358743_wr8(dev->sccb_handle, ACR_MDF1, MASK_ACR_L3MDF_3906_PPM);
    ret |= tc358743_wr8(dev->sccb_handle, SDO_MODE1, MASK_SDO_FMT_I2S);
    ret |= tc358743_wr8(dev->sccb_handle, DIV_MODE, SET_DIV_DLY_MS(100));
    ret |= tc358743_wr16_and_or(dev->sccb_handle, CONFCTL, 0xffff, MASK_AUDCHNUM_2 | MASK_AUDOUTSEL_I2S | MASK_AUTOINDEX);
    return ret;
}

static esp_err_t tc358743_set_hdmi_info_frame(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;
    ret |= tc358743_wr8(dev->sccb_handle, PK_INT_MODE,
                        MASK_ISRC2_INT_MODE | MASK_ISRC_INT_MODE | MASK_ACP_INT_MODE | MASK_VS_INT_MODE | MASK_SPD_INT_MODE |
                        MASK_MS_INT_MODE | MASK_AUD_INT_MODE | MASK_AVI_INT_MODE);
    ret |= tc358743_wr8(dev->sccb_handle, NO_PKT_LIMIT, 0x2c);
    ret |= tc358743_wr8(dev->sccb_handle, NO_PKT_CLR, 0x53);
    ret |= tc358743_wr8(dev->sccb_handle, ERR_PK_LIMIT, 0x01);
    ret |= tc358743_wr8(dev->sccb_handle, NO_PKT_LIMIT2, 0x30);
    ret |= tc358743_wr8(dev->sccb_handle, NO_GDB_LIMIT, 0x10);
    return ret;
}

static esp_err_t tc358743_initial_setup(esp_cam_sensor_device_t *dev)
{
    struct tc358743_cam *cam_tc358743 = (struct tc358743_cam *)dev->priv;
    esp_err_t ret = ESP_OK;
    ret |= tc358743_wr16_and_or(dev->sccb_handle, SYSCTL, (uint16_t) ~(MASK_IRRST | MASK_CECRST), MASK_IRRST | MASK_CECRST);
    ret |= tc358743_reset_blocks(dev->sccb_handle, MASK_CTXRST | MASK_HDMIRST);
    ret |= tc358743_sleep_mode(dev->sccb_handle, false);

    ret |= tc358743_wr16(dev->sccb_handle, FIFOCTL, cam_tc358743->tc358743_para.fifo_level);
    ret |= tc358743_set_ref_clk(dev->sccb_handle, cam_tc358743->tc358743_para.refclk_hz);
    ret |= tc358743_wr8_and_or(dev->sccb_handle, DDC_CTL, (uint8_t)~MASK_DDC5V_MODE, cam_tc358743->tc358743_para.ddc5v_mode & MASK_DDC5V_MODE);
    ret |= tc358743_wr8_and_or(dev->sccb_handle, EDID_MODE, (uint8_t)~MASK_EDID_MODE, MASK_EDID_MODE_E_DDC);

    ret |= tc358743_set_hdmi_phy(dev);
    ret |= tc358743_set_hdmi_hdcp(dev->sccb_handle, cam_tc358743->tc358743_para.enable_hdcp);
    ret |= tc358743_set_hdmi_audio(dev);
    ret |= tc358743_set_hdmi_info_frame(dev);

    ret |= tc358743_wr8_and_or(dev->sccb_handle, VI_MODE, (uint8_t)~MASK_RGB_DVI, 0);
    ret |= tc358743_wr8_and_or(dev->sccb_handle, VOUT_SET2, (uint8_t)~MASK_VOUTCOLORMODE, MASK_VOUTCOLORMODE_AUTO);
    ret |= tc358743_wr8(dev->sccb_handle, VOUT_SET3, MASK_VOUT_EXTCNT);
    return ret;
}

/** RGB888 Linux tc358743_set_csi_color_space(RGB888_1X24). */
static esp_err_t tc358743_set_csi_color_space_rgb888(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;
    ret |= tc358743_wr8_and_or(dev->sccb_handle, VOUT_SET2, (uint8_t) ~(MASK_SEL422 | MASK_VOUT_422FIL_100), 0);
    ret |= tc358743_wr8_and_or(dev->sccb_handle, VI_REP, (uint8_t)~MASK_VOUT_COLOR_SEL, MASK_VOUT_COLOR_RGB_FULL);
    ret |= tc358743_wr16_and_or(dev->sccb_handle, CONFCTL, (uint16_t)~MASK_YCBCRFMT, 0);
    return ret;
}

/** UYVY 16-bit Linux tc358743_set_csi_color_space(MEDIA_BUS_FMT_UYVY8_1X16). */
static esp_err_t tc358743_set_csi_color_space_uyvy422(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;
    ret |= tc358743_wr8_and_or(dev->sccb_handle, VOUT_SET2, (uint8_t) ~(MASK_SEL422 | MASK_VOUT_422FIL_100),
                               (uint8_t)(MASK_SEL422 | MASK_VOUT_422FIL_100));
    ret |= tc358743_wr8_and_or(dev->sccb_handle, VI_REP, (uint8_t)~MASK_VOUT_COLOR_SEL, MASK_VOUT_COLOR_601_YCBCR_FULL);
    ret |= tc358743_wr16_and_or(dev->sccb_handle, CONFCTL, (uint16_t)~MASK_YCBCRFMT, MASK_YCBCRFMT_422_8_BIT);
    return ret;
}

static esp_err_t tc358743_apply_csi_color_space(esp_cam_sensor_device_t *dev)
{
    struct tc358743_cam *cam_tc358743 = (struct tc358743_cam *)dev->priv;
    if (cam_tc358743->tc358743_para.csi_uyvy422) {
        return tc358743_set_csi_color_space_uyvy422(dev);
    } else {
        return tc358743_set_csi_color_space_rgb888(dev);
    }
    return ESP_OK;
}

static esp_err_t tc358743_set_csi_lanes(esp_cam_sensor_device_t *dev, unsigned lanes)
{
    struct tc358743_cam *cam_tc358743 = (struct tc358743_cam *)dev->priv;
    esp_err_t ret = ESP_OK;
    ret |= tc358743_reset_blocks(dev->sccb_handle, MASK_CTXRST);

    if (lanes < 1) {
        ret |= tc358743_wr32(dev->sccb_handle, CLW_CNTRL, MASK_CLW_LANEDISABLE);
    }
    if (lanes < 1) {
        ret |= tc358743_wr32(dev->sccb_handle, D0W_CNTRL, MASK_D0W_LANEDISABLE);
    }
    if (lanes < 2) {
        ret |= tc358743_wr32(dev->sccb_handle, D1W_CNTRL, MASK_D1W_LANEDISABLE);
    }
    if (lanes < 3) {
        ret |= tc358743_wr32(dev->sccb_handle, D2W_CNTRL, MASK_D2W_LANEDISABLE);
    }
    if (lanes < 4) {
        ret |= tc358743_wr32(dev->sccb_handle, D3W_CNTRL, MASK_D3W_LANEDISABLE);
    }

    ret |= tc358743_wr32(dev->sccb_handle, LINEINITCNT, cam_tc358743->tc358743_para.lineinitcnt);
    ret |= tc358743_wr32(dev->sccb_handle, LPTXTIMECNT, cam_tc358743->tc358743_para.lptxtimecnt);
    ret |= tc358743_wr32(dev->sccb_handle, TCLK_HEADERCNT, cam_tc358743->tc358743_para.tclk_headercnt);
    ret |= tc358743_wr32(dev->sccb_handle, TCLK_TRAILCNT, cam_tc358743->tc358743_para.tclk_trailcnt);
    ret |= tc358743_wr32(dev->sccb_handle, THS_HEADERCNT, cam_tc358743->tc358743_para.ths_headercnt);
    ret |= tc358743_wr32(dev->sccb_handle, TWAKEUP, cam_tc358743->tc358743_para.twakeup);
    ret |= tc358743_wr32(dev->sccb_handle, TCLK_POSTCNT, cam_tc358743->tc358743_para.tclk_postcnt);
    ret |= tc358743_wr32(dev->sccb_handle, THS_TRAILCNT, cam_tc358743->tc358743_para.ths_trailcnt);
    ret |= tc358743_wr32(dev->sccb_handle, HSTXVREGCNT, cam_tc358743->tc358743_para.hstxvregcnt);

    ret |= tc358743_wr32(dev->sccb_handle, HSTXVREGEN,
                         ((lanes > 0) ? MASK_CLM_HSTXVREGEN : 0) | ((lanes > 0) ? MASK_D0M_HSTXVREGEN : 0) |
                         ((lanes > 1) ? MASK_D1M_HSTXVREGEN : 0) | ((lanes > 2) ? MASK_D2M_HSTXVREGEN : 0) |
                         ((lanes > 3) ? MASK_D3M_HSTXVREGEN : 0));

    /* Linux tc358743 set_csi(): TXOPTIONCNTRL = 0 (non-continuous MIPI clock). */
    ret |= tc358743_wr32(dev->sccb_handle, TXOPTIONCNTRL, 0);
    ret |= tc358743_wr32(dev->sccb_handle, STARTCNTRL, MASK_START);
    ret |= tc358743_wr32(dev->sccb_handle, CSI_START, MASK_STRT);

    uint32_t nol = (lanes == 4) ? MASK_NOL_4 : (lanes == 3) ? MASK_NOL_3 : (lanes == 2) ? MASK_NOL_2 : MASK_NOL_1;

    ret |= tc358743_wr32(dev->sccb_handle, CSI_CONFW, MASK_MODE_SET | MASK_ADDRESS_CSI_CONTROL | MASK_CSI_MODE | MASK_TXHSMD | nol);
    ret |= tc358743_wr32(dev->sccb_handle, CSI_CONFW, MASK_MODE_SET | MASK_ADDRESS_CSI_ERR_INTENA | MASK_TXBRK | MASK_QUNK | MASK_WCER | MASK_INER);

    ret |= tc358743_wr32(dev->sccb_handle, CSI_CONFW, MASK_MODE_CLEAR | MASK_ADDRESS_CSI_ERR_HALT | MASK_TXBRK | MASK_QUNK);

    ret |= tc358743_wr32(dev->sccb_handle, CSI_CONFW, MASK_MODE_SET | MASK_ADDRESS_CSI_INT_ENA | MASK_INTER);
    return ret;
}

static esp_err_t tc358743_hpd_set(esp_cam_sensor_device_t *dev, bool on)
{
    return tc358743_wr8_and_or(dev->sccb_handle, HPD_CTL, (uint8_t)~MASK_HPD_OUT0, on ? MASK_HPD_OUT0 : 0);
}

/**
 * Load EDID into internal RAM (HPD must stay low, source must not DDC during this).
 * Caller raises HPD after PLL/CSI and any other sink setup (Linux: delayed hotplug ~143 ms).
 */
static esp_err_t tc358743_edid_write_builtin(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;
    const uint16_t edid_len = TC358743_EDID_TOTAL_LEN;
    ret |= tc358743_wr8(dev->sccb_handle, EDID_LEN1, edid_len & 0xff);
    ret |= tc358743_wr8(dev->sccb_handle, EDID_LEN2, edid_len >> 8);
    for (uint16_t i = 0; i < edid_len; i += 128) {
        ret |= tc358743_write(dev->sccb_handle, EDID_RAM + i, tc358743_edid_bin + i, 128);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    return ret;
}

static esp_err_t tc358743_get_sensor_id(esp_cam_sensor_device_t *dev, uint16_t *id)
{
    return tc358743_rd16(dev->sccb_handle, CHIPID, id);
}

static esp_err_t tc358743_sys_status(esp_cam_sensor_device_t *dev, uint8_t *out_st)
{
    return tc358743_rd8(dev->sccb_handle, SYS_STATUS, out_st);
}

static esp_err_t tc358743_debug_status(esp_cam_sensor_device_t *dev)
{
    uint8_t st = 0;
    if (tc358743_sys_status(dev, &st) != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "SYS_STATUS=0x%02x (TMDS=%d HDMI=%d SYNC=%d DDC5V=%d)", st, (int)(st >> 1) & 1,
             (int)(st >> 4) & 1, (int)(st >> 7) & 1, (int)st & 1);
    return ESP_OK;
}

static esp_err_t tc358743_init_streaming(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    struct tc358743_cam *cam_tc358743 = (struct tc358743_cam *)dev->priv;
    esp_err_t ret = ESP_OK;
    /* HPD low so the source does not DDC/EDID until we are ready (matches old bridge_init + Linux). */
    ret |= tc358743_hpd_set(dev, false);
    vTaskDelay(pdMS_TO_TICKS(20));

    ret |= tc358743_initial_setup(dev);
    ret |= tc358743_edid_write_builtin(dev);

    ret |= tc358743_enable_stream(dev->sccb_handle, false);
    ret |= tc358743_set_pll(dev->sccb_handle, cam_tc358743->tc358743_para.refclk_hz, cam_tc358743->tc358743_para.pll_prd, cam_tc358743->tc358743_para.pll_fbd);
    ret |= tc358743_set_csi_lanes(dev, format->mipi_info.lane_num);
    cam_tc358743->tc358743_para.csi_uyvy422 = (format->format == ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY);
    ret |= tc358743_apply_csi_color_space(dev);

    ret |= tc358743_wr16(dev->sccb_handle, INTSTATUS, 0xffff);
    ret |= tc358743_wr16(dev->sccb_handle, INTMASK, (uint16_t)(~(MASK_HDMI_MSK | MASK_CSI_MSK) & 0xffff));

    /* HPD and enable_stream(true): call tc358743_enable_hdmi_output() before esp_cam_ctlr_start() so MIPI is active. */
    ret |= tc358743_debug_status(dev);
    return ret;
}

static esp_err_t tc358743_enable_hdmi_output(esp_cam_sensor_device_t *dev)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    esp_err_t ret = ESP_OK;
    /* Same tail as old bridge_init: video FIFO on, then delayed hotplug edge. */
    ret |= tc358743_enable_stream(dev->sccb_handle, true);
    vTaskDelay(pdMS_TO_TICKS(150));
    ret |= tc358743_hpd_set(dev, true);
    vTaskDelay(pdMS_TO_TICKS(50));
    /* STRT after CONFCTL enables video, some boards leave CSI TX idle until this is rewritten. */
    ret |= tc358743_wr32(dev->sccb_handle, CSI_START, MASK_STRT);
    ret |= tc358743_debug_status(dev);
    return ret;
}

static esp_err_t tc358743_hdmi_hotplug_reset(esp_cam_sensor_device_t *dev)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    esp_err_t ret = ESP_OK;
    ret |= tc358743_enable_stream(dev->sccb_handle, false);
    ret |= tc358743_hpd_set(dev, false);
    vTaskDelay(pdMS_TO_TICKS(150));
    ret |= tc358743_enable_hdmi_output(dev);
    return ret;
}

static esp_err_t tc358743_reapply_csi_path_after_hdmi(esp_cam_sensor_device_t *dev)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    esp_err_t ret = ESP_OK;
    /*
     * Programming CSI before TMDS can leave MIPI idle; reapply after lock.
     * Note: VI_STATUS1==0 on TC9590-class maps means 444/24p/no GBD, not "no video"; use HAct/VAct.
     */
    ret |= tc358743_apply_csi_color_space(dev);
    ret |= tc358743_set_csi_lanes(dev, dev->cur_format->mipi_info.lane_num);
    ret |= tc358743_wr32(dev->sccb_handle, CSI_START, MASK_STRT);
    return ret;
}

static void wait_tc358743_pixel_stream(esp_cam_sensor_device_t *dev, uint32_t timeout_ms)
{
    const uint32_t step = 50;
    uint32_t waited = 0;
    while (waited < timeout_ms) {
        uint8_t st = 0;
        if (tc358743_sys_status(dev, &st) == ESP_OK && (st & 0x02) != 0 && (st & 0x80) != 0) {
            ESP_LOGI(TAG, "HDMI ready SYS_STATUS=0x%02x after %" PRIu32 " ms", st, waited);
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(step));
        waited += step;
    }
    ESP_LOGW(TAG, "HDMI lock wait %" PRIu32 " ms - starting CSI anyway", timeout_ms);
}

#if TC358743_ADV_DEBUG_EN
static esp_err_t tc358743_read_hact_vact_htotal(esp_cam_sensor_device_t *dev, uint16_t *hact, uint16_t *vact,
        uint16_t *htotal, uint16_t *vtotal)
{
    esp_err_t ret = ESP_OK;
    uint8_t h0 = 0;
    uint8_t h1 = 0;
    uint8_t v0 = 0;
    uint8_t v1 = 0;
    uint8_t ht0 = 0;
    uint8_t ht1 = 0;
    uint8_t vt0 = 0;
    uint8_t vt1 = 0;

    ret |= tc358743_rd8(dev->sccb_handle, HACT0, &h0);
    ret |= tc358743_rd8(dev->sccb_handle, HACT1, &h1);
    ret |= tc358743_rd8(dev->sccb_handle, VACT0, &v0);
    ret |= tc358743_rd8(dev->sccb_handle, VACT1, &v1);
    ret |= tc358743_rd8(dev->sccb_handle, HTOTAL0, &ht0);
    ret |= tc358743_rd8(dev->sccb_handle, HTOTAL1, &ht1);
    ret |= tc358743_rd8(dev->sccb_handle, VTOTAL0, &vt0);
    ret |= tc358743_rd8(dev->sccb_handle, VTOTAL1, &vt1);
    if (ret != ESP_OK) {
        return ret;
    }

    *hact = (uint16_t)h0 | ((uint16_t)(h1 & 0x1fu) << 8);
    *vact = (uint16_t)v0 | ((uint16_t)(v1 & 0x1fu) << 8);
    *htotal = (uint16_t)ht0 | ((uint16_t)(ht1 & 0x1fu) << 8);
    *vtotal = (uint16_t)vt0 | ((uint16_t)(vt1 & 0x3fu) << 8);
    return ret;
}

static void tc358743_debug_stall_extras(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;
    uint16_t conf = 0;
    uint32_t csi_err = 0;

    ret |= tc358743_rd16(dev->sccb_handle, CONFCTL, &conf);
    if (ret == ESP_OK) {
        unsigned yfmt = (unsigned)((conf >> 6) & 3u);
        ESP_LOGW(TAG,
                 "stall CONFCTL=0x%04x YCbCrFmt=%u (0=444 1=422_12 2=colorbar 3=422_8) VBUFEN:%u ABUFEN:%u",
                 conf, yfmt, (unsigned)(conf & 1u), (unsigned)((conf >> 1) & 1u));
    }

    ret |= tc358743_rd32(dev->sccb_handle, CSI_ERR, &csi_err);
    if (ret == ESP_OK) {
        ESP_LOGW(TAG,
                 "stall CSI_ERR=0x%08" PRIx32 " (Linux: INER=0x200 WCER=0x100 QUNK=0x10 TXBRK=0x2)", csi_err);
    }

    /*
     * TC9590XBG Table 4-2 / §6.8: contiguous read from PK_AVI_0HEAD (0x8710) yields
     * avi[0..2]=HB0..2, avi[3]=checksum, avi[4]=PB0, avi[5]=PB1, ... (CEA-861 payload).
     * §4.2: YCbCr444 24bpp uses the same CSI-2 DataType as RGB888 (0x24); §4.3 Y→G Cr→R Cb→B.
     */
    uint8_t avi[24];
    memset(avi, 0, sizeof(avi));
    ret = tc358743_read(dev->sccb_handle, PK_AVI_0HEAD, avi, PK_AVI_LEN);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "stall AVI read %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, avi, PK_AVI_LEN, ESP_LOG_WARN);
    if (PK_AVI_LEN >= 4u) {
        ESP_LOGW(TAG,
                 "stall AVI layout (Table 4-2): HB=[%02x %02x %02x] chksum=%02x PB0=%02x PB1=%02x PB2=%02x PB3=%02x",
                 avi[0], avi[1], avi[2], avi[3], avi[4], avi[5], avi[6], avi[7]);
    }
    ESP_LOGW(TAG,
             "stall MIPI note (§4.2): RGB888 and HDMI YCbCr444 24bpp both use DT 0x24: esp_cam RGB888 matches 444-out");

    /* CEA-861 AVI v2: PB1 Y2:Y1:Y0 = bits 7..5 of avi[5]; PB4 VIC = avi[8] bits 6..0 */
    if (avi[0] == 0x82u && avi[2] >= 2u && PK_AVI_LEN >= 6u) {
        uint8_t pb1 = avi[5];
        unsigned y = (unsigned)(pb1 >> 5) & 7u;
        const char *ys =
            (y == 0) ? "RGB" : (y == 1) ? "YCbCr422" : (y == 2) ? "YCbCr444" : (y == 3) ? "YCbCr420" : "other/RSVD";
        ESP_LOGW(TAG, "stall AVI CEA: PB1 Y=%u (%s)", y, ys);
        if (PK_AVI_LEN > 8u) {
            unsigned vic = (unsigned)(avi[8] & 0x7fu);
            ESP_LOGW(TAG, "stall AVI CEA: PB4 VIC=%u (0=unspecified per packet)", vic);
        }
    }
    return;
}

static void tc358743_debug_bridge(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;
    uint8_t sys = 0;
    uint8_t vi = 0;
    uint8_t vi2 = 0;
    uint8_t vi3 = 0;
    uint8_t clkst = 0;
    uint8_t phyerr = 0;
    uint8_t hdmi_dvi = 0;
    uint8_t vout2 = 0;
    uint8_t vimute = 0;
    uint16_t csi = 0;
    uint16_t csi_ctl = 0;
    uint16_t csi_int = 0;
    uint16_t intst = 0;
    uint16_t conf = 0;
    uint16_t hact = 0, vact = 0, htotal = 0, vtotal = 0;

    ret |= tc358743_rd8(dev->sccb_handle, SYS_STATUS, &sys);
    ret |= tc358743_rd8(dev->sccb_handle, VI_STATUS1, &vi);
    ret |= tc358743_rd8(dev->sccb_handle, VI_STATUS2, &vi2);
    ret |= tc358743_rd8(dev->sccb_handle, VI_STATUS3, &vi3);
    ret |= tc358743_rd8(dev->sccb_handle, CLK_STATUS, &clkst);
    ret |= tc358743_rd8(dev->sccb_handle, PHYERR_STATUS, &phyerr);
    ret |= tc358743_rd8(dev->sccb_handle, HDMI_DVI, &hdmi_dvi);
    ret |= tc358743_rd16(dev->sccb_handle, CSI_STATUS, &csi);
    ret |= tc358743_rd16(dev->sccb_handle, CSI_CONTROL, &csi_ctl);
    ret |= tc358743_rd16(dev->sccb_handle, CSI_INT, &csi_int);
    ret |= tc358743_rd16(dev->sccb_handle, INTSTATUS, &intst);
    ret |= tc358743_rd16(dev->sccb_handle, CONFCTL, &conf);
    ret |= tc358743_rd8(dev->sccb_handle, VOUT_SET2, &vout2);
    ret |= tc358743_rd8(dev->sccb_handle, VI_MUTE, &vimute);
    ret |= tc358743_read_hact_vact_htotal(dev, &hact, &vact, &htotal, &vtotal);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "tc358743_debug_bridge: %s", esp_err_to_name(ret));
        return;
    }

    /* Bit layout per Linux tc358743_regs.h (matches Toshiba REF_01). */
    unsigned csi_hlt = (unsigned)(csi & 1u);
    unsigned csi_rxact = (unsigned)((csi >> 8) & 1u);
    unsigned csi_txact = (unsigned)((csi >> 9) & 1u);
    unsigned csi_wsync = (unsigned)((csi >> 10) & 1u);
    unsigned csi_int_hlt = (unsigned)((csi_int >> 3) & 1u);
    unsigned csi_inter = (unsigned)((csi_int >> 2) & 1u);

    ESP_LOGW(TAG,
             "bridge SYS=0x%02x VI1=0x%02x VI2=0x%02x VI3=0x%02x CONFCTL=0x%04x VOUT2=0x%02x VI_MUTE=0x%02x",
             sys, vi, vi2, vi3, conf, vout2, vimute);
    ESP_LOGW(TAG,
             "  timing HAct=%u VAct=%u HTot=%u VTot=%u | HDMI_DVI=0x%02x CLK_ST=0x%02x PHYERR=0x%02x INTSTATUS=0x%04x",
             (unsigned)hact, (unsigned)vact, (unsigned)htotal, (unsigned)vtotal, hdmi_dvi, clkst, phyerr, intst);
    ESP_LOGW(TAG,
             " CSI_STATUS=0x%04x (Hlt:%u RxAct:%u TxAct:%u WSync:%u) CSIctl=0x%04x CSIint=0x%04x (IntHlt:%u INTER:%u)",
             csi, csi_hlt, csi_rxact, csi_txact, csi_wsync, csi_ctl, csi_int, csi_int_hlt, csi_inter);
}
#endif /* TC358743_ADV_DEBUG_EN */

static esp_err_t tc358743_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return ESP_OK;
}

// recover for HDMI hotplug
static esp_err_t tc358743_soft_reset(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = tc358743_hdmi_hotplug_reset(dev);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "tc358743_hdmi_hotplug_reset: %s", esp_err_to_name(ret));
        return ret;
    }
    wait_tc358743_pixel_stream(dev, TC358743_HDMI_LOCK_WAIT_MS);
    ret = tc358743_reapply_csi_path_after_hdmi(dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "tc358743_reapply_csi_path_after_hdmi: %s", esp_err_to_name(ret));
    }
    return ret;
}

static void tc358743_cfg_defaults(struct tc358743_cam *cam_tc358743, const esp_cam_sensor_format_t *format)
{
    memset(cam_tc358743, 0, sizeof(struct tc358743_cam));
    cam_tc358743->tc358743_para.refclk_hz = TC358743_REFCLK_27M_HZ;
    cam_tc358743->tc358743_para.pll_prd = cam_tc358743->tc358743_para.refclk_hz / 6000000u;
    const uint32_t lane_mbps = (format->format == ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY) ?
                               TC358743_1920X1080_YUV422_30FPS_MIPI_LANE_MBPS :
                               TC358743_1920X1080_RGB888_30FPS_MIPI_LANE_MBPS;
    const uint32_t bps_per_lane = lane_mbps * 1000000u;
    cam_tc358743->tc358743_para.pll_fbd = (uint16_t)(bps_per_lane / cam_tc358743->tc358743_para.refclk_hz * cam_tc358743->tc358743_para.pll_prd);
    cam_tc358743->tc358743_para.fifo_level = 374;
    cam_tc358743->tc358743_para.lanes = 2;
    cam_tc358743->tc358743_para.ddc5v_mode = 0x02;
    cam_tc358743->tc358743_para.lineinitcnt = 0x1b58;
    cam_tc358743->tc358743_para.lptxtimecnt = 0x007;
    cam_tc358743->tc358743_para.tclk_headercnt = 0x2806;
    cam_tc358743->tc358743_para.tclk_trailcnt = 0x00;
    cam_tc358743->tc358743_para.ths_headercnt = 0x0806;
    cam_tc358743->tc358743_para.twakeup = 0x4268;
    cam_tc358743->tc358743_para.tclk_postcnt = 0x008;
    cam_tc358743->tc358743_para.ths_trailcnt = 0x5;
    cam_tc358743->tc358743_para.hstxvregcnt = 0;
    cam_tc358743->tc358743_para.enable_hdcp = false;
    cam_tc358743->tc358743_para.hdmi_detection_delay = 0;
}

static esp_err_t tc358743_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    esp_err_t ret = tc358743_enable_stream(dev->sccb_handle, enable);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Enable stream fail");
        return ret;
    }
    dev->stream_status = enable ? true : false;
    if (!enable) {
        // Put all lanes in LP-11 state (STOPSTATE)
        ret = tc358743_set_csi_lanes(dev, dev->cur_format->mipi_info.lane_num);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Set CSI lanes fail");
            return ret;
        }
    }
#if TC358743_ADV_DEBUG_EN
    tc358743_debug_bridge(dev);
    tc358743_debug_stall_extras(dev);
#endif
    return ret;
}

// todo, check data seq
static esp_err_t tc358743_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, qdesc);

    esp_err_t ret = ESP_OK;
    switch (qdesc->id) {
    case ESP_CAM_SENSOR_DATA_SEQ:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_U8;
        qdesc->u8.size = sizeof(uint32_t);
        break;
    default: {
        ESP_LOGD(TAG, "id=%"PRIx32" is not supported", qdesc->id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }
    return ret;
}

static esp_err_t tc358743_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;

    switch (id) {
    case ESP_CAM_SENSOR_DATA_SEQ:
        if (dev->cur_format->format == ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY) {
            *(uint32_t *)arg = ESP_CAM_SENSOR_DATA_SEQ_WORD_INTERNAL_SWAPPED;
        } else {
            *(uint32_t *)arg = ESP_CAM_SENSOR_DATA_SEQ_NONE;
        }
        break;
    default:
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }

    return ret;
}

static esp_err_t tc358743_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t tc358743_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);

    formats->count = ARRAY_SIZE(tc358743_format_info);
    formats->format_array = &tc358743_format_info[0];
    return ESP_OK;
}

static esp_err_t tc358743_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_yuv = 1;
    return ESP_OK;
}

static esp_err_t tc358743_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    struct tc358743_cam *cam_tc358743 = (struct tc358743_cam *)dev->priv;

    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        format = &tc358743_format_info[get_tc358743_actual_format_index()];
    }

    tc358743_cfg_defaults(cam_tc358743, format);
    ret |= tc358743_init_streaming(dev, format);
    ret |= tc358743_enable_hdmi_output(dev);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set format regs fail");
        return ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT;
    }

    wait_tc358743_pixel_stream(dev, TC358743_HDMI_LOCK_WAIT_MS);
    dev->cur_format = format;

    return ret;
}

static esp_err_t tc358743_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, format);

    esp_err_t ret = ESP_FAIL;

    if (dev->cur_format != NULL) {
        memcpy(format, dev->cur_format, sizeof(esp_cam_sensor_format_t));
        ret = ESP_OK;
    }
    return ret;
}

static esp_err_t tc358743_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    TC358743_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = tc358743_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = tc358743_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = tc358743_wr8(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = tc358743_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = tc358743_rd8(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = tc358743_get_sensor_id(dev, &((esp_cam_sensor_id_t *)arg)->pid);
        break;
    default:
        break;
    }

    TC358743_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t tc358743_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        TC358743_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
        delay_ms(6);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->pwdn_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        ret = gpio_config(&conf);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "gpio config failed");

        gpio_set_level(dev->pwdn_pin, 1);
        delay_ms(5);
    }

    if (dev->reset_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->reset_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        ret = gpio_config(&conf);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "gpio config failed");

        gpio_set_level(dev->reset_pin, 1);
        delay_ms(5);
    }

    return ret;
}

static esp_err_t tc358743_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        TC358743_DISABLE_OUT_XCLK(dev->xclk_pin);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_set_level(dev->pwdn_pin, 0);
        delay_ms(5);
    }

    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(5);
    }

    return ret;
}

static esp_err_t tc358743_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del tc358743 (%p)", dev);
    if (dev) {
        if (dev->priv) {
            free(dev->priv);
            dev->priv = NULL;
        }
        tc358743_power_off(dev);
        free(dev);
        dev = NULL;
    }

    return ESP_OK;
}

static const esp_cam_sensor_ops_t tc358743_ops = {
    .query_para_desc = tc358743_query_para_desc,
    .get_para_value = tc358743_get_para_value,
    .set_para_value = tc358743_set_para_value,
    .query_support_formats = tc358743_query_support_formats,
    .query_support_capability = tc358743_query_support_capability,
    .set_format = tc358743_set_format,
    .get_format = tc358743_get_format,
    .priv_ioctl = tc358743_priv_ioctl,
    .del = tc358743_delete
};

esp_cam_sensor_device_t *tc358743_detect(esp_cam_sensor_config_t *config)
{
    esp_cam_sensor_device_t *dev = NULL;
    struct tc358743_cam *cam_tc358743;
    if (config == NULL) {
        return NULL;
    }

    dev = calloc(1, sizeof(esp_cam_sensor_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    cam_tc358743 = heap_caps_calloc(1, sizeof(struct tc358743_cam), MALLOC_CAP_DEFAULT);
    if (!cam_tc358743) {
        ESP_LOGE(TAG, "failed to calloc cam");
        free(dev);
        return NULL;
    }
    memset(cam_tc358743, 0, sizeof(struct tc358743_cam));

    dev->name = (char *)TC358743_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &tc358743_ops;
    dev->priv = cam_tc358743;
    dev->cur_format = &tc358743_format_info[get_tc358743_actual_format_index()];

    // Configure sensor power, clock, and SCCB port
    if (tc358743_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (tc358743_get_sensor_id(dev, &dev->id.pid) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if ((dev->id.pid >> 8) != TC358743_PID) {
        ESP_LOGE(TAG, "Camera sensor is not TC358743, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }

    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    tc358743_power_off(dev);
    free(dev->priv);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_TC358743_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(tc358743_detect, ESP_CAM_SENSOR_MIPI_CSI, TC358743_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return tc358743_detect(config);
}
#endif
