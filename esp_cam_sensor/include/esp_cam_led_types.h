/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include "esp_cam_sensor_types.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LED hardware type
 */
typedef enum {
    ESP_CAM_LED_HW_TYPE_GPIO  = 0,
    ESP_CAM_LED_HW_TYPE_LEDC  = 1,
    ESP_CAM_LED_HW_TYPE_I2C  = 2,
} esp_cam_led_hw_type_t;

/**
 * @brief LED spectrum response type
 */
typedef enum {
    ESP_CAM_LED_SPECTRUM_VISIBLE  = 0,
    ESP_CAM_LED_SPECTRUM_IR  = 1,
    ESP_CAM_LED_SPECTRUM_UV  = 2,
} esp_cam_led_spectrum_type_t;

/**
 * @brief LED device info
 */
typedef struct {
    esp_cam_led_hw_type_t type;           /* Hardware type */
    esp_cam_led_spectrum_type_t spectrum; /* Spectrum response type */
    uint32_t   brightness_range_min;  /* Minimum measurable brightness */
    uint32_t   brightness_range_max;  /* Maximum measurable brightness */
} esp_cam_led_dev_info_t;

/**
 * @brief LED error code
 */
#define ESP_CAM_LED_ERR_OFFSET                    0x2200 // todo, synchronize the IR-CUT and AF motor serial numbers to esp_cam_sensor_type.h.
#define ESP_CAM_LED_ERR_BASE                      ESP_CAM_SENSOR_ERR_BASE + ESP_CAM_LED_ERR_OFFSET
#define ESP_CAM_LED_ERR_NOT_DETECTED             (ESP_CAM_LED_ERR_BASE + 1)
#define ESP_CAM_LED_ERR_NOT_SUPPORTED            (ESP_CAM_LED_ERR_BASE + 2)
#define ESP_CAM_LED_ERR_BUSY                     (ESP_CAM_LED_ERR_BASE + 3)

/*
 * @brief LED command
 */
#define ESP_CAM_LED_IOC_NUM                      0x10
#define ESP_CAM_LED_IOC_BASE                     ESP_CAM_SENSOR_IOC_MAX + 0x40 // todo, synchronize the IR-CUT and AF motor serial numbers to esp_cam_sensor_type.h.
#define ESP_CAM_LED_IOCTL_GET_BRIGHTNESS         ESP_CAM_SENSOR_IOC(ESP_CAM_LED_IOC_BASE, sizeof(uint32_t)) /*!< Read brightness value（_IOR） */
#define ESP_CAM_LED_IOCTL_SET_BRIGHTNESS         ESP_CAM_SENSOR_IOC(ESP_CAM_LED_IOC_BASE + 0x01, sizeof(uint32_t)) /*!< Set brightness value（_IOW） */
#define ESP_CAM_LED_IOCTL_GET_INFO               ESP_CAM_SENSOR_IOC(ESP_CAM_LED_IOC_BASE + 0x02, sizeof(esp_cam_led_dev_info_t)) /*!< Get device information（range/precision/type）（_IOR） */
#define ESP_CAM_LED_IOC_MAX                      ESP_CAM_SENSOR_IOC(ESP_CAM_LED_IOC_BASE + ESP_CAM_LED_IOC_NUM, 0)

/**
 * @brief LED configuration
 */
typedef struct {
    esp_cam_led_hw_type_t type;           /* Hardware type */
    union {
        struct {
            gpio_num_t gpio_ctrl;      /*!< GPIO control pin, set to -1 if not used */
            gpio_num_t gpio_en;      /*!< GPIO enable pin, set to -1 if not used */
        };
        struct {
            esp_sccb_io_handle_t sccb_handle;     /* the handle of the sccb bus bound to the sensor, returned by sccb_new_i2c_io */
        };
    };
    void *platform_data;                         /*!< Platform specific data */
} esp_cam_led_config_t;

/**
 * @brief LED operations
 */
typedef struct _esp_cam_led_ops esp_cam_led_ops_t;

/**
 * @brief Type of LED device
 */
typedef struct {
    const char        *name;
    const esp_cam_led_dev_info_t *info;
    uint32_t          brightness;     /* Brightness value */
    union {
        struct {
            gpio_num_t gpio_ctrl;      /*!< GPIO control pin, set to -1 if not used */
            gpio_num_t gpio_en;      /*!< GPIO enable pin, set to -1 if not used */
        };
        struct {
            esp_sccb_io_handle_t sccb_handle;     /* the handle of the sccb bus bound to the sensor, returned by sccb_new_i2c_io */
        };
    };
    pthread_mutex_t lock;                             /*!< Mutex for concurrent access */
    const esp_cam_led_ops_t *ops;                     /*!< Pointer to the LED driver operation array. */
    void *reserved;
} esp_cam_led_device_t;

/**
 * @brief LED driver operation array
 */
typedef struct _esp_cam_led_ops {
    int (*priv_ioctl)(esp_cam_led_device_t *dev, uint32_t cmd, void *arg);
    int (*del)(esp_cam_led_device_t *dev);
} esp_cam_led_ops_t;

#ifdef __cplusplus
}
#endif
