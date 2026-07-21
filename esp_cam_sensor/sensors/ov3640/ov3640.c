/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/timers.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"
#include "ov3640.h"
#include "ov3640_settings.h"

typedef struct {
    uint32_t ae_target_level;
} ov3640_para_t;

struct ov3640_cam {
    ov3640_para_t ov3640_para;
};

#define OV3640_IO_MUX_LOCK(mux)
#define OV3640_IO_MUX_UNLOCK(mux)
#define OV3640_ENABLE_OUT_XCLK(pin,clk)
#define OV3640_DISABLE_OUT_XCLK(pin)

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))
#define OV3640_SUPPORT_NUM CONFIG_CAMERA_OV3640_MAX_SUPPORT
#define OV3640_AE_LEVEL_DEFAULT (-2)
#define OV3640_XCLK_DEFAULT     (24*1000*1000) // todo: confirm default xclk matches board wiring (format uses 24MHz)
#define OV3640_STREAM_ON        (0x00)
#define OV3640_STREAM_OFF       (0x03)

static const char *TAG = "ov3640";

#ifndef CONFIG_CAMERA_OV3640_DVP_IF_FORMAT_INDEX_DEFAULT
#error "Please choose at least one format in menuconfig for OV3640"
#endif

static const uint8_t ov3640_format_default_index = CONFIG_CAMERA_OV3640_DVP_IF_FORMAT_INDEX_DEFAULT;

static const uint8_t ov3640_format_index[] = {
#if CONFIG_CAMERA_OV3640_DVP_RGB565_LE_640X480_7FPS
    0,
#endif
#if CONFIG_CAMERA_OV3640_DVP_YUV422_UYVY_640X480_30FPS
    1,
#endif
#if CONFIG_CAMERA_OV3640_DVP_YUV422_UYVY_1024X768_5FPS
    2,
#endif
#if CONFIG_CAMERA_OV3640_DVP_YUV422_UYVY_1024X768_25FPS
    3,
#endif
#if CONFIG_CAMERA_OV3640_DVP_YUV422_UYVY_1280X720_15FPS
    4,
#endif
#if CONFIG_CAMERA_OV3640_DVP_JPEG_2048X1536_15FPS
    5,
#endif
#if CONFIG_CAMERA_OV3640_DVP_JPEG_1024X768_25FPS
    6,
#endif
#if CONFIG_CAMERA_OV3640_DVP_JPEG_1280X720_15FPS
    7,
#endif
};

static const esp_cam_sensor_format_t ov3640_format_info[] = {
#if CONFIG_CAMERA_OV3640_DVP_RGB565_LE_640X480_7FPS
    {
        .name = "DVP_8bit_24Minput_RGB565_LE_640x480_7fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RGB565_LE,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 24000000,
        .width = 640,
        .height = 480,
        .regs = ov3640_dvp_8bit_24Minput_640x480_rgb565_le_7fps,
        .regs_size = ARRAY_SIZE(ov3640_dvp_8bit_24Minput_640x480_rgb565_le_7fps),
        .fps = 7,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
#endif
#if CONFIG_CAMERA_OV3640_DVP_YUV422_UYVY_640X480_30FPS
    {
        .name = "DVP_8bit_24Minput_YUV422_UYVY_640x480_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 24000000,
        .width = 640,
        .height = 480,
        .regs = ov3640_dvp_8bit_24Minput_640x480_yuv422_uyvy_30fps,
        .regs_size = ARRAY_SIZE(ov3640_dvp_8bit_24Minput_640x480_yuv422_uyvy_30fps),
        .fps = 30,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
#endif
#if CONFIG_CAMERA_OV3640_DVP_YUV422_UYVY_1024X768_5FPS
    {
        .name = "DVP_8bit_24Minput_YUV422_UYVY_1024x768_5fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 24000000,
        .width = 1024,
        .height = 768,
        .regs = ov3640_dvp_8bit_24Minput_1024x768_yuv422_uyvy_5fps,
        .regs_size = ARRAY_SIZE(ov3640_dvp_8bit_24Minput_1024x768_yuv422_uyvy_5fps),
        .fps = 5, // todo: register 0x3011 comment says 7.5fps, filename says 5fps
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
#endif
#if CONFIG_CAMERA_OV3640_DVP_YUV422_UYVY_1024X768_25FPS
    {
        .name = "DVP_8bit_24Minput_YUV422_UYVY_1024x768_25fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 24000000,
        .width = 1024,
        .height = 768,
        .regs = ov3640_dvp_8bit_24Minput_1024x768_yuv422_uyvy_25fps,
        .regs_size = ARRAY_SIZE(ov3640_dvp_8bit_24Minput_1024x768_yuv422_uyvy_25fps),
        .fps = 25,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
#endif
#if CONFIG_CAMERA_OV3640_DVP_YUV422_UYVY_1280X720_15FPS
    {
        .name = "DVP_8bit_24Minput_YUV422_UYVY_1280x720_15fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 24000000,
        .width = 1280,
        .height = 720,
        .regs = ov3640_dvp_8bit_24Minput_1280x720_yuv422_uyvy_15fps,
        .regs_size = ARRAY_SIZE(ov3640_dvp_8bit_24Minput_1280x720_yuv422_uyvy_15fps),
        .fps = 15,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
#endif
#if CONFIG_CAMERA_OV3640_DVP_JPEG_2048X1536_15FPS
    {
        .name = "DVP_8bit_24Minput_JPEG_2048x1536_15fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_JPEG,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 24000000,
        .width = 2048,
        .height = 1536,
        .regs = ov3640_dvp_8bit_24Minput_2048x1536_jpeg_15fps,
        .regs_size = ARRAY_SIZE(ov3640_dvp_8bit_24Minput_2048x1536_jpeg_15fps),
        .fps = 15,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
#endif
#if CONFIG_CAMERA_OV3640_DVP_JPEG_1024X768_25FPS
    {
        .name = "DVP_8bit_24Minput_JPEG_1024x768_25fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_JPEG,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 24000000,
        .width = 1024,
        .height = 768,
        .regs = ov3640_dvp_8bit_24Minput_1024x768_jpeg_25fps,
        .regs_size = ARRAY_SIZE(ov3640_dvp_8bit_24Minput_1024x768_jpeg_25fps),
        .fps = 25,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
#endif
#if CONFIG_CAMERA_OV3640_DVP_JPEG_1280X720_15FPS
    {
        .name = "DVP_8bit_24Minput_JPEG_1280x720_15fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_JPEG,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 24000000,
        .width = 1280,
        .height = 720,
        .regs = ov3640_dvp_8bit_24Minput_1280x720_jpeg_15fps,
        .regs_size = ARRAY_SIZE(ov3640_dvp_8bit_24Minput_1280x720_jpeg_15fps),
        .fps = 15,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
#endif
};

static uint8_t get_ov3640_actual_format_index(void)
{
    for (int i = 0; i < ARRAY_SIZE(ov3640_format_index); i++) {
        if (ov3640_format_index[i] == ov3640_format_default_index) {
            return i;
        }
    }

    return 0;
}

static esp_err_t ov3640_read(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a16v8(sccb_handle, reg, read_buf);
}

static esp_err_t ov3640_write(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t data)
{
    return esp_sccb_transmit_reg_a16v8(sccb_handle, reg, data);
}

/* write a array of registers  */
static esp_err_t ov3640_write_array(esp_sccb_io_handle_t sccb_handle, const ov3640_reginfo_t *regarray)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && regarray[i].reg != OV3640_REGLIST_TAIL) {
        if (regarray[i].reg != OV3640_REG_DLY) {
            ret = ov3640_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    ESP_LOGD(TAG, "Set array done[i=%d]", i);
    return ret;
}

static esp_err_t ov3640_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_data = 0;

    ret = ov3640_read(sccb_handle, reg, &reg_data);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (reg_data & ~mask) | ((value << offset) & mask);
    ret = ov3640_write(sccb_handle, reg, value);
    return ret;
}

#define WRITE_REG_OR_RETURN(reg, val) ret = ov3640_write(dev->sccb_handle, reg, val); if(ret){return ret;}

static esp_err_t ov3640_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    return ov3640_set_reg_bits(dev->sccb_handle, 0x3080, 7, 1, enable ? 0x01 : 0x00);
}

static esp_err_t ov3640_set_saturation(esp_cam_sensor_device_t *dev, int level)
{
    esp_err_t ret = ESP_OK;
    if ((level >= -48) && (level <= 48)) {
        WRITE_REG_OR_RETURN(0x3302, 0xef);
        WRITE_REG_OR_RETURN(0x3355, 0x02);
        WRITE_REG_OR_RETURN(0x3358, 0x30 + level);
        WRITE_REG_OR_RETURN(0x3359, 0x30 + level);
    } else {
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}

static esp_err_t ov3640_set_contrast(esp_cam_sensor_device_t *dev, int level)
{
    esp_err_t ret = ESP_OK;
    if ((level >= -12) && (level <= 12)) {
        WRITE_REG_OR_RETURN(0x3302, 0xef);
        WRITE_REG_OR_RETURN(0x3355, 0x04);
        WRITE_REG_OR_RETURN(0x3354, 0x01);
        WRITE_REG_OR_RETURN(0x335c, 0x20 + level);
        WRITE_REG_OR_RETURN(0x335d, 0x20 + level);
    } else {
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}

static esp_err_t ov3640_set_sharpness(esp_cam_sensor_device_t *dev, int level)
{
    esp_err_t ret = ESP_OK;
    if ((level >= -3) && (level <= 3)) {
        WRITE_REG_OR_RETURN(0x332d, 0x45 + level);
        WRITE_REG_OR_RETURN(0x332f, 0x03);
    } else {
        WRITE_REG_OR_RETURN(0x332d, 0x60);
        WRITE_REG_OR_RETURN(0x332f, 0x03);
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}

static esp_err_t ov3640_set_brightness(esp_cam_sensor_device_t *dev, int level)
{
    esp_err_t ret = ESP_OK;
    if (level <= -128 || level > 128) {
        return ESP_ERR_INVALID_ARG;
    }
    if (level >= 0) {
        WRITE_REG_OR_RETURN(0x335e, 0xFF & level);
        WRITE_REG_OR_RETURN(0x3355, 0x04);
        WRITE_REG_OR_RETURN(0x3354, 0x01);
    } else {
        WRITE_REG_OR_RETURN(0x335e, 0xFF & -level);
        WRITE_REG_OR_RETURN(0x3355, 0x04);
        WRITE_REG_OR_RETURN(0x3354, 0x08);
    }

    return ret;
}

static esp_err_t ov3640_set_special_effect(esp_cam_sensor_device_t *dev, int effect)
{
    esp_err_t ret = ESP_OK;
    if (effect < 0 || effect >= NUM_COLORS) {
        return ESP_ERR_INVALID_ARG;
    }
    ret = ov3640_write_array(dev->sccb_handle, ov3640_colors[effect]);
    if (ret != ESP_OK) {
        return ret;
    }
    ESP_LOGD(TAG, "Set special effect to: %d", effect);
    return ret;
}

static esp_err_t ov3640_set_ae_level(esp_cam_sensor_device_t *dev, int level)
{
    esp_err_t ret = ESP_OK;
    struct ov3640_cam *cam_ov3640 = (struct ov3640_cam *)dev->priv;
    if (level < -5 || level > 5) {
        return ESP_ERR_INVALID_ARG;
    }
    ret = ov3640_write_array(dev->sccb_handle, ov3640_ae_levels[level + 5]);
    if (ret != ESP_OK) {
        return ret;
    }
    cam_ov3640->ov3640_para.ae_target_level = level;
    ESP_LOGD(TAG, "Set ae_level to: %d", level);
    return ret;
}

static esp_err_t ov3640_hw_reset(esp_cam_sensor_device_t *dev)
{
    return ESP_OK;
}

static esp_err_t ov3640_soft_reset(esp_cam_sensor_device_t *dev)
{
    return ESP_OK;
}

static esp_err_t ov3640_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t pid_h, pid_l;

    ret = ov3640_read(dev->sccb_handle, OV3640_REG_CHIP_ID_H, &pid_h);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = ov3640_read(dev->sccb_handle, OV3640_REG_CHIP_ID_L, &pid_l);
    if (ret != ESP_OK) {
        return ret;
    }
    id->pid = (pid_h << 8) | pid_l;

    return ret;
}

static esp_err_t ov3640_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t stream_status;
    bool stream_en = enable ? true : false;
    if (ov3640_read(dev->sccb_handle, 0x3086, &stream_status) != ESP_OK) {
        ESP_LOGE(TAG, "Check stream failed");
        return ret;
    }

    if (stream_en && (stream_status == OV3640_STREAM_ON)) {
        return ESP_OK;
    }

    if (!stream_en && (stream_status == OV3640_STREAM_OFF)) {
        return ESP_OK;
    }

    ret = ov3640_write(dev->sccb_handle, 0x3086, enable ? OV3640_STREAM_ON : OV3640_STREAM_OFF);

    if (ret == ESP_OK) {
        dev->stream_status = enable;
    }
    ESP_LOGD(TAG, "Stream=%d", enable);
    return ret;
}

static esp_err_t ov3640_set_mirror(esp_cam_sensor_device_t *dev, int enable)
{
    return ov3640_set_reg_bits(dev->sccb_handle, 0x307c, 1, 1, enable ? 0x01 : 0x00);
}

static esp_err_t ov3640_set_vflip(esp_cam_sensor_device_t *dev, int enable)
{
    return ov3640_set_reg_bits(dev->sccb_handle, 0x307c, 0, 1, enable ? 0x01 : 0x00);
}

static esp_err_t ov3640_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    esp_err_t ret = ESP_OK;
    switch (qdesc->id) {
    case ESP_CAM_SENSOR_VFLIP:
    case ESP_CAM_SENSOR_HMIRROR:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = 0;
        qdesc->number.maximum = 1;
        qdesc->number.step = 1;
        qdesc->default_value = 0;
        break;
    case ESP_CAM_SENSOR_AE_LEVEL:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = -5;
        qdesc->number.maximum = 5;
        qdesc->number.step = 1;
        qdesc->default_value = OV3640_AE_LEVEL_DEFAULT;
        break;
    case ESP_CAM_SENSOR_CONTRAST:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = -12;
        qdesc->number.maximum = 12;
        qdesc->number.step = 1;
        qdesc->default_value = 0;
        break;
    case ESP_CAM_SENSOR_SATURATION:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = -48;
        qdesc->number.maximum = 48;
        qdesc->number.step = 1;
        qdesc->default_value = 0;
        break;
    case ESP_CAM_SENSOR_SPECIAL_EFFECT:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = 0;
        qdesc->number.maximum = NUM_COLORS - 1;
        qdesc->number.step = 1;
        qdesc->default_value = 0;
        break;
    case ESP_CAM_SENSOR_SHARPNESS:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = -3;
        qdesc->number.maximum = 3;
        qdesc->number.step = 1;
        qdesc->default_value = 0;
        break;
    default: {
        ESP_LOGD(TAG, "id=%"PRIx32" is not supported", qdesc->id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }
    return ret;
}

static esp_err_t ov3640_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    struct ov3640_cam *cam_ov3640 = (struct ov3640_cam *)dev->priv;

    switch (id) {
    case ESP_CAM_SENSOR_AE_LEVEL: {
        ESP_RETURN_ON_FALSE(size == 4, ESP_ERR_INVALID_ARG, TAG, "Para size err");
        *(uint32_t *)arg = cam_ov3640->ov3640_para.ae_target_level;
    }
    break;
    default: {
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    }
    return ret;
}

static esp_err_t ov3640_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    // The parameter types of this sensor are all 4 bytes.
    ESP_RETURN_ON_FALSE(size == 4, ESP_ERR_INVALID_ARG, TAG, "Para size err");
    int value = *(int *)arg;
    esp_err_t ret = ESP_OK;

    switch (id) {
    case ESP_CAM_SENSOR_VFLIP: {
        ret = ov3640_set_vflip(dev, value);
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        ret = ov3640_set_mirror(dev, value);
        break;
    }
    case ESP_CAM_SENSOR_AE_LEVEL: {
        ret = ov3640_set_ae_level(dev, value);
        break;
    }
    case ESP_CAM_SENSOR_CONTRAST: {
        ret = ov3640_set_contrast(dev, value);
        break;
    }
    case ESP_CAM_SENSOR_SATURATION: {
        ret = ov3640_set_saturation(dev, value);
        break;
    }
    case ESP_CAM_SENSOR_SPECIAL_EFFECT: {
        ret = ov3640_set_special_effect(dev, value);
        break;
    }
    case ESP_CAM_SENSOR_SHARPNESS: {
        ret = ov3640_set_sharpness(dev, value);
        break;
    }
    case ESP_CAM_SENSOR_BRIGHTNESS: {
        ret = ov3640_set_brightness(dev, value);
        break;
    }
    default: {
        ESP_LOGE(TAG, "set id=%"PRIx32" is not supported", id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }

    return ret;
}

static esp_err_t ov3640_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);

    formats->count = ARRAY_SIZE(ov3640_format_info);
    formats->format_array = &ov3640_format_info[0];
    return ESP_OK;
}

static esp_err_t ov3640_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_yuv = 1;
    sensor_cap->fmt_rgb565 = 1;
    return 0;
}

static esp_err_t ov3640_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        format = &ov3640_format_info[get_ov3640_actual_format_index()];
    }

    ret = ov3640_write_array(dev->sccb_handle, (ov3640_reginfo_t *)format->regs);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "Set format regs failed");
    delay_ms(10);

    ESP_LOGD(TAG, "Set fmt done %s", format->name);

    dev->cur_format = format;

    return ret;
}

static esp_err_t ov3640_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
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

static esp_err_t ov3640_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    OV3640_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = ov3640_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = ov3640_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = ov3640_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);
        ret = ov3640_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);
        ret = ov3640_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = ov3640_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);
        ret = ov3640_get_sensor_id(dev, arg);
        break;
    default:
        break;
    }

    OV3640_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t ov3640_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        OV3640_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
        delay_ms(20);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->pwdn_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        gpio_config(&conf);

        // carefully, logic is inverted compared to reset pin
        gpio_set_level(dev->pwdn_pin, 0);
        delay_ms(10);
    }

    if (dev->reset_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->reset_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        gpio_config(&conf);

        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }

    return ret;
}

static esp_err_t ov3640_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        OV3640_DISABLE_OUT_XCLK(dev->xclk_pin);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_set_level(dev->pwdn_pin, 1);
        delay_ms(10);
    }

    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
    }

    return ret;
}

static esp_err_t ov3640_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del ov3640 (%p)", dev);
    if (dev) {
        if (dev->priv) {
            free(dev->priv);
            dev->priv = NULL;
        }
        free(dev);
        dev = NULL;
    }

    return ESP_OK;
}

static const esp_cam_sensor_ops_t ov3640_ops = {
    .query_para_desc = ov3640_query_para_desc,
    .get_para_value = ov3640_get_para_value,
    .set_para_value = ov3640_set_para_value,
    .query_support_formats = ov3640_query_support_formats,
    .query_support_capability = ov3640_query_support_capability,
    .set_format = ov3640_set_format,
    .get_format = ov3640_get_format,
    .priv_ioctl = ov3640_priv_ioctl,
    .del = ov3640_delete
};

esp_cam_sensor_device_t *ov3640_detect(esp_cam_sensor_config_t *config)
{
    esp_cam_sensor_device_t *dev = NULL;
    struct ov3640_cam *cam_ov3640;

    if (config == NULL) {
        return NULL;
    }

    dev = calloc(1, sizeof(esp_cam_sensor_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    cam_ov3640 = heap_caps_calloc(1, sizeof(struct ov3640_cam), MALLOC_CAP_DEFAULT);
    if (!cam_ov3640) {
        ESP_LOGE(TAG, "failed to calloc cam");
        free(dev);
        return NULL;
    }
    memset(cam_ov3640, 0x0, sizeof(struct ov3640_cam));

    dev->name = (char *)OV3640_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &ov3640_ops;
    dev->priv = cam_ov3640;

    dev->cur_format = &ov3640_format_info[get_ov3640_actual_format_index()];

    // Configure sensor power, clock, and SCCB port
    if (ov3640_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (ov3640_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if ((dev->id.pid != OV3640_PID1) && (dev->id.pid != OV3640_PID2)) {
        ESP_LOGW(TAG, "Camera sensor is not OV3640, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    ov3640_power_off(dev);
    free(dev->priv);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_OV3640_AUTO_DETECT_DVP_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(ov3640_detect, ESP_CAM_SENSOR_DVP, OV3640_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_DVP;
    return ov3640_detect(config);
}
#endif
