/* SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "sccb.h"
#include "unity.h"
#include "esp_camera.h"
#include "mipi_csi.h"

#ifndef CONFIG_SCCB_BASED_I3C_ENABLED
#include "driver/i2c.h"
#define SCCB_PORT_NUM I2C_NUM_0   /*!< I2C port number for master dev */
#define SCCB_SCL_IO    23          /*!< gpio number for I2C master clock */
#define SCCB_SDA_IO    22          /*!< gpio number for I2C master data  */
#else
#define SCCB_PORT_NUM I3C_NUM_0   /*!< I3C port number for master dev */
#define SCCB_SCL_IO    19         /*!< gpio number for I3C master clock */
#define SCCB_SDA_IO    18         /*!< gpio number for I3C master data  */
#endif
#define FREQ_HZ          100000   /*!< clock frequency */
static const char *TAG = "cam_dump";

void app_main(void)
{
    esp_err_t ret = ESP_OK;
    if (sccb_init(SCCB_PORT_NUM, SCCB_SDA_IO, SCCB_SCL_IO, FREQ_HZ) != ESP_OK) {
        ESP_LOGE(TAG, "SCCB init failed");
    }
    // This should auto detect, not call it in manual
    extern esp_camera_device_t *sc2336_csi_detect(esp_camera_driver_config_t *config);
    esp_camera_driver_config_t drv_config = {
        .pwdn_pin = -1,
        .reset_pin = -1,
        .sccb_port = SCCB_PORT_NUM,
        .xclk_freq_hz = FREQ_HZ,
        .xclk_pin = -1,
    };

    esp_camera_device_t *device = sc2336_csi_detect(&drv_config);
    if (device) {
        const char *name = esp_camera_get_name(device);
        if (name) {
            ESP_LOGI(TAG, "device name is: %s", name);
        }
    }

    /*Init cam interface*/
    mipi_csi_port_config_t mipi_if_cfg = {0};
    esp_mipi_csi_handle_t handle = NULL;
    ret = esp_mipi_csi_driver_install(MIPI_CSI_PORT0, &mipi_if_cfg, 0, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "csi init fail[%d]", ret);
        return;
    }

    ESP_LOGI(TAG, "Test Done");
}
