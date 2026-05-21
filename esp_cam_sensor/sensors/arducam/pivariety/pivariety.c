/*
 * SPDX-FileCopyrightText: 2026 Arducam Electronic Technology (Nanjing) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/stat.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"
#include "pivariety_settings.h"
#include "pivariety_v4l2_cid.h"
#include "pivariety.h"

typedef struct {
    uint32_t exposure_val;
    uint32_t exposure_max;
    uint32_t gain_index; // current gain index
    size_t limited_gain_index;
} pivariety_para_t;

struct pivariety_cam {
    pivariety_para_t pivariety_para;
};

#define PIVARIETY_IO_MUX_LOCK(mux)
#define PIVARIETY_IO_MUX_UNLOCK(mux)
#define PIVARIETY_ENABLE_OUT_XCLK(pin,clk)
#define PIVARIETY_DISABLE_OUT_XCLK(pin)

#define EXPOSURE_V4L2_UNIT_US                   100
#define EXPOSURE_V4L2_TO_PIVARIETY(v, sf)          \
    ((uint32_t)(((double)v) * (sf)->fps * (sf)->isp_info->isp_v1_info.vts / (1000000 / EXPOSURE_V4L2_UNIT_US) + 0.5))
#define EXPOSURE_PIVARIETY_TO_V4L2(v, sf)          \
    ((int32_t)(((double)v) * 1000000 / (sf)->fps / (sf)->isp_info->isp_v1_info.vts / EXPOSURE_V4L2_UNIT_US + 0.5))

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))


static const uint8_t s_pivariety_exp_min = 0x02;
static esp_cam_sensor_format_t *pivariety_format_info;
static size_t pivariety_format_info_size;
static esp_cam_sensor_isp_info_t *pivariety_isp_info;
static uint32_t *pivariety_gain_map;
static const char *TAG = "pivariety";


static esp_err_t pivariety_read(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint32_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a16v32(sccb_handle, reg, read_buf);
}

static esp_err_t pivariety_write(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint32_t data)
{
    return esp_sccb_transmit_reg_a16v32(sccb_handle, reg, data);
}


static esp_err_t pivariety_wait_for_free(esp_sccb_io_handle_t sccb_handle,  int interval)
{
    uint32_t value;
    uint32_t count = 0;
    esp_err_t ret = ESP_OK;
    while (count++ < (1000 / interval)) {
        ret = pivariety_read(sccb_handle, PIVARIETY_REG_SYSTEM_IDLE, &value);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "read system idle failed");
        if (!value) {
            break;
        }
        delay_ms(interval);
    }

    return ret;
}

/* write a array of registers  */
static esp_err_t pivariety_write_array(esp_sccb_io_handle_t sccb_handle, pivariety_reginfo_t *regarray, size_t regs_size)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && i < (int)regs_size && regarray[i].reg != PIVARIETY_REG_END) {
        if (regarray[i].reg != PIVARIETY_REG_DELAY) {
            ret = pivariety_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    if (i >= (int)regs_size && regarray[i - 1].reg != PIVARIETY_REG_END) {
        ESP_LOGW(TAG, "write_array reached regs_size without PIVARIETY_REG_END; possible malformed regs");
    }
    return ret;
}

static esp_err_t pivariety_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return ESP_OK;
}

static esp_err_t pivariety_soft_reset(esp_cam_sensor_device_t *dev)
{
    return ESP_OK;
}

static esp_err_t pivariety_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    esp_err_t ret = ESP_FAIL;
    uint32_t pid;

    ret = pivariety_read(dev->sccb_handle, PIVARIETY_REG_DEVICE_ID, &pid);
    if (ret != ESP_OK) {
        return ret;
    }
    id->pid = pid ;

    return ret;
}

static esp_err_t pivariety_set_exp_val(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    esp_err_t ret;
    struct pivariety_cam *cam_pivariety = (struct pivariety_cam *)dev->priv;
    uint32_t value_buf = MAX(u32_val, s_pivariety_exp_min);
    value_buf = MIN(value_buf, cam_pivariety->pivariety_para.exposure_max);

    ESP_LOGD(TAG, "set exposure 0x%" PRIx32, value_buf);
    // According to reference code: 0x3502 is low 8 bits, 0x3501 is high 8 bits
    ret = pivariety_write(dev->sccb_handle,
                          PIVARIETY_REG_CTRL_ID,
                          V4L2_CID_EXPOSURE);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "ctrl id reg  write failed");
    ret = pivariety_wait_for_free(dev->sccb_handle, 5);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "pivariety_wait_for_free error");
    ret = pivariety_write(dev->sccb_handle,
                          PIVARIETY_REG_CTRL_VALUE,
                          value_buf);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "ctrl value write failed");

    cam_pivariety->pivariety_para.exposure_val = value_buf;
    return ret;
}


static esp_err_t pivariety_set_gain_val(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    esp_err_t ret;
    struct pivariety_cam *cam_pivariety = (struct pivariety_cam *)dev->priv;
    // Limit gain index to valid range

    if (cam_pivariety->pivariety_para.limited_gain_index == 0 || pivariety_gain_map == NULL) {
        ESP_LOGE(TAG, "gain map is empty or not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Limit gain index to valid range
    if (u32_val >= cam_pivariety->pivariety_para.limited_gain_index) {
        u32_val = cam_pivariety->pivariety_para.limited_gain_index - 1;
    }

    ESP_LOGD(TAG, "set gain index %" PRIu32 ", gain=0x%04" PRIx32,
             u32_val, pivariety_gain_map[u32_val]);

    ret = pivariety_write(dev->sccb_handle,
                          PIVARIETY_REG_CTRL_ID,
                          V4L2_CID_ANALOGUE_GAIN);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "ctrl id reg  write failed");
    ret = pivariety_wait_for_free(dev->sccb_handle, 5);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "pivariety_wait_for_free error");
    ret = pivariety_write(dev->sccb_handle,
                          PIVARIETY_REG_CTRL_VALUE,
                          pivariety_gain_map[u32_val]);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "ctrl value write failed");

    cam_pivariety->pivariety_para.gain_index = u32_val;
    return ret;
}




static esp_err_t pivariety_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;
    ret = pivariety_write(dev->sccb_handle, PIVARIETY_REG_STREAM_ON, enable ? 0x01 : 0x00);
    if (ret == ESP_OK) {
        dev->stream_status = enable;
        ESP_LOGD(TAG, "Stream=%d", enable);
    }
    return ret;
}

static esp_err_t pivariety_set_mirror(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_OK;
    ret = pivariety_write(dev->sccb_handle, PIVARIETY_REG_CTRL_ID, V4L2_CID_HFLIP);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "hflip control id reg write failed");
    return pivariety_write(dev->sccb_handle, PIVARIETY_REG_CTRL_VALUE, enable ? 0x01 : 0x00);
}

static esp_err_t pivariety_set_vflip(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_OK;
    ret = pivariety_write(dev->sccb_handle, PIVARIETY_REG_CTRL_ID, V4L2_CID_VFLIP);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "vflip control id reg write failed");
    return pivariety_write(dev->sccb_handle, PIVARIETY_REG_CTRL_VALUE, enable ? 0x01 : 0x00);
}

static esp_err_t pivariety_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    esp_err_t ret = ESP_OK;
    struct pivariety_cam *cam_pivariety = (struct pivariety_cam *)dev->priv;
    switch (qdesc->id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = 0x02;
        qdesc->number.maximum = dev->cur_format->isp_info->isp_v1_info.vts - 6;
        qdesc->number.step = 1;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.exp_def;
        break;
    case ESP_CAM_SENSOR_EXPOSURE_US:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = EXPOSURE_PIVARIETY_TO_V4L2(2, dev->cur_format);
        qdesc->number.maximum = EXPOSURE_PIVARIETY_TO_V4L2((dev->cur_format->isp_info->isp_v1_info.vts - 6), dev->cur_format); // max = VTS-6 = height+vblank-6, so when update vblank, exposure_max must be updated
        qdesc->number.step = MAX(EXPOSURE_PIVARIETY_TO_V4L2(0x01, dev->cur_format), 1);
        qdesc->default_value = EXPOSURE_PIVARIETY_TO_V4L2((dev->cur_format->isp_info->isp_v1_info.exp_def), dev->cur_format);
        break;
    case ESP_CAM_SENSOR_GAIN:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION;
        qdesc->enumeration.count = cam_pivariety->pivariety_para.limited_gain_index;
        qdesc->enumeration.elements = pivariety_gain_map;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.gain_def;
        break;
    case ESP_CAM_SENSOR_VFLIP:
    case ESP_CAM_SENSOR_HMIRROR:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = 0;
        qdesc->number.maximum = 1;
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

static esp_err_t pivariety_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    struct pivariety_cam *cam_pivariety = (struct pivariety_cam *)dev->priv;
    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        *(uint32_t *)arg = cam_pivariety->pivariety_para.exposure_val;
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        *(uint32_t *)arg = cam_pivariety->pivariety_para.gain_index;
        break;
    }
    default: {
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    }
    return ret;
}

static esp_err_t pivariety_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;

    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        uint32_t u32_val = *(uint32_t *)arg;
        ret = pivariety_set_exp_val(dev, u32_val);
        break;
    }
    case ESP_CAM_SENSOR_EXPOSURE_US: {
        uint32_t u32_val = *(uint32_t *)arg;
        uint32_t ori_exp = EXPOSURE_V4L2_TO_PIVARIETY(u32_val, dev->cur_format);
        ret = pivariety_set_exp_val(dev, ori_exp);
        break;
    }

    case ESP_CAM_SENSOR_GAIN: {
        uint32_t u32_val = *(uint32_t *)arg;
        ret = pivariety_set_gain_val(dev, u32_val);
        break;
    }
    case ESP_CAM_SENSOR_VFLIP: {
        int *value = (int *)arg;
        ret = pivariety_set_vflip(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        int *value = (int *)arg;
        ret = pivariety_set_mirror(dev, *value);
        break;
    }
    default: {
        ESP_LOGE(TAG, "set id=%" PRIx32 " is not supported", id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }

    return ret;
}

static esp_err_t pivariety_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);

    if (pivariety_format_info != NULL && pivariety_format_info_size > 0) {
        formats->count = pivariety_format_info_size;
        formats->format_array = (const esp_cam_sensor_format_t *)&pivariety_format_info[0];
    } else {
        formats->count = 0;
        formats->format_array = NULL;
    }

    return ESP_OK;
}

static esp_err_t pivariety_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_raw = 1;
    return 0;
}

static esp_err_t pivariety_get_length_of_set(esp_cam_sensor_device_t *dev, uint16_t reg, uint32_t *length)
{
    uint32_t index = 0;
    uint32_t val = 0;
    uint32_t tmp_len = 0;
    esp_err_t ret = ESP_OK;

    ret = pivariety_read(dev->sccb_handle, reg, &val);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "length read failed");

    while (1) {
        ret = pivariety_write(dev->sccb_handle, reg, index);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "index reg write failed");
        ret = pivariety_read(dev->sccb_handle, reg, &tmp_len);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "read length failed");
        if (tmp_len == ERROR_DATA) {
            break;
        }
        index++;
        /* guard to avoid infinite loop */
        if (index > 10000) {
            pivariety_write(dev->sccb_handle, reg, val);
            ESP_LOGE(TAG, "too many entries when getting length of set");
            return ESP_ERR_INVALID_SIZE;
        }
    }
    *length = index;
    ret = pivariety_write(dev->sccb_handle, reg, val);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "index reg write failed");
    return ret;
}

static esp_err_t pivariety_map_format(pivariety_pixtype_t pivariety_format_type, esp_cam_sensor_format_t *format_info)
{
    char tmp[128];
    unsigned lanes;
    unsigned xclk_mhz;
    if (format_info == NULL) {
        ESP_LOGE(TAG, "format_info is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    const char *format_label = NULL;
    switch (pivariety_format_type) {
    case PIVARIETY_RAW8:
        format_info->format = ESP_CAM_SENSOR_PIXFORMAT_RAW8;
        format_label = "RAW8";
        break;
    case PIVARIETY_RAW10:
        format_info->format = ESP_CAM_SENSOR_PIXFORMAT_RAW10;
        format_label = "RAW10";
        break;
    case PIVARIETY_RAW12:
        format_info->format = ESP_CAM_SENSOR_PIXFORMAT_RAW12;
        format_label = "RAW12";
        break;
    case PIVARIETY_YUV420_8BIT:
        format_info->format = ESP_CAM_SENSOR_PIXFORMAT_YUV420;
        format_label = "YUV420_8bit";
        break;
    case PIVARIETY_YUV420_10BIT:
        format_info->format = ESP_CAM_SENSOR_PIXFORMAT_YUV420;
        format_label = "YUV420_10bit";
        break;
    case PIVARIETY_YUV422_8BIT:
        format_info->format = ESP_CAM_SENSOR_PIXFORMAT_YUV422;
        format_label = "YUV422_8bit";
        break;
    case PIVARIETY_JPEG:
        format_info->format = ESP_CAM_SENSOR_PIXFORMAT_JPEG;
        format_label = "JPEG";
        break;
    default:
        ESP_LOGE(TAG, "Unsupported format: 0x%x", pivariety_format_type);
        return ESP_ERR_NOT_SUPPORTED;
    }
    lanes = (unsigned)format_info->mipi_info.lane_num;
    xclk_mhz = (unsigned)(format_info->xclk / 1000000U);
    snprintf(tmp, sizeof(tmp), "MIPI_%ulane_%uMinput_%s_%ux%u_%ufps",
             lanes, xclk_mhz, (format_label ? format_label : "UNKNOWN"),
             (unsigned)format_info->width, (unsigned)format_info->height, (unsigned)format_info->fps);
    format_info->name = strdup(tmp);
    if (!format_info->name) {
        ESP_LOGE(TAG, "No memory for format_info->name");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t pivariety_map_bayerorder(pivariety_bayerorder_t pivariety_bayer_order, esp_cam_sensor_bayer_pattern_t *esp_bayer_order)
{
    switch (pivariety_bayer_order) {
    case PIVARIETY_BAYER_ORDER_BGGR:
        *esp_bayer_order = ESP_CAM_SENSOR_BAYER_BGGR;
        break;
    case PIVARIETY_BAYER_ORDER_GBRG:
        *esp_bayer_order = ESP_CAM_SENSOR_BAYER_GBRG;
        break;
    case PIVARIETY_BAYER_ORDER_GRBG:
        *esp_bayer_order = ESP_CAM_SENSOR_BAYER_GRBG;
        break;
    case PIVARIETY_BAYER_ORDER_RGGB:
        *esp_bayer_order = ESP_CAM_SENSOR_BAYER_RGGB;
        break;
    default:
        ESP_LOGE(TAG, "Unsupported bayer order: %d", pivariety_bayer_order);
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}

static esp_err_t pivariety_read_ctrl(esp_cam_sensor_device_t *dev, uint32_t v4l2_cid, uint32_t *value, pivariety_ctrltype_t ctrl_type)
{
    esp_err_t ret = pivariety_write(dev->sccb_handle, PIVARIETY_REG_CTRL_ID, v4l2_cid);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "ctrl id write failed");
    switch (ctrl_type) {
    case PIVARIETY_CTRL_MIN:
        ret = pivariety_read(dev->sccb_handle, PIVARIETY_REG_CTRL_MIN, value);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "ctrl min read failed");
        break;
    case PIVARIETY_CTRL_MAX:
        ret = pivariety_read(dev->sccb_handle, PIVARIETY_REG_CTRL_MAX, value);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "ctrl max read failed");
        break;
    case PIVARIETY_CTRL_STEP:
        ret = pivariety_read(dev->sccb_handle, PIVARIETY_REG_CTRL_STEP, value);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "ctrl step read failed");
        break;
    case PIVARIETY_CTRL_DEF:
        ret = pivariety_read(dev->sccb_handle, PIVARIETY_REG_CTRL_DEF, value);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "ctrl def read failed");
        break;
    case PIVARIETY_CTRL_VALUE:
        ret = pivariety_read(dev->sccb_handle, PIVARIETY_REG_CTRL_VALUE, value);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "ctrl val read failed");
        break;
    default:
        ESP_LOGE(TAG, "Unsupported ctrl type: %d", ctrl_type);
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ret;
}

static esp_err_t pivariety_write_ctrl(esp_cam_sensor_device_t *dev, uint32_t v4l2_cid, uint32_t value, pivariety_ctrltype_t ctrl_type)
{
    esp_err_t ret = pivariety_write(dev->sccb_handle, PIVARIETY_REG_CTRL_ID, v4l2_cid);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "ctrl id write failed");
    switch (ctrl_type) {
    case PIVARIETY_CTRL_MIN:
        ret = pivariety_write(dev->sccb_handle, PIVARIETY_REG_CTRL_MIN, value);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "ctrl min write failed");
        break;
    case PIVARIETY_CTRL_MAX:
        ret = pivariety_write(dev->sccb_handle, PIVARIETY_REG_CTRL_MAX, value);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "ctrl max write failed");
        break;
    case PIVARIETY_CTRL_STEP:
        ret = pivariety_write(dev->sccb_handle, PIVARIETY_REG_CTRL_STEP, value);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "ctrl step write failed");
        break;
    case PIVARIETY_CTRL_DEF:
        ret = pivariety_write(dev->sccb_handle, PIVARIETY_REG_CTRL_DEF, value);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "ctrl def write failed");
        break;
    case PIVARIETY_CTRL_VALUE:
        ret = pivariety_write(dev->sccb_handle, PIVARIETY_REG_CTRL_VALUE, value);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "ctrl val write failed");
        break;
    default:
        ESP_LOGE(TAG, "Unsupported ctrl type: %d", ctrl_type);
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ret;
}

static esp_err_t pivariety_update_ctrl(esp_cam_sensor_device_t *dev, uint32_t v4l2_cid, uint32_t *value, pivariety_ctrltype_t ctrl_type)
{
    esp_err_t ret = pivariety_read_ctrl(dev, v4l2_cid, value, PIVARIETY_CTRL_VALUE);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "ctrl val read failed");
    ret = pivariety_write_ctrl(dev, v4l2_cid, *value, PIVARIETY_CTRL_VALUE);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "ctrl val write failed");
    ret = pivariety_wait_for_free(dev->sccb_handle, 5);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "pivariety_wait_for_free error");
    ret = pivariety_read_ctrl(dev, v4l2_cid, value, ctrl_type);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "ctrl val read failed");
    return ret;
}

static void pivariety_dump_format(esp_cam_sensor_format_t *format_info)
{
    if (format_info == NULL) {
        ESP_LOGI(TAG, "No format info available");
        return;
    }

    ESP_LOGI(TAG, "---------- Format info ----------");
    ESP_LOGI(TAG, "name: %s", (format_info->name ? format_info->name : "(null)"));
    ESP_LOGI(TAG, "format(enum): %d", (int)format_info->format);
    ESP_LOGI(TAG, "port: %d, xclk: %u", (int)format_info->port, (unsigned)format_info->xclk);
    ESP_LOGI(TAG, "width: %u, height: %u, fps: %u", (unsigned)format_info->width, (unsigned)format_info->height, (unsigned)format_info->fps);

    ESP_LOGI(TAG, "mipi_info: mipi_clk=%u, lanes=%u, line_sync_en=%d",
             (unsigned)format_info->mipi_info.mipi_clk,
             (unsigned)format_info->mipi_info.lane_num,
             (int)format_info->mipi_info.line_sync_en);

    ESP_LOGI(TAG, "regs_size: %zu", format_info->regs_size);
    if (format_info->regs && format_info->regs_size > 0) {
        const pivariety_reginfo_t *regs = (const pivariety_reginfo_t *)format_info->regs;
        for (size_t j = 0; j < format_info->regs_size; j++) {
            if (regs[j].reg == PIVARIETY_REG_END) {
                ESP_LOGI(TAG, "regs[%zu]: END", j);
                break;
            }
            if (regs[j].reg == PIVARIETY_REG_DELAY) {
                ESP_LOGI(TAG, "regs[%zu]: DELAY %u ms", j, (unsigned)regs[j].val);
            } else {
                ESP_LOGI(TAG, "regs[%zu]: reg=0x%04x val=0x%08x", j, (unsigned)regs[j].reg, (unsigned)regs[j].val);
            }
        }
    } else {
        ESP_LOGI(TAG, "regs: (null)");
    }
    ESP_LOGI(TAG, "---------------------------------");

    if (format_info->isp_info) {
        ESP_LOGI(TAG, "----------- ISP info -----------");
        ESP_LOGI(TAG, "version: %u", (unsigned)format_info->isp_info->isp_v1_info.version);
        ESP_LOGI(TAG, "pclk: %u, vts: %u, hts: %u, tline_ns: %u",
                 (unsigned)format_info->isp_info->isp_v1_info.pclk,
                 (unsigned)format_info->isp_info->isp_v1_info.vts,
                 (unsigned)format_info->isp_info->isp_v1_info.hts,
                 (unsigned)format_info->isp_info->isp_v1_info.tline_ns);
        ESP_LOGI(TAG, "gain_def: %u, exp_def: %u, bayer_type: %d",
                 (unsigned)format_info->isp_info->isp_v1_info.gain_def,
                 (unsigned)format_info->isp_info->isp_v1_info.exp_def,
                 (int)format_info->isp_info->isp_v1_info.bayer_type);
    } else {
        ESP_LOGI(TAG, "isp_info: (null)");
    }
    ESP_LOGI(TAG, "--------------------------------");
}

static esp_err_t pivariety_enum_format(esp_cam_sensor_device_t *dev)
{
    uint32_t width, height;
    uint32_t num_format = 0;
    uint32_t num_resolution = 0;
    uint32_t format_type, bayer_type;
    esp_err_t ret = ESP_OK;
    esp_cam_sensor_format_t format_info = {
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .mipi_info = {
            .mipi_clk = 640000000,
            .line_sync_en = false,
        },
        .reserved = NULL,
    };
    esp_cam_sensor_isp_info_t isp_info = {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
        },
    };

    ret = pivariety_get_length_of_set(dev, PIVARIETY_REG_PIXFORMAT_INDEX, &num_format);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = pivariety_get_length_of_set(dev, PIVARIETY_REG_RESOLUTION_INDEX, &num_resolution);
    if (ret != ESP_OK) {
        return ret;
    }

    if (num_format == 0 || num_resolution == 0) {
        ESP_LOGE(TAG, "no formats/resolutions reported by sensor");
        return ESP_ERR_NOT_FOUND;
    }

    if (pivariety_format_info == NULL) {
        size_t total = num_format * num_resolution;
        pivariety_format_info = calloc(total, sizeof(esp_cam_sensor_format_t));
        pivariety_format_info_size = total;
        if (!pivariety_format_info) {
            ESP_LOGE(TAG, "No memory for format info");
            return ESP_ERR_NO_MEM;
        }
    }

    if (pivariety_isp_info == NULL) {
        size_t total = num_format * num_resolution;
        pivariety_isp_info = calloc(total, sizeof(esp_cam_sensor_isp_info_t));
        if (!pivariety_isp_info) {
            ESP_LOGE(TAG, "No memory for isp info");
            return ESP_ERR_NO_MEM;
        }
    }


    // Enumerate formats and resolutions
    for (uint32_t format_index = 0; format_index < num_format; format_index++) {
        ret = pivariety_write(dev->sccb_handle, PIVARIETY_REG_PIXFORMAT_INDEX, format_index);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "PIVARIETY_REG_PIXFORMAT_INDEX write failed");
        ret = pivariety_read(dev->sccb_handle, PIVARIETY_REG_PIXFORMAT_TYPE, &format_type);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "PIVARIETY_REG_PIXFORMAT_TYPE read failed");
        ret = pivariety_read(dev->sccb_handle, PIVARIETY_REG_MIPI_LANES, &format_info.mipi_info.lane_num);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "PIVARIETY_REG_MIPI_LANES read failed");
        ret = pivariety_read(dev->sccb_handle, PIVARIETY_REG_BAYER_ORDER, &bayer_type);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "PIVARIETY_REG_BAYER_ORDER read failed");
        ret = pivariety_map_bayerorder(bayer_type, &isp_info.isp_v1_info.bayer_type);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "pivariety_map_bayerorder failed");
        for (uint32_t res_index = 0; res_index < num_resolution; res_index++) {
            uint32_t index = format_index * num_resolution + res_index;
            ret = pivariety_write(dev->sccb_handle, PIVARIETY_REG_RESOLUTION_INDEX, res_index);
            ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "PIVARIETY_REG_RESOLUTION_INDEX write failed");
            ret = pivariety_read(dev->sccb_handle, PIVARIETY_REG_FORMAT_WIDTH, &width);
            ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "PIVARIETY_REG_FORMAT_WIDTH read failed");
            ret = pivariety_read(dev->sccb_handle, PIVARIETY_REG_FORMAT_HEIGHT, &height);
            ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "PIVARIETY_REG_FORMAT_HEIGHT read failed");
            ret = pivariety_update_ctrl(dev, V4L2_CID_ARDUCAM_FRAME_RATE, (uint32_t *)&format_info.fps, PIVARIETY_CTRL_DEF);
            ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "V4L2_CID_ARDUCAM_FRAME_RATE update failed");
            ret = pivariety_update_ctrl(dev, V4L2_CID_PIXEL_RATE, (uint32_t *)&isp_info.isp_v1_info.pclk, PIVARIETY_CTRL_DEF);
            ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "V4L2_CID_PIXEL_RATE update failed");
            ret = pivariety_update_ctrl(dev, V4L2_CID_VBLANK, (uint32_t *)&isp_info.isp_v1_info.vts, PIVARIETY_CTRL_DEF);
            ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "V4L2_CID_VBLANK update failed");
            ret = pivariety_update_ctrl(dev, V4L2_CID_HBLANK, (uint32_t *)&isp_info.isp_v1_info.hts, PIVARIETY_CTRL_DEF);
            ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "V4L2_CID_HBLANK update failed");
            ret = pivariety_update_ctrl(dev, V4L2_CID_ANALOGUE_GAIN, (uint32_t *)&isp_info.isp_v1_info.gain_def, PIVARIETY_CTRL_DEF);
            ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "V4L2_CID_ANALOGUE_GAIN update failed");
            ret = pivariety_update_ctrl(dev, V4L2_CID_EXPOSURE, (uint32_t *)&isp_info.isp_v1_info.exp_def, PIVARIETY_CTRL_DEF);
            ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "V4L2_CID_EXPOSURE update failed");
            size_t regs_count = 3;
            pivariety_reginfo_t *regs = calloc(regs_count, sizeof(pivariety_reginfo_t));
            if (regs == NULL) {
                ESP_LOGE(TAG, "No memory for regs for index %" PRIu32, index);
                return ESP_ERR_NO_MEM;
            }
            regs[0].reg = PIVARIETY_REG_PIXFORMAT_INDEX;  regs[0].val = format_index;
            regs[1].reg = PIVARIETY_REG_RESOLUTION_INDEX; regs[1].val = res_index;
            regs[2].reg = PIVARIETY_REG_END;    regs[2].val = 0x00000000;

            format_info.regs = regs;
            format_info.regs_size = regs_count;
            format_info.width = width;
            format_info.height = height;
            isp_info.isp_v1_info.hts += width;
            isp_info.isp_v1_info.vts += height;
            isp_info.isp_v1_info.tline_ns = (uint32_t)(isp_info.isp_v1_info.hts / (isp_info.isp_v1_info.pclk / 1000000.0) * 1000);
            ret = pivariety_map_format(format_type, &format_info);
            if (ret != ESP_OK) {
                free(regs);
                format_info.regs = NULL;
                format_info.regs_size = 0;
                ESP_LOGE(TAG, "map format error");
                return ret;
            }

            memcpy(&pivariety_isp_info[index], &isp_info, sizeof(esp_cam_sensor_isp_info_t));
            memcpy(&pivariety_format_info[index], &format_info, sizeof(esp_cam_sensor_format_t));
            pivariety_format_info[index].isp_info = &pivariety_isp_info[index];

            pivariety_dump_format(&pivariety_format_info[index]);

        }
    }

    ret = pivariety_write(dev->sccb_handle, PIVARIETY_REG_PIXFORMAT_INDEX, 0);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "PIVARIETY_REG_PIXFORMAT_INDEX write failed");
    ret = pivariety_write(dev->sccb_handle, PIVARIETY_REG_RESOLUTION_INDEX, 0);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "PIVARIETY_REG_RESOLUTION_INDEX write failed");

    return ret;
}

static esp_err_t pivariety_get_gain_val_map(esp_cam_sensor_device_t *dev)
{
    struct pivariety_cam *cam_pivariety = (struct pivariety_cam *)dev->priv;
    esp_err_t ret = ESP_OK;
    ret = pivariety_read(dev->sccb_handle, PIVARIETY_REG_IPC_GAIN_TABLE_LENGTH, \
                         (uint32_t *)&cam_pivariety->pivariety_para.limited_gain_index );
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "PIVARIETY_REG_IPC_GAIN_TABLE_LENGTH get failed");
    if (pivariety_gain_map == NULL) {
        pivariety_gain_map = calloc(cam_pivariety->pivariety_para.limited_gain_index, sizeof(uint32_t));
        if (!pivariety_gain_map) {
            ESP_LOGE(TAG, "No memory for gain val map");
            return ESP_ERR_NO_MEM;
        }
    }

    for (size_t i = 0; i < cam_pivariety->pivariety_para.limited_gain_index; i++) {
        ret = pivariety_read(dev->sccb_handle, PIVARIETY_REG_IPC_GAIN_TABLE_DATA, &pivariety_gain_map[i]);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "PIVARIETY_REG_IPC_GAIN_TABLE_DATA read failed");
    }

    return ret;
}

static esp_err_t pivariety_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    struct pivariety_cam *cam_pivariety = (struct pivariety_cam *)dev->priv;
    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        format = &pivariety_format_info[CONFIG_CAMERA_PIVARIETY_MIPI_IF_FORMAT_INDEX_DEFAULT];
    }

    ret = pivariety_write_array(dev->sccb_handle, (pivariety_reginfo_t *)format->regs, format->regs_size);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "pivariety write array failed");
    delay_ms(500);

    dev->cur_format = format;
    // init para
    cam_pivariety->pivariety_para.exposure_val = dev->cur_format->isp_info->isp_v1_info.exp_def;
    cam_pivariety->pivariety_para.gain_index = dev->cur_format->isp_info->isp_v1_info.gain_def;
    cam_pivariety->pivariety_para.exposure_max = dev->cur_format->isp_info->isp_v1_info.vts - 6;

    return ret;
}

static esp_err_t pivariety_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
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

static esp_err_t pivariety_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint32_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    PIVARIETY_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = pivariety_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = pivariety_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = pivariety_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = pivariety_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = pivariety_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = pivariety_get_sensor_id(dev, arg);
        break;
    default:
        break;
    }

    PIVARIETY_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t pivariety_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        PIVARIETY_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->pwdn_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        ESP_RETURN_ON_FALSE(gpio_config(&conf) == ESP_OK, ESP_ERR_INVALID_STATE, TAG, "pwdn pin gpio config failed");

        // carefully, logic is inverted compared to reset pin
        gpio_set_level(dev->pwdn_pin, 1);
        delay_ms(10);
        gpio_set_level(dev->pwdn_pin, 0);
        delay_ms(10);
    }

    if (dev->reset_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->reset_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        ESP_RETURN_ON_FALSE(gpio_config(&conf) == ESP_OK, ESP_ERR_INVALID_STATE, TAG, "reset pin gpio config failed");

        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }

    return ret;
}

static esp_err_t pivariety_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        PIVARIETY_DISABLE_OUT_XCLK(dev->xclk_pin);
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

static esp_err_t pivariety_delete(esp_cam_sensor_device_t *dev)
{
    pivariety_power_off(dev);
    ESP_LOGD(TAG, "del pivariety (%p)", dev);
    if (dev) {
        if (dev->priv) {
            free(dev->priv);
            dev->priv = NULL;
        }
        free(dev);
        dev = NULL;
    }
    if (pivariety_format_info) {
        for (size_t i = 0; i < pivariety_format_info_size; i++) {
            if (pivariety_format_info[i].regs) {
                free((void *)pivariety_format_info[i].regs);
                pivariety_format_info[i].regs = NULL;
            }
            if (pivariety_format_info[i].name != NULL) {
                free((void *)pivariety_format_info[i].name);
                pivariety_format_info[i].name = NULL;
            }
        }
        free(pivariety_format_info);
        pivariety_format_info = NULL;
        pivariety_format_info_size = 0;
    }
    if (pivariety_isp_info) {
        free(pivariety_isp_info);
        pivariety_isp_info = NULL;
    }
    if (pivariety_gain_map) {
        free(pivariety_gain_map);
        pivariety_gain_map = NULL;
    }

    return ESP_OK;
}

static const esp_cam_sensor_ops_t pivariety_ops = {
    .query_para_desc = pivariety_query_para_desc,
    .get_para_value = pivariety_get_para_value,
    .set_para_value = pivariety_set_para_value,
    .query_support_formats = pivariety_query_support_formats,
    .query_support_capability = pivariety_query_support_capability,
    .set_format = pivariety_set_format,
    .get_format = pivariety_get_format,
    .priv_ioctl = pivariety_priv_ioctl,
    .del = pivariety_delete
};

esp_cam_sensor_device_t *pivariety_detect(esp_cam_sensor_config_t *config)
{
    esp_cam_sensor_device_t *dev = NULL;
    struct pivariety_cam *cam_pivariety;

    if (config == NULL) {
        return NULL;
    }

    dev = calloc(1, sizeof(esp_cam_sensor_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    cam_pivariety = heap_caps_calloc(1, sizeof(struct pivariety_cam), MALLOC_CAP_DEFAULT);
    if (!cam_pivariety) {
        ESP_LOGE(TAG, "failed to calloc cam");
        free(dev);
        return NULL;
    }

    dev->name = (char *)PIVARIETY_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &pivariety_ops;
    dev->priv = cam_pivariety;
    if (pivariety_enum_format(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Enum format fail");
        goto err_free_handler;
    }
    dev->cur_format = (const esp_cam_sensor_format_t *)&pivariety_format_info[CONFIG_CAMERA_PIVARIETY_MIPI_IF_FORMAT_INDEX_DEFAULT];

    if (pivariety_get_gain_val_map(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Get gain val map fail");
        goto err_free_handler;
    }

    // Configure sensor power, clock, and SCCB port
    if (pivariety_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (pivariety_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != PIVARIETY_PID) {
        ESP_LOGE(TAG, "Camera sensor is not PIVARIETY, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    pivariety_delete(dev);
    return NULL;
}

#if CONFIG_CAMERA_PIVARIETY_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(pivariety_detect, ESP_CAM_SENSOR_MIPI_CSI, PIVARIETY_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return pivariety_detect(config);
}
#endif
