/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include "sdkconfig.h"
#include <string.h>
#include <inttypes.h>
#include <sys/lock.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "hal/gpio_ll.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#if CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE
#include "usb/usb_host.h"
#endif
#include "esp_sccb_i2c.h"
#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"
#if CONFIG_ESP_VIDEO_ENABLE_CAMERA_MOTOR_CONTROLLER
#include "esp_cam_motor.h"
#include "esp_cam_motor_detect.h"
#endif
#include "esp_cam_sensor_xclk.h"
#include "esp_video_init.h"
#include "esp_video_device_internal.h"
#if CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE
#include "esp_cam_ctlr_dvp_ext.h"
#endif
#if CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER
#include "esp_video_pipeline_isp.h"
#endif

#if ESP_VIDEO_ENABLE_SCCB_DEVICE
typedef esp_err_t (*esp_video_create_device_fn_t)(esp_cam_sensor_device_t *cam, void *priv);
typedef esp_err_t (*esp_video_init_clk_fn_t)(void *priv);
typedef esp_err_t (*esp_video_deinit_clk_fn_t)(void *priv);

typedef struct i2c_dev {
    i2c_master_bus_handle_t i2c_handle;
    gpio_num_t scl_pin;
    gpio_num_t sda_pin;
} i2c_dev_t;

typedef struct video_device_init_config {
    bool is_motor;
    union {
        struct {
            gpio_num_t reset_pin;
            gpio_num_t pwdn_pin;
            esp_cam_sensor_detect_fn_t *detect;
        } sensor_cfg;

#if CONFIG_ESP_VIDEO_ENABLE_CAMERA_MOTOR_CONTROLLER
        struct {
            gpio_num_t reset_pin;
            gpio_num_t pwdn_pin;
            gpio_num_t signal_pin;
            esp_cam_motor_detect_fn_t *detect;
        } motor_cfg;
#endif /* CONFIG_ESP_VIDEO_ENABLE_CAMERA_MOTOR_CONTROLLER */
    };

    const esp_video_init_sccb_config_t *sccb_config;

    esp_video_create_device_fn_t create_func;
    void *create_priv;

    esp_video_init_clk_fn_t init_clk_func;
    esp_video_deinit_clk_fn_t deinit_clk_func;
    void *clk_priv;

    uint8_t *device_inited;
} video_device_init_config_t;

#if CONFIG_ESP_VIDEO_ENABLE_SPI_VIDEO_DEVICE
typedef struct spi_dev {
    esp_cam_sensor_xclk_handle_t xclk_handle;
} spi_dev_t;

typedef struct spi_video_device_init_clk_config {
    int index;

    gpio_num_t xclk_pin;
    int xclk_freq_hz;

    esp_cam_sensor_xclk_source_t xclk_source;

    /* This is used when xclk_source is ESP_CAM_SENSOR_XCLK_LEDC */

#if CONFIG_CAMERA_XCLK_USE_LEDC
    struct {
        ledc_timer_t timer;                     /*!< The timer source of channel */
        ledc_clk_cfg_t clk_cfg;                 /*!< LEDC source clock from ledc_clk_cfg_t */
        ledc_channel_t channel;                 /*!< LEDC channel used for XCLK */
    } xclk_ledc_cfg;
#endif
} spi_video_device_init_clk_config_t;

typedef struct spi_video_device_init_config_t {
    int index;
    const esp_video_init_spi_config_t *spi_config;
} spi_video_device_init_config_t;

static spi_dev_t s_spi_dev[ESP_VIDEO_SPI_DEVICE_NUM];
#endif /* CONFIG_ESP_VIDEO_ENABLE_SPI_VIDEO_DEVICE */

static i2c_dev_t s_i2c_dev[I2C_NUM_MAX];
#endif /* ESP_VIDEO_ENABLE_SCCB_DEVICE */
#if CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE
static SemaphoreHandle_t s_usb_uvc_sem;
#endif /* CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE */
static uint32_t s_video_device_inited_flags = 0;
static _lock_t s_init_lock;
static const char *TAG = "esp_video_init";

#if ESP_VIDEO_ENABLE_SCCB_DEVICE
static esp_err_t destroy_cam_device(esp_cam_sensor_device_t *cam)
{
    int reset_pin = cam->reset_pin;
    int pwdn_pin = cam->pwdn_pin;
    esp_err_t ret = esp_cam_sensor_del_dev(cam);
    if (ret == ESP_OK) {
        if (reset_pin >= 0) {
            gpio_reset_pin(reset_pin);
        }
        if (pwdn_pin >= 0) {
            gpio_reset_pin(pwdn_pin);
        }
    }

    return ret;
}

/**
 * @brief Initialize I2C and SCCB devices
 *
 * @param config video device initialization configuration
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t esp_video_init_sensor_and_video_device(const video_device_init_config_t *config)
{
    esp_err_t ret = ESP_OK;
    bool i2c_initialized = false;
    i2c_master_bus_handle_t i2c_handle = NULL;
    const esp_video_init_sccb_config_t *sccb_config = config->sccb_config;

    /**
     * Step 1: Check if I2C master bus handle is already initialized
     */
    if (sccb_config->init_sccb) {
        if (s_i2c_dev[sccb_config->i2c_config.port].i2c_handle != NULL) {
            ESP_LOGD(TAG, "I2C master port %d is already initialized", sccb_config->i2c_config.port);

            if (s_i2c_dev[sccb_config->i2c_config.port].scl_pin != sccb_config->i2c_config.scl_pin ||
                    s_i2c_dev[sccb_config->i2c_config.port].sda_pin != sccb_config->i2c_config.sda_pin) {
                ESP_LOGE(TAG, "I2C master port %d SCL pin or SDA pin is mismatched", sccb_config->i2c_config.port);
                return ESP_ERR_INVALID_ARG;
            } else {
                ESP_LOGD(TAG, "I2C master port %d SCL pin and SDA pin is matched", sccb_config->i2c_config.port);
            }

            i2c_handle = s_i2c_dev[sccb_config->i2c_config.port].i2c_handle;
        } else {

            /**
             * Step 2: Initialize I2C master bus handle
             */
            i2c_master_bus_config_t i2c_bus_config = {0};

            i2c_bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
            i2c_bus_config.i2c_port = sccb_config->i2c_config.port;
            i2c_bus_config.scl_io_num = sccb_config->i2c_config.scl_pin;
            i2c_bus_config.sda_io_num = sccb_config->i2c_config.sda_pin;
            i2c_bus_config.glitch_ignore_cnt = 7;
            i2c_bus_config.flags.enable_internal_pullup = true;

            ESP_RETURN_ON_ERROR(i2c_new_master_bus(&i2c_bus_config, &s_i2c_dev[sccb_config->i2c_config.port].i2c_handle), TAG, "Failed to initialize I2C master bus port %d", sccb_config->i2c_config.port);
            s_i2c_dev[sccb_config->i2c_config.port].scl_pin = sccb_config->i2c_config.scl_pin;
            s_i2c_dev[sccb_config->i2c_config.port].sda_pin = sccb_config->i2c_config.sda_pin;

            i2c_handle = s_i2c_dev[sccb_config->i2c_config.port].i2c_handle;
            i2c_initialized = true;
        }
    } else {
        ESP_RETURN_ON_FALSE(sccb_config->i2c_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "I2C handle is not initialized");
        i2c_handle = sccb_config->i2c_handle;
    }

    /**
     * Step 3: Initialize SCCB device
     */
    esp_sccb_io_handle_t sccb_io;
    sccb_i2c_config_t sccb_i2c_config = {0};
    uint16_t sccb_addr = 0;

    if (config->is_motor) {
#if CONFIG_ESP_VIDEO_ENABLE_CAMERA_MOTOR_CONTROLLER
        sccb_addr = config->motor_cfg.detect->sccb_addr;
#endif /* CONFIG_ESP_VIDEO_ENABLE_CAMERA_MOTOR_CONTROLLER */
    } else {
        sccb_addr = config->sensor_cfg.detect->sccb_addr;
    }

    sccb_i2c_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    sccb_i2c_config.device_address = sccb_addr;
    sccb_i2c_config.scl_speed_hz = sccb_config->freq;
    ESP_GOTO_ON_ERROR(sccb_new_i2c_io(i2c_handle, &sccb_i2c_config, &sccb_io), fail_0, TAG, "Failed to initialize SCCB device");

    if (config->init_clk_func) {
        ESP_GOTO_ON_ERROR(config->init_clk_func(config->clk_priv), fail_1, TAG, "Failed to initialize clock");
    }

    esp_cam_sensor_device_t *cam_dev = NULL;
    gpio_num_t reset_pin = GPIO_NUM_NC;
    gpio_num_t pwdn_pin = GPIO_NUM_NC;
    if (config->is_motor) {
#if CONFIG_ESP_VIDEO_ENABLE_CAMERA_MOTOR_CONTROLLER
        esp_cam_motor_config_t cfg = {
            .sccb_handle = sccb_io,
            .reset_pin = config->motor_cfg.reset_pin,
            .pwdn_pin = config->motor_cfg.pwdn_pin,
            .signal_pin = config->motor_cfg.signal_pin,
        };

        esp_cam_motor_device_t *motor_dev = config->motor_cfg.detect->detect(&cfg);
        if (!motor_dev) {
            ret = ESP_OK;
            reset_pin = config->motor_cfg.reset_pin;
            pwdn_pin = config->motor_cfg.pwdn_pin;
            ESP_LOGE(TAG, "Failed to detect camera motor with address=%x", config->motor_cfg.detect->sccb_addr);
            goto fail_2;
        }

        cam_dev = (esp_cam_sensor_device_t *)motor_dev;
#else
        ESP_LOGE(TAG, "Camera motor is not supported");
        goto fail_2;
#endif /* CONFIG_ESP_VIDEO_ENABLE_CAMERA_MOTOR_CONTROLLER */
    } else {
        esp_cam_sensor_config_t cfg = {
            .sccb_handle = sccb_io,
            .reset_pin = config->sensor_cfg.reset_pin,
            .pwdn_pin = config->sensor_cfg.pwdn_pin,
            .sensor_port = config->sensor_cfg.detect->port,
        };

        cam_dev = config->sensor_cfg.detect->detect(&cfg);
        if (!cam_dev) {
            ret = ESP_OK;
            reset_pin = config->sensor_cfg.reset_pin;
            pwdn_pin = config->sensor_cfg.pwdn_pin;
            ESP_LOGE(TAG, "Failed to detect camera sensor with address=%x", config->sensor_cfg.detect->sccb_addr);
            goto fail_2;
        }
    }

    ESP_GOTO_ON_ERROR(config->create_func(cam_dev, config->create_priv), fail_3, TAG, "Failed to initialize video device %p", config->create_func);

    /**
     * Set device_inited to 1 to mean that the video device has been initialized, and skip the next detection for this port.
     */
    if (config->device_inited) {
        *config->device_inited = 1;
    }

    return ESP_OK;

fail_3:
    if (config->is_motor) {
#if CONFIG_ESP_VIDEO_ENABLE_CAMERA_MOTOR_CONTROLLER
        esp_cam_motor_del_dev((esp_cam_motor_device_t *)cam_dev);
#endif /* CONFIG_ESP_VIDEO_ENABLE_CAMERA_MOTOR_CONTROLLER */
    } else {
        destroy_cam_device(cam_dev);
    }
fail_2:
    if (reset_pin >= 0) {
        gpio_reset_pin(reset_pin);
    }
    if (pwdn_pin >= 0) {
        gpio_reset_pin(pwdn_pin);
    }
    if (config->deinit_clk_func) {
        config->deinit_clk_func(config->clk_priv);
    }
fail_1:
    esp_sccb_del_i2c_io(sccb_io);
fail_0:
    if (i2c_initialized) {
        i2c_del_master_bus(i2c_handle);
        s_i2c_dev[sccb_config->i2c_config.port].i2c_handle = NULL;
    }
    return ret;
}

#if CONFIG_ESP_VIDEO_ENABLE_SPI_VIDEO_DEVICE
static esp_err_t init_spi_clk_func(void *priv)
{
    esp_err_t ret = ESP_OK;
    const spi_video_device_init_clk_config_t *config = priv;
    esp_cam_sensor_xclk_handle_t xclk_handle = NULL;

    if (config->xclk_pin >= 0 && config->xclk_freq_hz > 0) {
        bool is_xclk_configured = false;
        esp_cam_sensor_xclk_config_t cam_xclk_config;

        memset(&cam_xclk_config, 0, sizeof(esp_cam_sensor_xclk_config_t));

#if CONFIG_CAMERA_XCLK_USE_LEDC
        if (config->xclk_source == ESP_CAM_SENSOR_XCLK_LEDC) {
            cam_xclk_config.ledc_cfg.timer = config->xclk_ledc_cfg.timer;
            cam_xclk_config.ledc_cfg.clk_cfg = config->xclk_ledc_cfg.clk_cfg;
            cam_xclk_config.ledc_cfg.channel = config->xclk_ledc_cfg.channel;
            cam_xclk_config.ledc_cfg.xclk_freq_hz = config->xclk_freq_hz;
            cam_xclk_config.ledc_cfg.xclk_pin = config->xclk_pin;
            is_xclk_configured = true;
        }
#endif
#if CONFIG_CAMERA_XCLK_USE_ESP_CLOCK_ROUTER
        if (config->xclk_source == ESP_CAM_SENSOR_XCLK_ESP_CLOCK_ROUTER) {
            cam_xclk_config.esp_clock_router_cfg.xclk_pin = config->xclk_pin;
            cam_xclk_config.esp_clock_router_cfg.xclk_freq_hz = config->xclk_freq_hz;
            is_xclk_configured = true;
        }
#endif

        ESP_RETURN_ON_FALSE(is_xclk_configured, ESP_ERR_INVALID_ARG, TAG, "XCLK is not configured");

        ESP_GOTO_ON_ERROR(esp_cam_sensor_xclk_allocate(config->xclk_source, &xclk_handle), fail_0, TAG, "Failed to allocate XCLK");
        ESP_GOTO_ON_ERROR(esp_cam_sensor_xclk_start(xclk_handle, &cam_xclk_config), fail_1, TAG, "Failed to start XCLK");

        s_spi_dev[config->index].xclk_handle = xclk_handle;
    }

    return ESP_OK;

fail_1:
    esp_cam_sensor_xclk_free(xclk_handle);
fail_0:
    return ret;
}

static esp_err_t deinit_spi_clk_func(void *priv)
{
    esp_err_t ret = ESP_OK;
    const spi_video_device_init_clk_config_t *config = priv;

    if (config->xclk_pin >= 0 && config->xclk_freq_hz > 0) {
        esp_cam_sensor_xclk_handle_t xclk_handle = s_spi_dev[config->index].xclk_handle;

        ESP_RETURN_ON_ERROR(esp_cam_sensor_xclk_stop(xclk_handle), TAG, "Failed to stop XCLK");
        ESP_RETURN_ON_ERROR(esp_cam_sensor_xclk_free(xclk_handle), TAG, "Failed to free XCLK");
        s_spi_dev[config->index].xclk_handle = NULL;
    }

    return ret;
}

static esp_err_t create_spi_video_device(esp_cam_sensor_device_t *cam, void *priv)
{
    const spi_video_device_init_config_t *spi_config = priv;
    const esp_video_init_spi_config_t *spi_init_config = spi_config->spi_config;
    esp_video_spi_device_config_t spi_dev_config = {
        .intf = spi_init_config->intf,
        .io_mode = spi_init_config->io_mode,
        .spi_port = spi_init_config->spi_port,
        .spi_cs_pin = spi_init_config->spi_cs_pin,
        .spi_sclk_pin = spi_init_config->spi_sclk_pin,
        .spi_data0_io_pin = spi_init_config->spi_data0_io_pin,
        .spi_data1_io_pin = spi_init_config->spi_data1_io_pin,
        .spi_data2_io_pin = spi_init_config->spi_data2_io_pin,
        .spi_data3_io_pin = spi_init_config->spi_data3_io_pin,
    };

    return esp_video_create_spi_video_device(cam, &spi_dev_config, spi_config->index);
}

static esp_err_t destroy_spi_video_device(void)
{
    esp_err_t ret = ESP_OK;

    for (int i = 0; i < ESP_VIDEO_SPI_DEVICE_NUM; i++) {
        esp_cam_sensor_device_t *cam_dev = esp_video_get_spi_video_device_sensor(i);
        if (!cam_dev) {
            continue;
        }
        ESP_RETURN_ON_ERROR(esp_sccb_del_i2c_io(cam_dev->sccb_handle), TAG, "Failed to delete SCCB device");
        ESP_RETURN_ON_ERROR(destroy_cam_device(cam_dev), TAG, "Failed to delete SPI sensor");
        ESP_RETURN_ON_ERROR(esp_video_destroy_spi_video_device(i), TAG, "Failed to destroy SPI video device");
    }

    return ret;
}
#endif /* CONFIG_ESP_VIDEO_ENABLE_SPI_VIDEO_DEVICE */

#if CONFIG_ESP_VIDEO_ENABLE_CAMERA_MOTOR_CONTROLLER
static esp_err_t create_cam_motor_video_device(esp_cam_sensor_device_t *cam, void *priv)
{
    return esp_video_csi_video_device_add_motor((esp_cam_motor_device_t *)cam);
}

#endif /* CONFIG_ESP_VIDEO_ENABLE_CAMERA_MOTOR_CONTROLLER */

#if CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE
static esp_err_t create_csi_video_device(esp_cam_sensor_device_t *cam, void *priv)
{
    const esp_video_init_config_t *config = priv;
    esp_video_csi_device_config_t csi_dev_config = {
        .dont_init_ldo = config->csi->dont_init_ldo,
    };
    ESP_RETURN_ON_ERROR(esp_video_create_csi_video_device(cam, &csi_dev_config), TAG, "Failed to create CSI video device");

#if CONFIG_ESP_VIDEO_ENABLE_CAMERA_MOTOR_CONTROLLER
    uint8_t motor_inited = 0;
    if (config->cam_motor) {
        const esp_video_init_cam_motor_config_t *cm = config->cam_motor;

        for (esp_cam_motor_detect_fn_t *p = &__esp_cam_motor_detect_fn_array_start; p < &__esp_cam_motor_detect_fn_array_end; p++) {
            video_device_init_config_t motor_init_config = {
                .is_motor = true,
                .sccb_config = &cm->sccb_config,
                .motor_cfg = {
                    .reset_pin = cm->reset_pin,
                    .pwdn_pin = cm->pwdn_pin,
                    .signal_pin = cm->signal_pin,
                    .detect = p,
                },
                .create_func = create_cam_motor_video_device,
                .device_inited = &motor_inited,
            };
            esp_err_t ret = esp_video_init_sensor_and_video_device(&motor_init_config);
            if (ret != ESP_OK) {
                esp_video_destroy_csi_video_device();
                ESP_LOGE(TAG, "Failed to initialize camera motor");
                return ret;
            }
            if (motor_inited) {
                s_video_device_inited_flags |= ESP_VIDEO_INIT_FLAGS_MOTOR;
                break;
            }
        }
    }
#endif /* CONFIG_ESP_VIDEO_ENABLE_CAMERA_MOTOR_CONTROLLER */

#if CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER
    if (cam->cur_format && cam->cur_format->isp_info) {
        const esp_ipa_config_t *ipa_config = esp_ipa_pipeline_get_config(cam->name);
        if (ipa_config) {
            esp_video_isp_config_t isp_config = {
                .cam_dev = ESP_VIDEO_MIPI_CSI_DEVICE_NAME,
                .isp_dev = ESP_VIDEO_ISP1_DEVICE_NAME,
                .ipa_config = ipa_config
            };

            esp_err_t ret = esp_video_isp_pipeline_init(&isp_config);
            if (ret != ESP_OK) {
#if CONFIG_ESP_VIDEO_ENABLE_CAMERA_MOTOR_CONTROLLER
                if (motor_inited) {
                    esp_cam_motor_device_t *motor_dev = esp_video_get_csi_video_device_motor();
                    esp_sccb_del_i2c_io(motor_dev->sccb_handle);
                    esp_cam_motor_del_dev(motor_dev);
                    s_video_device_inited_flags &= ~ESP_VIDEO_INIT_FLAGS_MOTOR;
                }
#endif
                esp_video_destroy_csi_video_device();
                ESP_LOGE(TAG, "Failed to initialize ISP pipeline controller");
                return ret;
            }
        } else {
            ESP_LOGW(TAG, "failed to get configuration to initialize ISP controller");
        }
    }
#endif /* CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER */

    return ESP_OK;
}

static esp_err_t destroy_csi_video_device(void)
{
    esp_err_t ret = ESP_OK;

#if CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER
    if (esp_video_isp_pipeline_is_initialized()) {
        ESP_RETURN_ON_ERROR(esp_video_isp_pipeline_deinit(), TAG, "Failed to destroy ISP controller");
    }
#endif

    esp_cam_sensor_device_t *cam_dev = esp_video_get_csi_video_device_sensor();
    ESP_RETURN_ON_FALSE(cam_dev, ESP_ERR_INVALID_STATE, TAG, "CSI video device has no camera sensor");
    ESP_RETURN_ON_ERROR(esp_sccb_del_i2c_io(cam_dev->sccb_handle), TAG, "Failed to delete SCCB device");
    ESP_RETURN_ON_ERROR(destroy_cam_device(cam_dev), TAG, "Failed to delete CSI sensor");

#if CONFIG_ESP_VIDEO_ENABLE_CAMERA_MOTOR_CONTROLLER
    if (s_video_device_inited_flags & ESP_VIDEO_INIT_FLAGS_MOTOR) {
        ESP_RETURN_ON_ERROR(esp_cam_motor_del_dev(esp_video_get_csi_video_device_motor()), TAG, "Failed to delete CSI motor");
        s_video_device_inited_flags &= ~ESP_VIDEO_INIT_FLAGS_MOTOR;
    }
#endif /* CONFIG_ESP_VIDEO_ENABLE_CAMERA_MOTOR_CONTROLLER */

    ESP_RETURN_ON_ERROR(esp_video_destroy_csi_video_device(), TAG, "Failed to destroy CSI video device");

    return ret;
}
#endif /* CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE */

#if CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE
static esp_err_t init_dvp_clk_func(void *priv)
{
    esp_err_t ret = ESP_OK;
    const esp_video_init_dvp_config_t *dvp_config = priv;
    int dvp_ctlr_id = 0;

    ESP_RETURN_ON_ERROR(esp_cam_ctlr_dvp_init_ext(dvp_ctlr_id, CAM_CLK_SRC_DEFAULT, &dvp_config->dvp_pin), TAG, "Failed to initialize DVP");
    if (dvp_config->dvp_pin.xclk_io >= 0 && dvp_config->xclk_freq > 0) {
        ESP_GOTO_ON_ERROR(esp_cam_ctlr_dvp_output_clock(dvp_ctlr_id, CAM_CLK_SRC_DEFAULT, dvp_config->xclk_freq), fail_0, TAG, "Failed to set DVP output clock frequency");
    }

    return ESP_OK;

fail_0:
    esp_cam_ctlr_dvp_deinit(dvp_ctlr_id);
    return ret;
}

static esp_err_t deinit_dvp_clk_func(void *priv)
{
    int dvp_ctlr_id = 0;

    ESP_RETURN_ON_ERROR(esp_cam_ctlr_dvp_deinit(dvp_ctlr_id), TAG, "Failed to deinitialize DVP");
    return ESP_OK;
}

static esp_err_t create_dvp_video_device(esp_cam_sensor_device_t *cam, void *priv)
{
    return esp_video_create_dvp_video_device(cam);
}

static esp_err_t destroy_dvp_video_device(void)
{
    int dvp_ctlr_id = 0;
    esp_cam_sensor_device_t *cam_dev = esp_video_get_dvp_video_device_sensor();
    ESP_RETURN_ON_FALSE(cam_dev, ESP_ERR_INVALID_STATE, TAG, "DVP video device has no camera sensor");
    ESP_RETURN_ON_ERROR(esp_sccb_del_i2c_io(cam_dev->sccb_handle), TAG, "Failed to delete SCCB device");
    ESP_RETURN_ON_ERROR(destroy_cam_device(cam_dev), TAG, "Failed to delete DVP sensor");
    ESP_RETURN_ON_ERROR(esp_video_destroy_dvp_video_device(), TAG, "Failed to destroy DVP video device");
    ESP_RETURN_ON_ERROR(esp_cam_ctlr_dvp_deinit(dvp_ctlr_id), TAG, "Failed to deinit DVP port");

    return ESP_OK;
}
#endif /* CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE */
#endif /* CONFIG_ESP_VIDEO_ENABLE_SCCB_DEVICE */

#if CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE
static void usb_lib_task(void *arg)
{
    ESP_LOGD(TAG, "USB Host installed");

    while (1) {
        uint32_t event_flags;

        if (usb_host_lib_handle_events(portMAX_DELAY, &event_flags) == ESP_OK) {
            if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
                if (usb_host_device_free_all() == ESP_OK) {
                    ESP_LOGI(TAG, "USB: All devices freed");
                    break;
                } else {
                    ESP_LOGD(TAG, "USB: Wait for the FLAGS_ALL_FREE");
                }
            }

            if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
                ESP_LOGD(TAG, "USB: All devices freed");
                break;
            }
        } else {
            ESP_LOGE(TAG, "Failed to handle USB events");
            break;
        }
    }

    vTaskDelay(10); // Short delay to allow clients clean-up
    if (usb_host_uninstall() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to uninstall USB Host driver");
    }
    xSemaphoreGive(s_usb_uvc_sem);
    vTaskDelete(NULL);
}

static esp_err_t create_usb_uvc_video_device(const esp_video_init_usb_uvc_config_t *config)
{
    TaskHandle_t usb_lib_task_handler = NULL;

    if (config->usb.init_usb_host_lib) {
        BaseType_t usb_task_created = pdFALSE;
        int task_affinity = config->usb.task_affinity >= 0 ? config->usb.task_affinity : tskNO_AFFINITY;

        ESP_LOGD(TAG, "Installing USB Host");

        s_usb_uvc_sem = xSemaphoreCreateBinary();
        ESP_RETURN_ON_FALSE(s_usb_uvc_sem, ESP_ERR_NO_MEM, TAG, "Failed to create USB UVC semaphore");

        usb_host_config_t host_config = {
            .skip_phy_setup = false,
            .intr_flags = ESP_INTR_FLAG_LEVEL1,
            .peripheral_map = config->usb.peripheral_map,
        };
        if (usb_host_install(&host_config) != ESP_OK) {
            vSemaphoreDelete(s_usb_uvc_sem);
            ESP_LOGE(TAG, "Failed to install USB Host driver");
            return ESP_FAIL;
        }

        usb_task_created = xTaskCreatePinnedToCore(
                               usb_lib_task,
                               "usb_lib",
                               config->usb.task_stack,
                               NULL,
                               config->usb.task_priority,
                               &usb_lib_task_handler,
                               task_affinity);
        if (usb_task_created != pdTRUE) {
            usb_host_uninstall();
            vSemaphoreDelete(s_usb_uvc_sem);
            ESP_LOGE(TAG, "Failed to create USB host library task");
            return ESP_FAIL;
        }
    }

    esp_video_usb_uvc_device_config_t cfg = {
        .uvc_dev_num = config->uvc.uvc_dev_num,
        .task_stack = config->uvc.task_stack,
        .task_priority = config->uvc.task_priority,
        .task_affinity = config->uvc.task_affinity,
    };

    if (esp_video_install_usb_uvc_driver(&cfg) != ESP_OK) {
        if (usb_lib_task_handler) {
            vTaskDelete(usb_lib_task_handler);
            usb_host_uninstall();
            vSemaphoreDelete(s_usb_uvc_sem);
        }
        ESP_LOGE(TAG, "Failed to install USB UVC driver");
        return ESP_FAIL;
    }

    return ESP_OK;
}
#endif /* CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE */

/**
 * @brief Deinitialize video hardware and software, including I2C, MIPI CSI and so on with flags.
 *
 * @param flags video device flags, which can be a combination of ESP_VIDEO_INIT_FLAGS_XXX
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 *
 * @note This function will deinitialize the video hardware and software in the order of JPEG, H.264, MIPI CSI, DVP, SPI, USB UVC, ISP.
 */
esp_err_t esp_video_deinit_with_flags(uint32_t flags)
{
    esp_err_t ret = ESP_OK;

    _lock_acquire_recursive(&s_init_lock);

#if CONFIG_ESP_VIDEO_ENABLE_HW_JPEG_VIDEO_DEVICE
    if (flags & ESP_VIDEO_INIT_FLAGS_JPEG) {
        if (s_video_device_inited_flags & ESP_VIDEO_INIT_FLAGS_JPEG) {
            ESP_GOTO_ON_ERROR(esp_video_destroy_jpeg_video_device(), fail0, TAG, "Failed to deinitialize hardware JPEG video device");
            s_video_device_inited_flags &= ~ESP_VIDEO_INIT_FLAGS_JPEG;
        } else {
            ESP_LOGD(TAG, "hardware JPEG video device is not initialized");
        }
    }
#endif

#if CONFIG_ESP_VIDEO_ENABLE_HW_H264_VIDEO_DEVICE
    if (flags & ESP_VIDEO_INIT_FLAGS_H264) {
        if (s_video_device_inited_flags & ESP_VIDEO_INIT_FLAGS_H264) {
            ESP_GOTO_ON_ERROR(esp_video_destroy_h264_video_device(true), fail0, TAG, "Failed to deinitialize hardware H.264 video device");
            s_video_device_inited_flags &= ~ESP_VIDEO_INIT_FLAGS_H264;
        } else {
            ESP_LOGD(TAG, "hardware H.264 video device is not initialized");
        }
    }
#endif

#if CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE
    if (flags & ESP_VIDEO_INIT_FLAGS_MIPI_CSI) {
        if (s_video_device_inited_flags & ESP_VIDEO_INIT_FLAGS_MIPI_CSI) {
            ESP_GOTO_ON_ERROR(destroy_csi_video_device(), fail0, TAG, "Failed to deinitialize MIPI CSI video device");
            s_video_device_inited_flags &= ~ESP_VIDEO_INIT_FLAGS_MIPI_CSI;
        } else {
            ESP_LOGD(TAG, "MIPI CSI video device is not initialized");
        }
    }
#endif

#if CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE
    if (flags & ESP_VIDEO_INIT_FLAGS_DVP) {
        if (s_video_device_inited_flags & ESP_VIDEO_INIT_FLAGS_DVP) {
            ESP_GOTO_ON_ERROR(destroy_dvp_video_device(), fail0, TAG, "Failed to deinitialize DVP video device");
            s_video_device_inited_flags &= ~ESP_VIDEO_INIT_FLAGS_DVP;
        } else {
            ESP_LOGD(TAG, "DVP video device is not initialized");
        }
    }
#endif

#if CONFIG_ESP_VIDEO_ENABLE_SPI_VIDEO_DEVICE
    if (flags & ESP_VIDEO_INIT_FLAGS_SPI) {
        if (s_video_device_inited_flags & ESP_VIDEO_INIT_FLAGS_SPI) {
            ESP_GOTO_ON_ERROR(destroy_spi_video_device(), fail0, TAG, "Failed to deinitialize SPI video device");
            s_video_device_inited_flags &= ~ESP_VIDEO_INIT_FLAGS_SPI;
        } else {
            ESP_LOGD(TAG, "SPI video device is not initialized");
        }
    }
#endif

#if CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE
    if (flags & ESP_VIDEO_INIT_FLAGS_USB_UVC) {
        if (s_video_device_inited_flags & ESP_VIDEO_INIT_FLAGS_USB_UVC) {
            ESP_GOTO_ON_ERROR(esp_video_uninstall_usb_uvc_driver(), fail0, TAG, "Failed to deinitialize USB UVC driver");
            if (s_usb_uvc_sem) {
                xSemaphoreTake(s_usb_uvc_sem, portMAX_DELAY);
                vSemaphoreDelete(s_usb_uvc_sem);
                s_usb_uvc_sem = NULL;
            }
            s_video_device_inited_flags &= ~ESP_VIDEO_INIT_FLAGS_USB_UVC;
        } else {
            ESP_LOGD(TAG, "USB UVC video device is not initialized");
        }
    }
#endif

#if CONFIG_ESP_VIDEO_ENABLE_ISP_VIDEO_DEVICE
    if (flags & ESP_VIDEO_INIT_FLAGS_ISP) {
        if (s_video_device_inited_flags & ESP_VIDEO_INIT_FLAGS_ISP) {
            ESP_GOTO_ON_ERROR(esp_video_destroy_isp_video_device(), fail0, TAG, "Failed to deinitialize ISP video device");
            s_video_device_inited_flags &= ~ESP_VIDEO_INIT_FLAGS_ISP;
        } else {
            ESP_LOGD(TAG, "ISP video device is not initialized");
        }
    }
#endif

fail0:
#if ESP_VIDEO_ENABLE_SCCB_DEVICE
    /**
     * Deinitialize I2C device internal object
     */
    for (int i = 0; i < I2C_NUM_MAX; i++) {
        if (s_i2c_dev[i].i2c_handle) {
            i2c_del_master_bus(s_i2c_dev[i].i2c_handle);
            s_i2c_dev[i].i2c_handle = NULL;
        }
    }
    memset(s_i2c_dev, 0, sizeof(s_i2c_dev));
#endif

#if CONFIG_ESP_VIDEO_ENABLE_SPI_VIDEO_DEVICE
    /**
     * Deinitialize SPI device internal object
     */
    for (int i = 0; i < ESP_VIDEO_SPI_DEVICE_NUM; i++) {
        if (s_spi_dev[i].xclk_handle) {
            esp_cam_sensor_xclk_stop(s_spi_dev[i].xclk_handle);
            esp_cam_sensor_xclk_free(s_spi_dev[i].xclk_handle);
            s_spi_dev[i].xclk_handle = NULL;
        }
    }
    memset(s_spi_dev, 0, sizeof(s_spi_dev));
#endif

    _lock_release_recursive(&s_init_lock);
    return ret;
}

/**
 * @brief Initialize video hardware and software, including I2C, MIPI CSI and so on with flags.
 *
 * @param config video hardware configuration
 * @param flags video device flags, which can be a combination of ESP_VIDEO_INIT_FLAGS_XXX
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_init_with_flags(const esp_video_init_config_t *config, uint32_t flags)
{
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Invalid video hardware configuration");

    _lock_acquire_recursive(&s_init_lock);

#if CONFIG_ESP_VIDEO_ENABLE_ISP_VIDEO_DEVICE
    if (flags & ESP_VIDEO_INIT_FLAGS_ISP) {
        if (!(s_video_device_inited_flags & ESP_VIDEO_INIT_FLAGS_ISP)) {
            ESP_GOTO_ON_ERROR(esp_video_create_isp_video_device(), fail0, TAG, "Failed to create hardware ISP video device");
            s_video_device_inited_flags |= ESP_VIDEO_INIT_FLAGS_ISP;
        } else {
            ESP_LOGW(TAG, "ISP video device is already initialized");
        }
    }
#endif

#if CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE
    if (flags & ESP_VIDEO_INIT_FLAGS_USB_UVC) {
        if (!(s_video_device_inited_flags & ESP_VIDEO_INIT_FLAGS_USB_UVC)) {
            ESP_GOTO_ON_ERROR(create_usb_uvc_video_device(config->usb_uvc), fail1, TAG, "Failed to create USB UVC video device");
            s_video_device_inited_flags |= ESP_VIDEO_INIT_FLAGS_USB_UVC;
        } else {
            ESP_LOGW(TAG, "USB UVC video device is already initialized");
        }
    }
#endif

    for (esp_cam_sensor_detect_fn_t *p = &__esp_cam_sensor_detect_fn_array_start; p < &__esp_cam_sensor_detect_fn_array_end; ++p) {
#if CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE
        if (flags & ESP_VIDEO_INIT_FLAGS_MIPI_CSI) {
            if (!(s_video_device_inited_flags & ESP_VIDEO_INIT_FLAGS_MIPI_CSI) && p->port == ESP_CAM_SENSOR_MIPI_CSI && config->csi != NULL) {
                uint8_t device_inited = 0;
                const esp_video_init_csi_config_t *csi = config->csi;
                video_device_init_config_t csi_init_config = {
                    .sccb_config = &csi->sccb_config,
                    .sensor_cfg = {
                        .reset_pin = csi->reset_pin,
                        .pwdn_pin = csi->pwdn_pin,
                        .detect = p,
                    },
                    .create_func = create_csi_video_device,
                    .create_priv = (void *)config,
                    .device_inited = &device_inited,
                };

                ESP_GOTO_ON_ERROR(esp_video_init_sensor_and_video_device(&csi_init_config),
                                  fail1, TAG, "Failed to initialize MIPI CSI video device");
                if (device_inited) {
                    s_video_device_inited_flags |= ESP_VIDEO_INIT_FLAGS_MIPI_CSI;
                }
            }
        }
#endif /* CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE */
#if CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE
        if (flags & ESP_VIDEO_INIT_FLAGS_DVP) {
            if (!(s_video_device_inited_flags & ESP_VIDEO_INIT_FLAGS_DVP) && p->port == ESP_CAM_SENSOR_DVP && config->dvp != NULL) {
                uint8_t device_inited = 0;
                const esp_video_init_dvp_config_t *dvp = config->dvp;
                video_device_init_config_t dvp_init_config = {
                    .sccb_config = &dvp->sccb_config,
                    .sensor_cfg = {
                        .reset_pin = dvp->reset_pin,
                        .pwdn_pin = dvp->pwdn_pin,
                        .detect = p,
                    },
                    .create_func = create_dvp_video_device,
                    .create_priv = (void *)dvp,
                    .init_clk_func = init_dvp_clk_func,
                    .deinit_clk_func = deinit_dvp_clk_func,
                    .clk_priv = (void *)dvp,
                    .device_inited = &device_inited,
                };

                ESP_GOTO_ON_ERROR(esp_video_init_sensor_and_video_device(&dvp_init_config),
                                  fail1, TAG, "Failed to initialize DVP video device");
                if (device_inited) {
                    s_video_device_inited_flags |= ESP_VIDEO_INIT_FLAGS_DVP;
                }
            }
        }
#endif /* CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE */
#if CONFIG_ESP_VIDEO_ENABLE_SPI_VIDEO_DEVICE
        if (flags & ESP_VIDEO_INIT_FLAGS_SPI) {
            if (!(s_video_device_inited_flags & ESP_VIDEO_INIT_FLAGS_SPI) && p->port == ESP_CAM_SENSOR_SPI && config->spi != NULL) {
                uint8_t device_inited = 0;
                const esp_video_init_spi_config_t *spi = config->spi;
                int index = -1;
                for (int i = 0; i < ESP_VIDEO_SPI_DEVICE_NUM; i++) {
                    if (s_spi_dev[i].xclk_handle == NULL) {
                        index = i;
                        break;
                    }
                }
                if (index < 0) {
                    ESP_LOGE(TAG, "No available SPI device slot");
                    continue;
                }
                spi_video_device_init_clk_config_t spi_clk_common_config = {
                    .index = index,
                    .xclk_pin = spi->xclk_pin,
                    .xclk_freq_hz = spi->xclk_freq,
                    .xclk_source = spi->xclk_source,
#if CONFIG_CAMERA_XCLK_USE_LEDC
                    .xclk_ledc_cfg = {
                        .timer = spi->xclk_ledc_cfg.timer,
                        .clk_cfg = spi->xclk_ledc_cfg.clk_cfg,
                        .channel = spi->xclk_ledc_cfg.channel,
                    }
#endif
                };
                spi_video_device_init_config_t spi_device_init_config = {
                    .index = index,
                    .spi_config = spi,
                };
                video_device_init_config_t spi_init_config = {
                    .sensor_cfg = {
                        .reset_pin = spi->reset_pin,
                        .pwdn_pin = spi->pwdn_pin,
                        .detect = p,
                    },
                    .sccb_config = &spi->sccb_config,
                    .create_func = create_spi_video_device,
                    .create_priv = &spi_device_init_config,
                    .init_clk_func = init_spi_clk_func,
                    .deinit_clk_func = deinit_spi_clk_func,
                    .clk_priv = &spi_clk_common_config,
                    .device_inited = &device_inited,
                };

                ESP_GOTO_ON_ERROR(esp_video_init_sensor_and_video_device(&spi_init_config),
                                  fail1, TAG, "Failed to initialize SPI video device");
                if (device_inited) {
                    s_video_device_inited_flags |= ESP_VIDEO_INIT_FLAGS_SPI;
                }
            }
        }
#endif /* CONFIG_ESP_VIDEO_ENABLE_SPI_VIDEO_DEVICE */
    }

#if CONFIG_ESP_VIDEO_ENABLE_HW_H264_VIDEO_DEVICE
    if (flags & ESP_VIDEO_INIT_FLAGS_H264) {
        if (!(s_video_device_inited_flags & ESP_VIDEO_INIT_FLAGS_H264)) {
            ESP_GOTO_ON_ERROR(esp_video_create_h264_video_device(true), fail1, TAG, "Failed to create hardware H.264 video device");
            s_video_device_inited_flags |= ESP_VIDEO_INIT_FLAGS_H264;
        } else {
            ESP_LOGW(TAG, "H.264 video device is already initialized");
        }
    }
#endif

#if CONFIG_ESP_VIDEO_ENABLE_HW_JPEG_VIDEO_DEVICE
    if (flags & ESP_VIDEO_INIT_FLAGS_JPEG) {
        if (!(s_video_device_inited_flags & ESP_VIDEO_INIT_FLAGS_JPEG)) {
            jpeg_encoder_handle_t handle = config->jpeg ? config->jpeg->enc_handle : NULL;

            ESP_GOTO_ON_ERROR(esp_video_create_jpeg_video_device(handle), fail1, TAG, "Failed to create hardware JPEG video device");
            s_video_device_inited_flags |= ESP_VIDEO_INIT_FLAGS_JPEG;
        } else {
            ESP_LOGW(TAG, "JPEG video device is already initialized");
        }
    }
#endif

    _lock_release_recursive(&s_init_lock);
    return ESP_OK;

fail1:
    esp_video_deinit_with_flags(s_video_device_inited_flags);
#if CONFIG_ESP_VIDEO_ENABLE_ISP_VIDEO_DEVICE
fail0:
#endif
    _lock_release_recursive(&s_init_lock);
    return ret;
}

/**
 * @brief Initialize video hardware and software, including I2C, MIPI CSI and so on.
 *
 * @param config video hardware configuration
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 *
 * @note This function will initialize the video hardware and software with all flags.
 *       It is equivalent to calling esp_video_init_with_flags(config, ESP_VIDEO_INIT_FLAGS_ALL).
 */
esp_err_t esp_video_init(const esp_video_init_config_t *config)
{
    return esp_video_init_with_flags(config, ESP_VIDEO_INIT_FLAGS_ALL);
}

/**
 * @brief Deinitialize video hardware and software, including I2C, MIPI CSI and so on.
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 *
 * @note This function will deinitialize the video hardware and software with all flags.
 *       It is equivalent to calling esp_video_deinit_with_flags(ESP_VIDEO_INIT_FLAGS_ALL).
 */
esp_err_t esp_video_deinit(void)
{
    return esp_video_deinit_with_flags(ESP_VIDEO_INIT_FLAGS_ALL);
}
