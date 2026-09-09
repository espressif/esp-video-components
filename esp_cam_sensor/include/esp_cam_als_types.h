/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_cam_sensor_types.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALS_TIMER_CREATE(name, period, cb, arg) \
    xTimerCreate(name, pdMS_TO_TICKS(period), pdTRUE, arg, cb)
#define ALS_TIMER_START(timer)      xTimerStart((TimerHandle_t)(timer), portMAX_DELAY)
#define ALS_TIMER_STOP(timer)       xTimerStop((TimerHandle_t)(timer), portMAX_DELAY)
#define ALS_TIMER_DELETE(timer)     xTimerDelete((TimerHandle_t)(timer), portMAX_DELAY)
#define ALS_TIMER_CHANGE_PERIOD(timer, period) xTimerChangePeriod((TimerHandle_t)(timer), pdMS_TO_TICKS(period), portMAX_DELAY)
#define ALS_DELAY_MS(ms)            vTaskDelay(((ms) > 0 && pdMS_TO_TICKS(ms) > 0) ? pdMS_TO_TICKS(ms) : 1)

/**
 * @brief Ambient Light Sensor hardware type
 */
typedef enum {
    ESP_CAM_ALS_HW_TYPE_ADC  = 0,  /*!< ADC sampling class（PT-560/CD5528 etc.） */
    ESP_CAM_ALS_HW_TYPE_I2C  = 1,  /*!< I2C digital class（BH1750/AP3216/TSL2561 etc.） */
} esp_cam_als_hw_type_t;

/**
 * @brief Ambient Light Sensor spectrum response type
 */
typedef enum {
    ALS_SPECTRUM_FULL      = 0,  /*!< Full spectrum response（including infrared） */
    ALS_SPECTRUM_VISIBLE   = 1,  /*!< Visible light response（infrared cutoff） */
} esp_cam_als_spectrum_type_t;

/**
 * @brief Ambient Light Sensor device info
 */
typedef struct {
    esp_cam_als_hw_type_t type;           /* Sensor type */
    esp_cam_als_spectrum_type_t spectrum; /* Spectrum response type */
    union {
        struct {
            uint32_t   raw_max;        /* Maximum raw value */
        };
        struct {
            uint32_t   lux_range_min;  /* Minimum measurable Lux (x100) */
            uint32_t   lux_range_max;  /* Maximum measurable Lux (x100) */
        };
    };
} esp_cam_als_dev_info_t;

/**
 * @brief Ambient Light Sensor error code
 */
#define ESP_CAM_ALS_ERR_OFFSET                    0x2100 // todo, synchronize the IR-CUT and AF motor serial numbers to esp_cam_sensor_type.h.
#define ESP_CAM_ALS_ERR_BASE                      ESP_CAM_SENSOR_ERR_BASE + ESP_CAM_ALS_ERR_OFFSET
#define ESP_CAM_ALS_ERR_NOT_DETECTED             (ESP_CAM_ALS_ERR_BASE + 1)
#define ESP_CAM_ALS_ERR_NOT_SUPPORTED            (ESP_CAM_ALS_ERR_BASE + 2)
#define ESP_CAM_ALS_ERR_BUSY                     (ESP_CAM_ALS_ERR_BASE + 3)

/*
 * @brief Ambient Light Sensor command
 */
#define ESP_CAM_ALS_IOC_NUM                      0x10
#define ESP_CAM_ALS_IOC_BASE                     ESP_CAM_SENSOR_IOC_MAX + 0x30 // todo, synchronize the IR-CUT and AF motor serial numbers to esp_cam_sensor_type.h.
#define ESP_CAM_ALS_IOCTL_GET_RAW                ESP_CAM_SENSOR_IOC(ESP_CAM_ALS_IOC_BASE, sizeof(uint32_t)) /*!< Read raw raw ADC(mV)/LUX(x100) value（_IOR） */
#define ESP_CAM_ALS_IOCTL_GET_RAW_FORCE_READ     ESP_CAM_SENSOR_IOC(ESP_CAM_ALS_IOC_BASE + 0x01, sizeof(uint32_t)) /*!< Force read raw ADC(mV)/LUX(x100) value（_IOR） */
#define ESP_CAM_ALS_IOCTL_SET_SAMPLE_RATE        ESP_CAM_SENSOR_IOC(ESP_CAM_ALS_IOC_BASE + 0x02, sizeof(uint16_t)) /*!< Set sampling period(ms)（_IOW） */
#define ESP_CAM_ALS_IOCTL_GET_SAMPLE_RATE        ESP_CAM_SENSOR_IOC(ESP_CAM_ALS_IOC_BASE + 0x03, sizeof(uint16_t)) /*!< Get sampling period(ms)（_IOR） */
#define ESP_CAM_ALS_IOCTL_GET_INFO               ESP_CAM_SENSOR_IOC(ESP_CAM_ALS_IOC_BASE + 0x04, sizeof(esp_cam_als_dev_info_t)) /*!< Get device information（range/precision/type）（_IOR） */
#define ESP_CAM_ALS_IOCTL_LOW_POWER_SWITCH       ESP_CAM_SENSOR_IOC(ESP_CAM_ALS_IOC_BASE + 0x05, sizeof(int)) /*!< Low power switch（_IOW） */
#define ESP_CAM_ALS_IOC_MAX                      ESP_CAM_SENSOR_IOC(ESP_CAM_ALS_IOC_BASE + ESP_CAM_ALS_IOC_NUM, 0)

/**
 * @brief Ambient Light Sensor configuration
 */
typedef struct {
    uint16_t          sample_rate_ms;     /* Sampling rate(ms) */
    union {
        struct {
            adc_unit_t unit;
            adc_oneshot_unit_handle_t adc_handle;
            adc_channel_t channel;
            adc_atten_t atten;
            adc_bitwidth_t bitwidth;
            bool cali_enable; /* Calibration enable, Todo, move this to other part and chis only use adc_handle and channel */
        };
        struct {
            esp_sccb_io_handle_t sccb_handle;     /* the handle of the sccb bus bound to the sensor, returned by sccb_new_i2c_io */
        };
    };
    void *platform_data;                         /*!< Platform specific data */
} esp_cam_als_config_t;

/**
 * @brief Ambient Light Sensor operations
 */
typedef struct _esp_cam_als_ops esp_cam_als_ops_t;

/**
 * @brief Type of Ambient Light Sensor device
 */
typedef struct {
    const char        *name;
    const esp_cam_als_dev_info_t *info;
    uint16_t          sample_rate_ms;     /* Sampling rate(ms) */
    TimerHandle_t     timer;              /* Timer handle */
    union {
        struct {
            adc_oneshot_unit_handle_t adc_handle;     /* the handle of the adc unit, returned by adc_oneshot_new_unit */
            adc_channel_t channel;                    /* the channel of the adc, returned by adc_oneshot_get_channel */
            adc_cali_handle_t cali_handle;            /* the handle of the adc calibration, returned by adc_cali_create_scheme_line_fitting */
            bool cali_enable;                         /* Calibration enable */
            uint32_t raw_value;                        /* Cached sample: ADC raw, or calibrated mV when cali_enable */
        };
        struct {
            esp_sccb_io_handle_t sccb_handle;         /* the handle of the sccb bus bound to the sensor, returned by sccb_new_i2c_io */
            float lux_value;                        /* the lux value of the sensor, returned by sccb_read */
        };
    };
    pthread_mutex_t lock;                             /*!< Mutex for concurrent access */
    const esp_cam_als_ops_t *ops;                     /*!< Pointer to the Ambient Light Sensor driver operation array. */
    void *reserved;
} esp_cam_als_device_t;

/**
 * @brief Ambient Light Sensor driver operation array
 */
typedef struct _esp_cam_als_ops {
    int (*priv_ioctl)(esp_cam_als_device_t *dev, uint32_t cmd, void *arg);
    int (*del)(esp_cam_als_device_t *dev);
} esp_cam_als_ops_t;

#ifdef __cplusplus
}
#endif
