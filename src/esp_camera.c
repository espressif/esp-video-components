/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include "hal/gpio_ll.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_camera.h"

#include "sccb.h"
#include "xclk.h"

#ifdef CONFIG_DVP_ENABLE
#include "dvp_video.h"
#endif

#ifdef CONFIG_MIPI_CSI_ENABLE
#include "mipi_csi_video.h"
#endif

#ifdef CONFIG_SIMULATED_INTF
#include "sim_video.h"
#endif

#ifdef CONFIG_ESP_VIDEO_MEDIA_CONTROLLER
#include "esp_media.h"
#endif

#include "esp_jpeg_video.h"

static const char *TAG = "esp_camera";

esp_err_t esp_camera_query_para_desc(esp_camera_device_t *dev, struct v4l2_query_ext_ctrl *qctrl)
{
    if (!dev || !dev->ops || !qctrl) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!dev->ops->query_para_desc) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    return dev->ops->query_para_desc(dev, qctrl);
}

esp_err_t esp_camera_get_para_value(esp_camera_device_t *dev, struct v4l2_ext_control *ctrl)
{
    if (!dev || !dev->ops || !ctrl) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!dev->ops->get_para_value) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    return dev->ops->get_para_value(dev, ctrl);
}

esp_err_t esp_camera_set_para_value(esp_camera_device_t *dev, const struct v4l2_ext_control *ctrl)
{
    if (!dev || !dev->ops || !ctrl) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!dev->ops->set_para_value) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    return dev->ops->set_para_value(dev, ctrl);
}

esp_err_t esp_camera_get_capability(esp_camera_device_t *dev, sensor_capability_t *caps)
{
    if (!dev || !dev->ops || !caps) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!dev->ops->query_support_capability) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    return dev->ops->query_support_capability(dev, caps);
}

esp_err_t esp_camera_query_format(esp_camera_device_t *dev, sensor_format_array_info_t *format_arry)
{
    if (!dev || !dev->ops || !format_arry) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!dev->ops->query_support_formats) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    return dev->ops->query_support_formats(dev, format_arry);
}

esp_err_t esp_camera_set_format(esp_camera_device_t *dev, const sensor_format_t *format)
{
    if (!dev || !dev->ops || !format) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!dev->ops->set_format) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    return dev->ops->set_format(dev, format);
}

esp_err_t esp_camera_get_format(esp_camera_device_t *dev, sensor_format_t *format)
{
    if (!dev || !dev->ops || !format) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!dev->ops->get_format) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    return dev->ops->get_format(dev, format);
}

esp_err_t esp_camera_ioctl(esp_camera_device_t *dev, uint32_t cmd, void *arg)
{
    if (!dev || !dev->ops) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!dev->ops->priv_ioctl) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    return dev->ops->priv_ioctl(dev, cmd, arg);
}

const char *esp_camera_get_name(esp_camera_device_t *dev)
{
    if (!dev) {
        return NULL;
    }

    return dev->name;
}

#if CONFIG_MIPI_CSI_ENABLE || CONFIG_DVP_ENABLE
static esp_err_t esp_camera_init_format(esp_camera_device_t *dev, int index)
{
    esp_err_t ret;
    sensor_format_array_info_t format_arry = {0};

    ret = esp_camera_query_format(dev, &format_arry);
    if (ret != ESP_OK) {
        return ret;
    }

    if (index >= format_arry.count) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Format description: %s", format_arry.format_array[index].name);

    ret = esp_camera_set_format(dev, &format_arry.format_array[index]);

    return ret;
}
#endif

esp_err_t esp_camera_init(const esp_camera_config_t *config)
{
    extern esp_camera_detect_fn_t __esp_camera_detect_fn_array_start;
    extern esp_camera_detect_fn_t __esp_camera_detect_fn_array_end;

    esp_err_t ret;
    esp_camera_detect_fn_t *p;

    if (config == NULL || config->sccb_num > 2
#ifdef CONFIG_DVP_ENABLE
            || config->dvp_num > 2
#endif
       ) {
        ESP_LOGW(TAG, "Please validate camera config");
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < config->sccb_num; i++) {
        if (config->sccb[i].sda_pin != -1 && config->sccb[i].scl_pin != -1) {
            ESP_LOGD(TAG, "Initializing SCCB[%d]", i);

            ret = sccb_i2c_init(config->sccb[i].port, config->sccb[i].sda_pin, config->sccb[i].scl_pin, config->sccb[i].freq);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "failed to initialize I2C port %d", config->sccb[i].port);
                return ret;
            }
            // ToDo: initialize the sccb driver, if freq == 0, using the default freq of 100000
        }
    }

    for (p = &__esp_camera_detect_fn_array_start; p < &__esp_camera_detect_fn_array_end; ++p) {
#ifdef CONFIG_MIPI_CSI_ENABLE
        if (p->intf == CAMERA_INTF_CSI && config->csi != NULL) {
            esp_camera_driver_config_t cfg = {
                .sccb_port = config->sccb[config->csi->ctrl_cfg.sccb_config_index].port,
                .xclk_pin = config->csi->ctrl_cfg.xclk_pin,
                .reset_pin = config->csi->ctrl_cfg.reset_pin,
                .pwdn_pin = config->csi->ctrl_cfg.pwdn_pin,
            };

            esp_camera_device_t *cam_dev = (*(p->fn))((void *)&cfg);

            // ToDo: initialize the csi driver and video layer

            // Avoid compiling warning
            if (cam_dev) {
                ret = esp_camera_init_format(cam_dev, 0);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "failed to initialize sensor format");
                    return ret;
                }

                ret = csi_create_camera_video_device(cam_dev);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "failed to create MIPI-CSI video device");
                    return ret;
                }
            } else {
                ESP_LOGE(TAG, "failed to detect MIPI-CSI camera");
                return ESP_FAIL;
            }
        }
#endif

#ifdef CONFIG_DVP_ENABLE
        if (p->intf == CAMERA_INTF_DVP &&  config->sccb_num != 0 && config->dvp_num > 0 && config->dvp != NULL) {
            for (size_t i = 0; i < config->dvp_num; i++) {
                esp_camera_device_t *cam_dev;
                esp_camera_driver_config_t cfg = {
                    .sccb_port = config->sccb[config->dvp->ctrl_cfg.sccb_config_index].port,
                    .xclk_pin = config->dvp[i].dvp_pin_cfg.xclk_pin,
                    .reset_pin = config->dvp->ctrl_cfg.reset_pin,
                    .pwdn_pin = config->dvp->ctrl_cfg.pwdn_pin,
                };

                ret = dvp_device_init_gpio(i, &config->dvp[i].dvp_pin_cfg);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "failed to initialzie GPIO of DVP index %d", i);
                    return ret;
                }

#ifdef CONFIG_DVP_ENABLE_OUTPUT_CLOCK
                ret = dvp_device_init_ouput_clock(i, config->dvp[i].ctrl_cfg.xclk_freq_hz);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "failed to initliaze LCD_CAM output XCLK of DVP index %d", i);
                    return ret;
                }
#else
                if (config->dvp[i].dvp_pin_cfg.xclk_pin >= 0) {
                    const esp_camera_dvp_ctrl_config_t *ctrl_cfg = &config->dvp[i].ctrl_cfg;

                    ret = xclk_enable_out_clock(ctrl_cfg->xclk_timer, ctrl_cfg->xclk_timer_channel,
                                                ctrl_cfg->xclk_freq_hz, config->dvp[i].dvp_pin_cfg.xclk_pin);
                    if (ret != ESP_OK) {
                        ESP_LOGE(TAG, "failed to initialize LEDC output XCLK of DVP index %d", i);
                        return ret;
                    }
                }
#endif

                cam_dev = (*(p->fn))((void *)&cfg);
                if (cam_dev) {
                    ret = esp_camera_init_format(cam_dev, 0);
                    if (ret != ESP_OK) {
                        ESP_LOGE(TAG, "failed to initialize sensor format");
                        return ret;
                    }

                    ret = dvp_create_camera_video_device(cam_dev, i, &config->dvp[i].dvp_pin_cfg);
                    if (ret != ESP_OK) {
                        ESP_LOGE(TAG, "failed to create DVP video device");
                        return ret;
                    }
                } else {
                    ESP_LOGE(TAG, "failed to detect DVP camera %d", i);
                    return ESP_FAIL;
                }
            }
        }
#endif

#ifdef CONFIG_SIMULATED_INTF
        if (p->intf == CAMERA_INTF_SIM && config->sim_num && config->sim != NULL) {
            for (size_t i = 0; i < config->sim_num; i++) {
                esp_camera_device_t *cam_dev = (*(p->fn))((void *)&config->sim[i]);

                if (cam_dev) {
                    ret = sim_create_camera_video_device(cam_dev);
                    if (ret != ESP_OK) {
                        ESP_LOGE(TAG, "failed to create sim camera video device=%p", cam_dev);
                        return ret;
                    }
                } else {
                    ESP_LOGE(TAG, "failed to initialize sim camera%d", i);
                    return ESP_FAIL;
                }
            }
        }
#endif
    }

#if CONFIG_ESP_VIDEO_SW_CODEC_JPEG_DEVICE
    ret = esp_sw_jpeg_create_video_device();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to create video device");
        return ret;
    }
#endif

#ifdef CONFIG_ESP_VIDEO_MEDIA_CONTROLLER
    if (esp_media_start() != ESP_OK) {
        ESP_LOGE(TAG, "Start media fail");
        return ESP_FAIL;
    }
#endif

    return ESP_OK;
}
