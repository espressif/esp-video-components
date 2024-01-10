/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/gpio.h"
#include "esp_log.h"

#include "esp_camera.h"
#include "ov2640_settings.h"
#include "sccb.h"

struct ov2640_cam {
    uint8_t jpeg_quality;
    int8_t ae_level;
    int8_t brightness;
    int8_t contrast;
    int8_t saturation;
    uint8_t special_effect;
    uint8_t wb_mode;
};

#define OV2640_IO_MUX_LOCK()
#define OV2640_IO_MUX_UNLOCK()
#define OV2640_SCCB_ADDR   0x30
#define OV2640_PID         0x26
#define OV2640_SENSOR_NAME "OV2640"
#define OV2640_SUPPORT_NUM CONFIG_CAMERA_OV2640_MAX_SUPPORT

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))

#define CAMERA_ENABLE_OUT_CLOCK(pin,clk)
#define CAMERA_DISABLE_OUT_CLOCK(pin)

static esp_camera_device_t *s_ov2640[OV2640_SUPPORT_NUM];
static uint8_t s_ov2640_index;
static volatile ov2640_bank_t reg_bank = BANK_MAX;
static const char *TAG = "ov2640";

static const sensor_format_t ov2640_format_info[] = {
    {
        .index = OV2640_FORMAT_INDEX0,
        .name = "DVP_8bit_20Minput_RGB565_640x480_6fps",
        .format = CAM_SENSOR_PIXFORMAT_RGB565,
        .port = DVP_OUTPUT_8BITS,
        .bayer_type = CAM_SENSOR_BAYER_GBRG,
        .hdr_mode = CAM_SENSOR_HDR_LINEAR,
        .xclk = 20000000,
        .start_pos_x = 4,
        .start_pos_y = 4,
        .width = 640,
        .height = 480,
        .regs = init_reglist_DVP_8bit_RGB565_640x480_XCLK_20_6fps,
        .regs_size = ARRAY_SIZE(init_reglist_DVP_8bit_RGB565_640x480_XCLK_20_6fps),
        .bpp = 16,
        .fps = 6,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
    {
        .index = OV2640_FORMAT_INDEX1,
        .name = "DVP_8bit_20Minput_YUV422_640x480_6fps",
        .format = CAM_SENSOR_PIXFORMAT_YUV422,
        .port = DVP_OUTPUT_8BITS,
        .bayer_type = CAM_SENSOR_BAYER_GBRG,
        .hdr_mode = CAM_SENSOR_HDR_LINEAR,
        .xclk = 20000000,
        .start_pos_x = 4,
        .start_pos_y = 4,
        .width = 640,
        .height = 480,
        .regs = init_reglist_DVP_8bit_YUV422_640x480_XCLK_20_6fps,
        .regs_size = ARRAY_SIZE(init_reglist_DVP_8bit_YUV422_640x480_XCLK_20_6fps),
        .bpp = 16,
        .fps = 6,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
    {
        .index = OV2640_FORMAT_INDEX2,
        .name = "DVP_8bit_20Minput_JPEG_640x480_25fps",
        .format = CAM_SENSOR_PIXFORMAT_JPEG,
        .port = DVP_OUTPUT_8BITS,
        .bayer_type = CAM_SENSOR_BAYER_GBRG,
        .hdr_mode = CAM_SENSOR_HDR_LINEAR,
        .xclk = 20000000,
        .start_pos_x = 4,
        .start_pos_y = 4,
        .width = 640,
        .height = 480,
        .regs = init_reglist_DVP_8bit_JPEG_640x480_XCLK_20_25fps,
        .regs_size = ARRAY_SIZE(init_reglist_DVP_8bit_JPEG_640x480_XCLK_20_25fps),
        .bpp = 8,
        .fps = 25,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
    {
        .index = OV2640_FORMAT_INDEX3,
        .name = "DVP_8bit_20Minput_RGB565_240x240_25fps",
        .format = CAM_SENSOR_PIXFORMAT_RGB565,
        .port = DVP_OUTPUT_8BITS,
        .bayer_type = CAM_SENSOR_BAYER_GBRG,
        .hdr_mode = CAM_SENSOR_HDR_LINEAR,
        .xclk = 20000000,
        .start_pos_x = 4,
        .start_pos_y = 4,
        .width = 240,
        .height = 240,
        .regs = init_reglist_DVP_8bit_RGB565_240x240_XCLK_20_25fps,
        .regs_size = ARRAY_SIZE(init_reglist_DVP_8bit_RGB565_240x240_XCLK_20_25fps),
        .bpp = 16,
        .fps = 25,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
    {
        .index = OV2640_FORMAT_INDEX4,
        .name = "DVP_8bit_20Minput_YUV422_240x240_25fps",
        .format = CAM_SENSOR_PIXFORMAT_YUV422,
        .port = DVP_OUTPUT_8BITS,
        .bayer_type = CAM_SENSOR_BAYER_GBRG,
        .hdr_mode = CAM_SENSOR_HDR_LINEAR,
        .xclk = 20000000,
        .start_pos_x = 4,
        .start_pos_y = 4,
        .width = 240,
        .height = 240,
        .regs = init_reglist_DVP_8bit_YUV422_240x240_XCLK_20_25fps,
        .regs_size = ARRAY_SIZE(init_reglist_DVP_8bit_YUV422_240x240_XCLK_20_25fps),
        .bpp = 16,
        .fps = 25,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
    {
        .index = OV2640_FORMAT_INDEX5,
        .name = "DVP_8bit_20Minput_JPEG_320x240_50fps",
        .format = CAM_SENSOR_PIXFORMAT_JPEG,
        .port = DVP_OUTPUT_8BITS,
        .bayer_type = CAM_SENSOR_BAYER_GBRG,
        .hdr_mode = CAM_SENSOR_HDR_LINEAR,
        .xclk = 20000000,
        .start_pos_x = 4,
        .start_pos_y = 4,
        .width = 320,
        .height = 240,
        .regs = init_reglist_DVP_8bit_JPEG_320x240_XCLK_20_50fps,
        .regs_size = ARRAY_SIZE(init_reglist_DVP_8bit_JPEG_320x240_XCLK_20_50fps),
        .bpp = 8,
        .fps = 50,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
    {
        .index = OV2640_FORMAT_INDEX6,
        .name = "DVP_8bit_20Minput_JPEG_1280x720_12fps",
        .format = CAM_SENSOR_PIXFORMAT_JPEG,
        .port = DVP_OUTPUT_8BITS,
        .bayer_type = CAM_SENSOR_BAYER_GBRG,
        .hdr_mode = CAM_SENSOR_HDR_LINEAR,
        .xclk = 20000000,
        .start_pos_x = 4,
        .start_pos_y = 4,
        .width = 1280,
        .height = 720,
        .regs = init_reglist_DVP_8bit_JPEG_1280x720_XCLK_20_12fps,
        .regs_size = ARRAY_SIZE(init_reglist_DVP_8bit_JPEG_1280x720_XCLK_20_12fps),
        .bpp = 8,
        .fps = 12,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
    {
        .index = OV2640_FORMAT_INDEX7,
        .name = "DVP_8bit_20Minput_JPEG_1600x1200_12fps",
        .format = CAM_SENSOR_PIXFORMAT_JPEG,
        .port = DVP_OUTPUT_8BITS,
        .bayer_type = CAM_SENSOR_BAYER_GBRG,
        .hdr_mode = CAM_SENSOR_HDR_LINEAR,
        .xclk = 20000000,
        .start_pos_x = 4,
        .start_pos_y = 4,
        .width = 1600,
        .height = 1200,
        .regs = init_reglist_DVP_8bit_JPEG_1600x1200_XCLK_20_12fps,
        .regs_size = ARRAY_SIZE(init_reglist_DVP_8bit_JPEG_1600x1200_XCLK_20_12fps),
        .bpp = 8,
        .fps = 12,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
};

static int ov2640_set_bank(uint8_t sccb_port, ov2640_bank_t bank)
{
    int res = 0;
    if (bank != reg_bank) {
        res = sccb_write_reg8_val8(sccb_port, OV2640_SCCB_ADDR, BANK_SEL, bank);
    }
    if (!res) {
        reg_bank = bank;
    }
    return res;
}

static int ov2640_write_array(uint8_t sccb_port, const ov2640_reginfo_t *regs, size_t regs_size)
{
    int i = 0, res = 0;
    while (!res && i < regs_size) {
        if (regs[i].reg == BANK_SEL) {
            res = ov2640_set_bank(sccb_port, regs[i].val);
        } else if (regs[i].reg == REG_DELAY) {
            delay_ms(regs[i].val);
        } else {
            res = sccb_write_reg8_val8(sccb_port, OV2640_SCCB_ADDR, regs[i].reg, regs[i].val);
        }
        i++;
    }
    ESP_LOGD(TAG, "write i=%d", i);
    return res;
}

static int ov2640_write_reg(uint8_t sccb_port, ov2640_bank_t bank, uint8_t reg, uint8_t value)
{
    int ret = ov2640_set_bank(sccb_port, bank);
    if (!ret) {
        ret = sccb_write_reg8_val8(sccb_port, OV2640_SCCB_ADDR, reg, value);
    }
    return ret;
}

static int ov2640_set_reg_bits(uint8_t sccb_port, uint8_t bank, uint8_t reg, uint8_t offset, uint8_t mask, uint8_t value)
{
    int ret = 0;
    uint8_t c_value, new_value;

    ret = ov2640_set_bank(sccb_port, bank);
    if (ret) {
        return ret;
    }
    c_value = sccb_read_reg8_val8(sccb_port, OV2640_SCCB_ADDR, reg);
    new_value = (c_value & ~(mask << offset)) | ((value & mask) << offset);
    ret = sccb_write_reg8_val8(sccb_port, OV2640_SCCB_ADDR, reg, new_value);
    return ret;
}

static int ov2640_read_reg(uint8_t sccb_port, ov2640_bank_t bank, uint8_t reg, uint8_t *read_buf)
{
    int value = -1;
    if (ov2640_set_bank(sccb_port, bank)) {
        return 0;
    }
    value = sccb_read_reg8_val8(sccb_port, OV2640_SCCB_ADDR, reg);
    if (value == -1) {
        ESP_LOGD(TAG, "Read err");
        return value;
    }
    *read_buf = value;
    return 0;
}

static int ov2640_write_reg_bits(uint8_t sccb_port, uint8_t bank, uint8_t reg, uint8_t mask, int enable)
{
    return ov2640_set_reg_bits(sccb_port, bank, reg, 0, mask, enable ? mask : 0);
}

#define WRITE_REG_OR_RETURN(bank, reg, val) ret = ov2640_write_reg(dev->sccb_port, bank, reg, val); if(ret){return ret;}
#define SET_REG_BITS_OR_RETURN(bank, reg, offset, mask, val) ret = ov2640_set_reg_bits(dev->sccb_port, bank, reg, offset, mask, val); if(ret){return ret;}

static int ov2640_set_test_pattern(esp_camera_device_t *dev, int enable)
{
    return ov2640_write_reg_bits(dev->sccb_port, BANK_SENSOR, COM7, COM7_COLOR_BAR, enable ? 1 : 0);
}

static int ov2640_hw_reset(esp_camera_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return 0;
}

static int ov2640_soft_reset(esp_camera_device_t *dev)
{
    int ret = ov2640_write_reg_bits(dev->sccb_port, BANK_SENSOR, COM7, COM7_SRST, 1);
    delay_ms(500);
    return ret;
}

static int ov2640_get_sensor_id(esp_camera_device_t *dev, sensor_id_t *id)
{
    int ret = -1;
    uint8_t PID = 0;
    if (ov2640_set_bank(dev->sccb_port, BANK_SENSOR) == 0) {
        ov2640_read_reg(dev->sccb_port, BANK_SENSOR, REG_PID, &PID);
    }
    if (OV2640_PID == PID) {
        id->PID = PID;
        ov2640_read_reg(dev->sccb_port, BANK_SENSOR, REG_VER, &id->VER);
        ov2640_read_reg(dev->sccb_port, BANK_SENSOR, REG_MIDL, &id->MIDL);
        ov2640_read_reg(dev->sccb_port, BANK_SENSOR, REG_MIDH, &id->MIDH);
        ret = 0;
    }
    return ret;
}

static int ov2640_set_stream(esp_camera_device_t *dev, int enable)
{
    int ret = -1;
    if (dev->pwdn_pin >= 0) {
        ret = gpio_set_level(dev->pwdn_pin, enable ? 0 : 1);
    } else {
        ret = ov2640_write_reg(dev->sccb_port, BANK_SENSOR, COM2, enable ? 0x02 : 0xe2);
        delay_ms(1000);
    }

    if (!ret) {
        dev->stream_status = enable;
    }
    ESP_LOGD(TAG, "Stream=%d", enable);
    return ret;
}

static int ov2640_set_hmirror(esp_camera_device_t *dev, int enable)
{
    return ov2640_write_reg_bits(dev->sccb_port, BANK_SENSOR, REG04, REG04_HFLIP_IMG, enable ? 1 : 0);
}

static int ov2640_set_vflip(esp_camera_device_t *dev, int enable)
{
    int ret = 0;
    ret = ov2640_write_reg_bits(dev->sccb_port, BANK_SENSOR, REG04, REG04_VREF_EN, enable ? 1 : 0);
    return ret & ov2640_write_reg_bits(dev->sccb_port, BANK_SENSOR, REG04, REG04_VFLIP_IMG, enable ? 1 : 0);
}

static int ov2640_set_jpeg_quality(esp_camera_device_t *dev, int quality)
{
    int ret = -1;
    struct ov2640_cam *cam_ov2640 = (struct ov2640_cam *)dev->priv;
    if (quality < 0) {
        quality = 0;
    } else if (quality > 63) {
        quality = 63;
    }
    ret = ov2640_write_reg(dev->sccb_port, BANK_DSP, QS, quality);
    if (ret == 0) {
        cam_ov2640->jpeg_quality = quality;
    }
    return ret;
}

static int ov2640_set_ae_level(esp_camera_device_t *dev, int level)
{
    int ret = 0;
    struct ov2640_cam *cam_ov2640 = (struct ov2640_cam *)dev->priv;
    level += 3;
    if (level <= 0 || level > NUM_AE_LEVELS) {
        return -1;
    }
    for (int i = 0; i < 3; i++) {
        WRITE_REG_OR_RETURN(BANK_SENSOR, ov2640_ae_levels_regs[0][i], ov2640_ae_levels_regs[level][i]);
    }
    cam_ov2640->ae_level = level - 3;
    return ret;
}

static int ov2640_set_contrast(esp_camera_device_t *dev, int level)
{
    int ret = 0;
    struct ov2640_cam *cam_ov2640 = (struct ov2640_cam *)dev->priv;
    level += 3;
    if (level <= 0 || level > NUM_CONTRAST_LEVELS) {
        return -1;
    }
    for (int i = 0; i < 7; i++) {
        WRITE_REG_OR_RETURN(BANK_DSP, ov2640_contrast_regs[0][i], ov2640_contrast_regs[level][i]);
    }
    cam_ov2640->contrast = level - 3;
    return ret;
}

static int ov2640_set_brightness(esp_camera_device_t *dev, int level)
{
    int ret = 0;
    struct ov2640_cam *cam_ov2640 = (struct ov2640_cam *)dev->priv;
    level += 3;
    if (level <= 0 || level > NUM_BRIGHTNESS_LEVELS) {
        return -1;
    }
    for (int i = 0; i < 5; i++) {
        WRITE_REG_OR_RETURN(BANK_DSP, ov2640_brightness_regs[0][i], ov2640_brightness_regs[level][i]);
    }
    cam_ov2640->brightness = level - 3;
    return ret;
}

static int ov2640_set_saturation(esp_camera_device_t *dev, int level)
{
    int ret = 0;
    struct ov2640_cam *cam_ov2640 = (struct ov2640_cam *)dev->priv;
    level += 3;
    if (level <= 0 || level > NUM_SATURATION_LEVELS) {
        return -1;
    }
    for (int i = 0; i < 5; i++) {
        WRITE_REG_OR_RETURN(BANK_DSP, ov2640_saturation_regs[0][i], ov2640_saturation_regs[level][i]);
    }
    cam_ov2640->saturation = level - 3;
    return ret;
}

static int ov2640_set_special_effect(esp_camera_device_t *dev, int effect)
{
    int ret = 0;
    struct ov2640_cam *cam_ov2640 = (struct ov2640_cam *)dev->priv;
    effect++;
    if (effect <= 0 || effect > NUM_SPECIAL_EFFECTS) {
        return -1;
    }
    for (int i = 0; i < 5; i++) {
        WRITE_REG_OR_RETURN(BANK_DSP, ov2640_special_effects_regs[0][i], ov2640_special_effects_regs[effect][i]);
    }
    cam_ov2640->special_effect = effect - 3;
    return ret;
}

static int ov2640_set_wb_mode(esp_camera_device_t *dev, int mode)
{
    int ret = 0;
    struct ov2640_cam *cam_ov2640 = (struct ov2640_cam *)dev->priv;
    if (mode < 0 || mode > NUM_WB_MODES) {
        return -1;
    }
    SET_REG_BITS_OR_RETURN(BANK_DSP, 0xC7, 6, 1, mode ? 1 : 0);
    if (mode) {
        for (int i = 0; i < 3; i++) {
            WRITE_REG_OR_RETURN(BANK_DSP, ov2640_wb_modes_regs[0][i], ov2640_wb_modes_regs[mode][i]);
        }
    }
    cam_ov2640->wb_mode = mode;
    return ret;
}

static int ov2640_query_para_desc(esp_camera_device_t *dev, struct v4l2_query_ext_ctrl *qctrl)
{
    esp_err_t ret = ESP_OK;

    switch (qctrl->id) {
    case CAM_SENSOR_JPEG_QUALITY:
        qctrl->type = V4L2_CTRL_TYPE_INTEGER;
        qctrl->minimum = 1;
        qctrl->maximum = 63;
        qctrl->step = 1;
        qctrl->elems = 1;
        qctrl->nr_of_dims = 0;
        qctrl->default_value = OV2640_JPEG_QUALITY_DEFAULT;
        break;
    case CAM_SENSOR_AE_LEVEL:
        qctrl->type = V4L2_CTRL_TYPE_INTEGER;
        qctrl->minimum = -2;
        qctrl->maximum = 1;
        qctrl->step = 1;
        qctrl->elems = 1;
        qctrl->nr_of_dims = 0;
        qctrl->default_value = 0;
        break;
    case CAM_SENSOR_CONTRAST:
        qctrl->type = V4L2_CTRL_TYPE_INTEGER;
        qctrl->minimum = -2;
        qctrl->maximum = 1;
        qctrl->step = 1;
        qctrl->elems = 1;
        qctrl->nr_of_dims = 0;
        qctrl->default_value = 0;
        break;
    case CAM_SENSOR_BRIGHTNESS:
        qctrl->type = V4L2_CTRL_TYPE_INTEGER;
        qctrl->minimum = -2;
        qctrl->maximum = 1;
        qctrl->step = 1;
        qctrl->elems = 1;
        qctrl->nr_of_dims = 0;
        qctrl->default_value = 0;
        break;
    case CAM_SENSOR_SATURATION:
        qctrl->type = V4L2_CTRL_TYPE_INTEGER;
        qctrl->minimum = -2;
        qctrl->maximum = 1;
        qctrl->step = 1;
        qctrl->elems = 1;
        qctrl->nr_of_dims = 0;
        qctrl->default_value = 0;
        break;
    case CAM_SENSOR_SPECIAL_EFFECT:
        qctrl->type = V4L2_CTRL_TYPE_INTEGER;
        qctrl->minimum = 0;
        qctrl->maximum = 6;
        qctrl->step = 1;
        qctrl->elems = 1;
        qctrl->nr_of_dims = 0;
        qctrl->default_value = 0;
        break;
    case CAM_SENSOR_WB:
        qctrl->type = V4L2_CTRL_TYPE_INTEGER;
        qctrl->minimum = 1;
        qctrl->maximum = 4;
        qctrl->step = 1;
        qctrl->elems = 1;
        qctrl->nr_of_dims = 0;
        qctrl->default_value = 0;
        break;
    default: {
        ESP_LOGE(TAG, "id=%"PRIx32" is not supported", qctrl->id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }

    return ret;
}

static int ov2640_get_para_value(esp_camera_device_t *dev, struct v4l2_ext_control *ctrl)
{
    esp_err_t ret = ESP_OK;
    struct ov2640_cam *cam_ov2640 = (struct ov2640_cam *)dev->priv;

    switch (ctrl->id) {
    case CAM_SENSOR_JPEG_QUALITY: {
        ctrl->value = cam_ov2640->jpeg_quality;
    }
    break;
    case CAM_SENSOR_AE_LEVEL: {
        ctrl->value = cam_ov2640->ae_level;
    }
    break;
    case CAM_SENSOR_CONTRAST: {
        ctrl->value = cam_ov2640->contrast;
    }
    break;
    case CAM_SENSOR_BRIGHTNESS: {
        ctrl->value = cam_ov2640->brightness;
    }
    break;
    case CAM_SENSOR_SATURATION: {
        ctrl->value = cam_ov2640->saturation;
    }
    break;
    case CAM_SENSOR_SPECIAL_EFFECT: {
        ctrl->value = cam_ov2640->special_effect;
    }
    break;
    case CAM_SENSOR_WB: {
        ctrl->value = cam_ov2640->wb_mode;
    }
    break;
    default: {
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    }
    return ret;
}

static int ov2640_set_para_value(esp_camera_device_t *dev, const struct v4l2_ext_control *ctrl)
{
    esp_err_t ret = ESP_OK;

    switch (ctrl->id) {
    case CAM_SENSOR_VFLIP: {
        ret = ov2640_set_vflip(dev, ctrl->value);
        break;
    }
    case CAM_SENSOR_HMIRROR: {
        ret = ov2640_set_hmirror(dev, ctrl->value);
        break;
    }
    case CAM_SENSOR_JPEG_QUALITY: {
        if (dev->cur_format->format == CAM_SENSOR_PIXFORMAT_JPEG) {
            ret = ov2640_set_jpeg_quality(dev, ctrl->value);
        }
        break;
    }
    case CAM_SENSOR_AE_LEVEL: {
        ret = ov2640_set_ae_level(dev, ctrl->value);
        break;
    }
    case CAM_SENSOR_CONTRAST: {
        ret = ov2640_set_contrast(dev, ctrl->value);
        break;
    }
    case CAM_SENSOR_BRIGHTNESS: {
        ret = ov2640_set_brightness(dev, ctrl->value);
        break;
    }
    case CAM_SENSOR_SATURATION: {
        ret = ov2640_set_saturation(dev, ctrl->value);
        break;
    }
    case CAM_SENSOR_SPECIAL_EFFECT: {
        ret = ov2640_set_special_effect(dev, ctrl->value);
        break;
    }
    case CAM_SENSOR_WB: {
        ret = ov2640_set_wb_mode(dev, ctrl->value);
        break;
    }
    default: {
        ESP_LOGE(TAG, "set id=%"PRIx32" is not supported", ctrl->id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }

    return ret;
}

static int ov2640_query_support_formats(esp_camera_device_t *dev, sensor_format_array_info_t *formats)
{
    formats->count = ARRAY_SIZE(ov2640_format_info);
    formats->format_array = &ov2640_format_info[0];
    ESP_LOGI(TAG, "f_array=%p", formats->format_array);
    return 0;
}

static int ov2640_query_support_capability(esp_camera_device_t *dev, sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);
    sensor_cap->fmt_yuv = 1;
    sensor_cap->fmt_rgb565 = 1;
    sensor_cap->fmt_jpeg = 1;
    return 0;
}

static int ov2640_set_format(esp_camera_device_t *dev, const sensor_format_t *format)
{
    int ret = 0;
    // write common reg list
    ret = ov2640_write_array(dev->sccb_port, ov2640_settings_cif, ARRAY_SIZE(ov2640_settings_cif));
    // write format related regs
    ret |= ov2640_write_array(dev->sccb_port, (ov2640_reginfo_t *)format->regs, format->regs_size);

    if (ret < 0) {
        ESP_LOGE(TAG, "Set format regs fail");
        return ESP_CAM_SENSOR_FAILED_TO_S_FORMAT;
    }

    dev->cur_format = &ov2640_format_info[format->index];

    return ret;
}

static int ov2640_get_format(esp_camera_device_t *dev, sensor_format_t *format)
{
    int ret = -1;

    if (dev->cur_format != NULL) {
        memcpy(format, dev->cur_format, sizeof(sensor_format_t));
        ret = 0;
    }
    return ret;
}

static int ov2640_priv_ioctl(esp_camera_device_t *dev, unsigned int cmd, void *arg)
{
    int ret = 0;
    struct sensor_reg_val *sensor_reg;
    OV2640_IO_MUX_LOCK();

    if (cmd & (_IOC_WRITE << _IOC_DIRSHIFT)) {
        switch (cmd) {
        case CAM_SENSOR_IOC_HW_RESET:
            ret = ov2640_hw_reset(dev);
            break;
        case CAM_SENSOR_IOC_SW_RESET:
            ret = ov2640_soft_reset(dev);
            break;
        case CAM_SENSOR_IOC_S_REG:
            sensor_reg = (struct sensor_reg_val *)arg;
            ret = sccb_write_reg8_val8(dev->sccb_port, OV2640_SCCB_ADDR, sensor_reg->regaddr, sensor_reg->value);
            break;
        case CAM_SENSOR_IOC_S_STREAM:
            ret = ov2640_set_stream(dev, *(int *)arg);
            break;
        case CAM_SENSOR_IOC_S_TEST_PATTERN:
            ret = ov2640_set_test_pattern(dev, *(int *)arg);
            break;
        default:
            ESP_LOGE(TAG, "cmd=%"PRIx16" is not supported", cmd);
            ret = ESP_ERR_INVALID_ARG;
            break;
        }
    } else {
        switch (cmd) {
        case CAM_SENSOR_IOC_G_REG:
            sensor_reg = (struct sensor_reg_val *)arg;
            sensor_reg->value = sccb_read_reg8_val8(dev->sccb_port, OV2640_SCCB_ADDR, sensor_reg->regaddr);
            break;
        case CAM_SENSOR_IOC_G_CHIP_ID:
            ret = ov2640_get_sensor_id(dev, arg);
            break;
        default:
            ESP_LOGE(TAG, "cmd=%"PRIx16" is not supported", cmd);
            ret = ESP_ERR_INVALID_ARG;
            break;
        }
    }
    OV2640_IO_MUX_UNLOCK();
    return ret;
}

static int ov2640_power_on(esp_camera_device_t *dev)
{
    int ret = 0;

    if (dev->xclk_pin >= 0) {
        CAMERA_ENABLE_OUT_CLOCK(dev->xclk_pin, dev->xclk_freq_hz);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->pwdn_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        gpio_config(&conf);

        // carefull, logic is inverted compared to reset pin
        gpio_set_level(dev->pwdn_pin, 1);
        delay_ms(10);
        gpio_set_level(dev->pwdn_pin, 0);
        delay_ms(10);
    }

    if (dev->reset_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->reset_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        gpio_config(&conf);

        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }

    return ret;
}

static int ov2640_power_off(esp_camera_device_t *dev)
{
    int ret = 0;

    if (dev->xclk_pin >= 0) {
        CAMERA_DISABLE_OUT_CLOCK(dev->xclk_pin);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_set_level(dev->pwdn_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->pwdn_pin, 1);
        delay_ms(10);
    }

    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
    }

    return ret;
}

static esp_camera_ops_t ov2640_ops = {
    .query_para_desc = ov2640_query_para_desc,
    .get_para_value = ov2640_get_para_value,
    .set_para_value = ov2640_set_para_value,
    .query_support_formats = ov2640_query_support_formats,
    .query_support_capability = ov2640_query_support_capability,
    .set_format = ov2640_set_format,
    .get_format = ov2640_get_format,
    .priv_ioctl = ov2640_priv_ioctl
};

esp_camera_device_t *ov2640_dvp_detect(const esp_camera_driver_config_t *config)
{
    esp_camera_device_t *dev = NULL;
    struct ov2640_cam *cam_ov2640;

    if (config == NULL) {
        return NULL;
    }

    if (s_ov2640_index >= OV2640_SUPPORT_NUM) {
        ESP_LOGE(TAG, "Only support max %d cameras", OV2640_SUPPORT_NUM);
        return NULL;
    }

    s_ov2640[s_ov2640_index] = calloc(sizeof(esp_camera_device_t), 1);
    if (s_ov2640[s_ov2640_index] == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    cam_ov2640 = calloc(sizeof(struct ov2640_cam), 1);
    if (!cam_ov2640) {
        ESP_LOGE(TAG, "failed to call calloc cam");
        free(s_ov2640[s_ov2640_index]);
        return NULL;
    }

    dev = s_ov2640[s_ov2640_index];
    dev->name = (char *)OV2640_SENSOR_NAME;
    dev->ops = &ov2640_ops;
    dev->sccb_port = config->sccb_port;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->priv = cam_ov2640;

    // Configure sensor power, clock, and SCCB port
    if (ov2640_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (ov2640_get_sensor_id(dev, &dev->id) == -1) {
        ESP_LOGE(TAG, "Camera get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.PID != OV2640_PID) {
        ESP_LOGE(TAG, "Camera sensor is not OV2640, PID=0x%x", dev->id.PID);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x with index %d", dev->id.PID, s_ov2640_index);

    s_ov2640_index++;

    return dev;

err_free_handler:
    ov2640_power_off(dev);
    free(dev->priv);
    free(dev);
    return NULL;
}

#if CONFIG_CAMERA_OV2640_AUTO_DETECT
ESP_CAMERA_DETECT_FN(ov2640_dvp_detect, CAMERA_INTF_DVP)
{
    return ov2640_dvp_detect(config);
}
#endif
