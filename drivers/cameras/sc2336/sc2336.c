/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "sc2336_settings.h"
#include "sccb.h"
#include "esp_log.h"

static const char *TAG = "sc2336";
#define SC2336_IO_MUX_LOCK
#define SC2336_IO_MUX_UNLOCK
#define CAMERA_ENABLE_OUT_CLOCK(pin,clk)
#define CAMERA_DISABLE_OUT_CLOCK()

#define SC2336_SCCB_ADDR   0x30
#define SC2336_PID         0xcb3a
#define SC2336_SENSOR_NAME "SC2336"
#define SC2336_OBJ_INDEX0 (0)
static sensor_t *s_sc2336_sensors[SENSOR_NUM_MAX];

static void delay_us(uint32_t t)
{
#if TEST_CSI_FPGA
    for (uint32_t tu = 0; tu < t; tu++);
#else
    ets_delay_us(t);
#endif
}

static int sc2336_read(uint16_t addr, uint8_t *read_buf)
{
    return sccb_read_reg16(SC2336_SCCB_ADDR, addr, 1, read_buf);
}

static int sc2336_write(uint16_t addr, uint8_t data)
{
    return sccb_write_reg16(SC2336_SCCB_ADDR, addr, 1, data);
}

/* write a array of registers  */
static int sc2336_write_array(reginfo_t *regarray)
{
    int i = 0, ret = 0;
    while (!ret && regarray[i].reg != SC2336_REG_END) {
        if (regarray[i].reg != SC2336_REG_DELAY) {
            ret = sc2336_write(regarray[i].reg, regarray[i].val);
        } else {
            delay_us(500); // Todo, append delay value.
        }
        i++;
    }
    ESP_LOGD(TAG, "count=%d", i);
    return ret;
}

static int sc2336_set_reg_bits(uint16_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    int ret = 0;
    uint8_t reg_data = 0;

    ret = sc2336_read(reg, &reg_data);
    if (ret < 0) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (ret & ~mask) | ((value << offset) & mask);
    ret = sc2336_write(reg, value);
    return ret;
}

static int sc2335_set_pattern(int enable)
{
    int ret = 0;
    if (enable) {
        ret = sc2336_set_reg_bits(0x4501, 3, 1, 0x01);
    } else {
        ret = sc2336_set_reg_bits(0x4501, 3, 1, 0x00);
    }
    return ret;
}

static int sc2336_soft_reset(void)
{
    int ret = sc2336_set_reg_bits(0x0103, 0, 1, 0x01);
    delay_us(5); // Todo, append delay value.
    return ret;
}

static int sc2336_get_sensor_id(sensor_id_t *id)
{
    int ret = -1;
    uint8_t pid_h, pid_l;
    sc2336_read(SC2336_REG_SENSOR_ID_H, &pid_h);
    sc2336_read(SC2336_REG_SENSOR_ID_L, &pid_l);
    uint16_t PID = (pid_h << 8) | pid_l;
    if (PID) {
        id->PID = PID;
        ret = 0;
    }
    return ret;
}

static int sc2336_set_stream(int enable)
{
    int ret = -1;
    if (enable) {
        ret = sc2336_write(SC2336_REG_SLEEP_MODE, 0x01);
    } else {
        ret = sc2336_write(SC2336_REG_SLEEP_MODE, 0x00);
    }
    // sc2335_set_pattern(sccb_port, 1);
    s_sc2336_sensors[SC2336_OBJ_INDEX0]->sensor_common.stream_status = enable;
    ESP_LOGD(TAG, "Stream=%d", enable);
    return ret;
}

static int sc2336_hw_power_on(sensor_probe_gpio_desc_t *gpios, int sccb_port, int xclk_freq_hz)
{
    int ret = 0;
    if (gpios->pin_xclk >= 0) {
        CAMERA_ENABLE_OUT_CLOCK(gpios->pin_xclk, xclk_freq_hz);
    }

    return ret;
}

static int sc2336_set_mirror(int enable)
{
    int ret = -1;
    if (enable) {
        ret = sc2336_set_reg_bits(0x3221, 1, 2, 0x03);
    } else {
        ret = sc2336_set_reg_bits(0x3221, 1, 2, 0x00);
    }

    return ret;
}

static int sc2336_set_vflip(int enable)
{
    int ret = -1;
    if (enable) {
        ret = sc2336_set_reg_bits(0x3221, 5, 2, 0x03);
    } else {
        ret = sc2336_set_reg_bits(0x3221, 5, 2, 0x00);
    }

    return ret;
}

static int query_support_formats(void *parry)
{
    sensor_format_array_info_t *formats = (sensor_format_array_info_t *)parry;
    formats->count = ARRAY_SIZE(sc2336_format_info);
    formats->format_array = &sc2336_format_info[0];
    ESP_LOGI(TAG, "f_array=%p", formats->format_array);
    return 0;
}

static int query_support_capability(void *arg)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);
    sensor_capability_t *sensor_cap = (sensor_capability_t *)arg;
    sensor_cap->fmt_raw = 1;
    return 0;
}

static int set_format(void *format)
{
    int ret = 0;
    sensor_format_t *fmt = (sensor_format_t *)format;

    ret = sc2336_write_array((reginfo_t *)fmt->regs);

    if (ret < 0) {
        ESP_CAM_SENSOR_LOGE("Set format regs fail");
        return ESP_CAM_SENSOR_FAILED_TO_S_FORMAT;
    }

    s_sc2336_sensors[SC2336_OBJ_INDEX0]->sensor_common.cur_format = fmt;

    return ret;
}

static int get_format(void *ret_format)
{
    int ret = -1;
    sensor_format_t **format = (sensor_format_t **)ret_format;
    if (s_sc2336_sensors[SC2336_OBJ_INDEX0]->sensor_common.cur_format != NULL) {
        *format = (void *)s_sc2336_sensors[SC2336_OBJ_INDEX0]->sensor_common.cur_format;
        ret = 0;
    }
    return ret;
}

static int priv_ioctl(unsigned int cmd, void *arg)
{
    int ret = 0;
    reginfo_t *sc2336_reg;
    SC2336_IO_MUX_LOCK

    if (cmd & SENSOR_IOC_SET) {
        switch (cmd) {
        case CAM_SENSOR_S_HW_RESET:
            ret = 0;
            break;
        case CAM_SENSOR_S_SF_RESET:
            ret = sc2336_soft_reset();
            break;
        case CAM_SENSOR_S_REG:
            sc2336_reg = (reginfo_t *)arg;
            ret = sc2336_write(sc2336_reg->reg, sc2336_reg->val);
            break;
        case CAM_SENSOR_S_STREAM:
            ret = sc2336_set_stream(*(int *)arg);
            break;
        case CAM_SENSOR_S_VFLIP:
            ret = sc2336_set_vflip(*(int *)arg);
            break;
        case CAM_SENSOR_S_HMIRROR:
            ret = sc2336_set_mirror(*(int *)arg);
            break;
        case CAM_SENSOR_S_TEST_PATTERN:
            ret = sc2335_set_pattern(*(int *)arg);
            break;
        }
    } else {
        switch (cmd) {
        case CAM_SENSOR_G_REG:
            sc2336_reg = (reginfo_t *)arg;
            ret = sc2336_read(sc2336_reg->reg, &sc2336_reg->val);
            break;
        case CAM_SENSOR_G_CHIP_ID:
            ret = sc2336_get_sensor_id(arg);
            break;
        default:
            break;
        }
    }
    SC2336_IO_MUX_UNLOCK
    return ret;
}

int get_name(void *name, size_t *size)
{
    strcpy((char *)name, SC2336_SENSOR_NAME);
    *size = strlen(SC2336_SENSOR_NAME);
    return 0;
}

static esp_camera_ops_t sc2336_ops = {
    .query_support_formats = query_support_formats,
    .query_support_capability = query_support_capability,
    .set_format = set_format,
    .get_format = get_format,
    .priv_ioctl = priv_ioctl,
    .get_name = get_name,
};

// We need manage these devices, and maybe need to add it into the private member of esp_device
esp_camera_device_t sc2336_detect(void)
{
    /*Providing the correct power-on sequence and clock for the sensor can the sensor work properly.*/
    sensor_probe_gpio_desc_t gpio_config = {
        .pin_pwdn = -1,
        .pin_reset = -1,
        .pin_xclk = -1,
    };
    int clk = 24000000;
    // Configure sensor power, clock, and I2C port
    sc2336_hw_power_on(&gpio_config, 0, clk);

    uint8_t pid_h = 0, pid_l = 0;
    sccb_read_reg16(SC2336_SCCB_ADDR, SC2336_REG_SENSOR_ID_H, 1, &pid_h);
    sccb_read_reg16(SC2336_SCCB_ADDR, SC2336_REG_SENSOR_ID_L, 1, &pid_l);
    uint16_t PID = (pid_h << 8) | pid_l;
    if (SC2336_PID != PID) {
        return NULL;
    }

    s_sc2336_sensors[SC2336_OBJ_INDEX0] = (sensor_t *)calloc(sizeof(sensor_t), 1);
    if (s_sc2336_sensors[SC2336_OBJ_INDEX0] == NULL) {
        ESP_LOGE(TAG, "Sensor obj calloc fail");
        // return ESP_ERR_NO_MEM;
        return NULL;
    }

    esp_camera_device_t handle = (esp_camera_device_t)&sc2336_ops;

    return handle;
}