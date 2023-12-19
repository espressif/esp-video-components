/* SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_err.h"
#include "esp_log.h"

#include "esp_camera.h"

static const char *TAG = "camera";

#if CONFIG_ESP_VIDEO_CAMERA_SCCB_NUM >0
static const esp_camera_sccb_config_t sccb_config[CONFIG_ESP_VIDEO_CAMERA_SCCB_NUM] = {
    {
        .i2c_or_i3c = CONFIG_ESP_VIDEO_CAMERA_SCCB0_I2C_I3C,
        .scl_pin    = CONFIG_ESP_VIDEO_CAMERA_SCCB0_I2C_I3C_SCL_PIN,
        .sda_pin    = CONFIG_ESP_VIDEO_CAMERA_SCCB0_I2C_I3C_SDA_PIN,
        .port       = CONFIG_ESP_VIDEO_CAMERA_SCCB0_I2C_I3C_PORT,
        .freq       = CONFIG_ESP_VIDEO_CAMERA_SCCB0_I2C_I3C_FREQ,
    },
#if CONFIG_ESP_VIDEO_CAMERA_SCCB_NUM > 1
    {
        .i2c_or_i3c = CONFIG_ESP_VIDEO_CAMERA_SCCB1_I2C_I3C,
        .scl_pin    = CONFIG_ESP_VIDEO_CAMERA_SCCB1_I2C_I3C_SCL_PIN,
        .sda_pin    = CONFIG_ESP_VIDEO_CAMERA_SCCB1_I2C_I3C_SDA_PIN,
        .port       = CONFIG_ESP_VIDEO_CAMERA_SCCB1_I2C_I3C_PORT,
        .freq       = CONFIG_ESP_VIDEO_CAMERA_SCCB1_I2C_I3C_FREQ,
    },
#endif
};
#endif

#if CONFIG_ESP_VIDEO_CAMERA_INTF_CSI_NUM > 0
static const esp_camera_csi_config_t csi_config[CONFIG_ESP_VIDEO_CAMERA_INTF_CSI_NUM] = {
    {
        .sccb_config_index = CONFIG_ESP_VIDEO_CAMERA_CSI0_SCCB_INDEX,
        .xclk_pin          = CONFIG_ESP_VIDEO_CAMERA_CSI0_XCLK_PIN,
        .reset_pin         = CONFIG_ESP_VIDEO_CAMERA_CSI0_RESET_PIN,
        .pwdn_pin          = CONFIG_ESP_VIDEO_CAMERA_CSI0_PWDN_PIN,
        .xclk_freq_hz      = CONFIG_ESP_VIDEO_CAMERA_CSI0_XCLK_FREQ,
    },
};
#endif

#if CONFIG_ESP_VIDEO_CAMERA_INTF_DVP_NUM > 0
static const esp_camera_dvp_config_t dvp_config[CONFIG_ESP_VIDEO_CAMERA_INTF_DVP_NUM] = {
    {
        .sccb_config_index = CONFIG_ESP_VIDEO_CAMERA_DVP0_SCCB_INDEX,
        .xclk_pin          = CONFIG_ESP_VIDEO_CAMERA_DVP0_XCLK_PIN,
        .reset_pin         = CONFIG_ESP_VIDEO_CAMERA_DVP0_RESET_PIN,
        .pwdn_pin          = CONFIG_ESP_VIDEO_CAMERA_DVP0_PWDN_PIN,
        .xclk_freq_hz      = CONFIG_ESP_VIDEO_CAMERA_DVP0_XCLK_FREQ,
    },
#if CONFIG_ESP_VIDEO_CAMERA_INTF_DVP_NUM > 1
    {
        .sccb_config_index = CONFIG_ESP_VIDEO_CAMERA_DVP1_SCCB_INDEX,
        .xclk_pin          = CONFIG_ESP_VIDEO_CAMERA_DVP1_XCLK_PIN,
        .reset_pin         = CONFIG_ESP_VIDEO_CAMERA_DVP1_RESET_PIN,
        .pwdn_pin          = CONFIG_ESP_VIDEO_CAMERA_DVP1_PWDN_PIN,
        .xclk_freq_hz      = CONFIG_ESP_VIDEO_CAMERA_DVP1_XCLK_FREQ,
    },
#endif
};
#endif

static const esp_camera_config_t cam_config = {
    .sccb_num = CONFIG_ESP_VIDEO_CAMERA_SCCB_NUM,
    .sccb     = sccb_config,
#if CONFIG_ESP_VIDEO_CAMERA_INTF_CSI_NUM > 0
    .csi      = csi_config,
#endif
    .dvp_num  = CONFIG_ESP_VIDEO_CAMERA_INTF_DVP_NUM,
#if CONFIG_ESP_VIDEO_CAMERA_INTF_DVP_NUM > 0
    .dvp      = dvp_config,
#endif
};

void app_main(void)
{
    esp_err_t ret = ESP_OK;

    ret = esp_camera_init(&cam_config);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", ret);
    }
}
