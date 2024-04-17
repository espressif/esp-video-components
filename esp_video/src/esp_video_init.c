/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <inttypes.h>
#include "hal/gpio_ll.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_sccb_i2c.h"
#include "esp_cam_sensor_detect.h"

#include "esp_video_init.h"
#include "esp_video_cam_device.h"

static const char *TAG = "esp_video_init";

/**
 * @brief Create SCCB device
 *
 * @param init_sccb_config SCCB initialization configuration
 * @param dev_addr device address
 *
 * @return
 *      - SCCB handle on success
 *      - NULL if failed
 */
static esp_sccb_io_handle_t create_sccb_device(const esp_video_init_sccb_config_t *init_sccb_config, uint16_t dev_addr)
{
    esp_err_t ret;
    esp_sccb_io_handle_t sccb_io;
    sccb_i2c_config_t sccb_config = {0};
    i2c_master_bus_handle_t bus_handle;

    if (init_sccb_config->init_sccb) {
        i2c_master_bus_config_t i2c_bus_config = {0};

        i2c_bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
        i2c_bus_config.i2c_port = init_sccb_config->i2c_config.port;
        i2c_bus_config.scl_io_num = init_sccb_config->i2c_config.scl_pin;
        i2c_bus_config.sda_io_num = init_sccb_config->i2c_config.sda_pin;
        i2c_bus_config.glitch_ignore_cnt = 7;
        i2c_bus_config.flags.enable_internal_pullup = true,

        ret = i2c_new_master_bus(&i2c_bus_config, &bus_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "failed to initialize I2C master bus port %d", init_sccb_config->i2c_config.port);
            return NULL;
        }
    } else {
        bus_handle = init_sccb_config->i2c_handle;
    }

    sccb_config.dev_addr_length = I2C_ADDR_BIT_LEN_7,
    sccb_config.device_address = dev_addr,
    sccb_config.scl_speed_hz = init_sccb_config->i2c_config.freq,
    ret = sccb_new_i2c_io(bus_handle, &sccb_config, &sccb_io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize SCCB port %d", init_sccb_config->i2c_config.port);
        return NULL;
    }

    return sccb_io;
}

/**
 * @brief Initialize video hardware and software, including I2C, MIPI CSI and so on.
 *
 * @param config video hardware configuration
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_init(const esp_video_init_config_t *config)
{
    esp_err_t ret;

    if (config == NULL) {
        ESP_LOGW(TAG, "Please validate camera config");
        return ESP_ERR_INVALID_ARG;
    }

    for (esp_cam_sensor_detect_fn_t *p = &__esp_cam_sensor_detect_fn_array_start; p < &__esp_cam_sensor_detect_fn_array_end; ++p) {
#if CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE
        if (p->port == ESP_CAM_SENSOR_MIPI_CSI && config->csi != NULL) {
            esp_cam_sensor_config_t cfg;
            esp_cam_sensor_device_t *cam_dev;

            cfg.sccb_handle = create_sccb_device(&config->csi->sccb_config, p->sccb_addr);
            if (!cfg.sccb_handle) {
                return ESP_FAIL;
            }

            cfg.reset_pin = config->csi->reset_pin,
            cfg.pwdn_pin = config->csi->pwdn_pin,
            cam_dev = (*(p->fn))((void *)&cfg);
            if (!cam_dev) {
                ESP_LOGE(TAG, "failed to detect MIPI-CSI camera");
                return ESP_FAIL;
            }

            ret = esp_video_create_csi_video_device(cam_dev);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "failed to create MIPI-CSI video device");
                return ret;
            }
        }
#endif
    }

    return ESP_OK;
}
