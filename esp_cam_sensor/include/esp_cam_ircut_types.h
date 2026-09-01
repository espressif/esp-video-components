/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_cam_sensor_types.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESP_CAM_IRCUT_MODE_UNKNOWN = 0,
    ESP_CAM_IRCUT_MODE_DAY = 1,              /*!< Day mode */
    ESP_CAM_IRCUT_MODE_NIGHT = 2,            /*!< Night mode */
} esp_cam_ircut_mode_t;

/**
 * @brief Camera IRCUT hardware type
 */
typedef enum {
    ESP_CAM_IRCUT_HW_GPIO_DIRECT = 0,  /*!< GPIO direct control */
    ESP_CAM_IRCUT_HW_SINGLE_LINE = 1,  /*!< AP1511 series special driver IC (single line control) */
    ESP_CAM_IRCUT_HW_DOUBLE_LINE = 2,  /*!< AP1511 series special driver IC (double line control) */
} esp_cam_ircut_hw_type_t;

/**
 * @brief Camera IRCUT error code
 */
#define ESP_CAM_IRCUT_ERR_OFFSET                    0x2000
#define ESP_CAM_IRCUT_ERR_BASE                      ESP_CAM_SENSOR_ERR_BASE + ESP_CAM_IRCUT_ERR_OFFSET
#define ESP_CAM_IRCUT_ERR_NOT_DETECTED             (ESP_CAM_IRCUT_ERR_BASE + 1)
#define ESP_CAM_IRCUT_ERR_NOT_SUPPORTED            (ESP_CAM_IRCUT_ERR_BASE + 2)
#define ESP_CAM_IRCUT_ERR_FAILED_RESET             (ESP_CAM_IRCUT_ERR_BASE + 3)

/*
 * @brief Camera IRCUT command
 */
#define ESP_CAM_IRCUT_IOC_NUM                      0x10
#define ESP_CAM_IRCUT_IOC_BASE                     ESP_CAM_SENSOR_IOC_MAX + 0x20
#define ESP_CAM_IRCUT_IOC_HW_POWER_ON              ESP_CAM_SENSOR_IOC(ESP_CAM_IRCUT_IOC_BASE, sizeof(int)) /*!< Hardware power on */
#define ESP_CAM_IRCUT_IOC_SET_MODE                 ESP_CAM_SENSOR_IOC(ESP_CAM_IRCUT_IOC_BASE + 0x01, sizeof(esp_cam_ircut_mode_t)) /*!< Set IRCUT mode */
#define ESP_CAM_IRCUT_IOC_GET_MODE                 ESP_CAM_SENSOR_IOC(ESP_CAM_IRCUT_IOC_BASE + 0x02, sizeof(esp_cam_ircut_mode_t)) /*!< Get IRCUT mode */
#define ESP_CAM_IRCUT_IOC_FORCE_RST                ESP_CAM_SENSOR_IOC(ESP_CAM_IRCUT_IOC_BASE + 0x03, sizeof(esp_cam_ircut_mode_t)) /*!< Force reset IRCUT to solve stagnation problem */
#define ESP_CAM_IRCUT_IOC_GET_HW_TYPE              ESP_CAM_SENSOR_IOC(ESP_CAM_IRCUT_IOC_BASE + 0x04, sizeof(esp_cam_ircut_hw_type_t)) /*!< Get IRCUT hardware type */
#define ESP_CAM_IRCUT_IOC_MAX                      ESP_CAM_SENSOR_IOC(ESP_CAM_IRCUT_IOC_BASE + ESP_CAM_IRCUT_IOC_NUM, 0)

/**
 * @brief Camera IRCUT configuration
 */
typedef struct {
    gpio_num_t gpio_pwdn;    /*!< IRCUT PWDN pin number, Set to -1 if not used. */
    union {
        struct {
            gpio_num_t gpio_fbc;     /*!< IRCUT FBC pin number, used for forward and backward control. */
            gpio_num_t gpio_en;      /*!< IRCUT EN pin number, used for enable and disable control. */
        };
        struct {
            gpio_num_t gpio_a;      /*!< GPIO pin number, used for direct drive mode. */
            gpio_num_t gpio_b;      /*!< GPIO pin number, used for direct drive mode. */
        };
    };
    void *platform_data;     /*!< Platform specific data */
} esp_cam_ircut_config_t;

typedef struct _esp_cam_ircut_ops esp_cam_ircut_ops_t;

/**
 * @brief Type of camera IRCUT device
 */
typedef struct {
    char *name;                                  /*!< String name*/
    gpio_num_t gpio_pwdn;                        /*!< IRCUT PWDN pin number, Set to -1 if not used. */
    union {
        struct {
            gpio_num_t gpio_fbc;     /*!< IRCUT FBC pin number, used for forward and backward control. */
            gpio_num_t gpio_en;      /*!< IRCUT EN pin number, used for enable and disable control. */
        };
        struct {
            gpio_num_t gpio_a;      /*!< GPIO pin number, used for direct drive mode. */
            gpio_num_t gpio_b;      /*!< GPIO pin number, used for direct drive mode. */
        };
    };
    esp_cam_ircut_hw_type_t hw_type;             /*!< Hardware type */
    unsigned int pulse_width_ms;                 /*!< Pulse width in milliseconds */
    esp_cam_ircut_mode_t current_mode;           /*!< Current mode */
    pthread_mutex_t lock;                        /*!< Mutex for concurrent access */
    const esp_cam_ircut_ops_t *ops;              /*!< Pointer to the camera IRCUT driver operation array. */
    void *priv;                                  /*!< Private data */
} esp_cam_ircut_device_t;

typedef struct esp_cam_sensor_param_desc esp_cam_ircut_param_desc_t;

/**
 * @brief camera IRCUT driver operation array
 */
typedef struct _esp_cam_ircut_ops {
    int (*priv_ioctl)(esp_cam_ircut_device_t *dev, uint32_t cmd, void *arg);
    int (*del)(esp_cam_ircut_device_t *dev);
} esp_cam_ircut_ops_t;

#ifdef __cplusplus
}
#endif
