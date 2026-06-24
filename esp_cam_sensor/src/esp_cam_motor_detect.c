/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"
#include "esp_cam_motor_detect.h"

#if CONFIG_CAMERA_SENSOR_MOTOR_DETECT_METHOD_STATIC_STORE

#if CONFIG_CAM_MOTOR_DW9714
#include "dw9714.h"
#endif

#define ESP_CAM_MOTOR_DETECT_ENTRY(f, i, j) \
    { .detect = __esp_cam_motor_detect_fn_##f, .cam_info = (i), .sccb_addr = (j) }

#define ESP_CAM_MOTOR_DETECT_DECLARE(f, i) \
    esp_cam_motor_device_t *__esp_cam_motor_detect_fn_##f(void *config)

#if CONFIG_CAM_MOTOR_DW9714_AUTO_DETECT
ESP_CAM_MOTOR_DETECT_DECLARE(dw9714_detect, NULL);
#endif

static const esp_cam_motor_detect_fn_t __esp_cam_motor_detect_fn_array_start[] = {
#if CONFIG_CAM_MOTOR_DW9714_AUTO_DETECT
    ESP_CAM_MOTOR_DETECT_ENTRY(dw9714_detect, NULL, DW9714_SCCB_ADDR),
#endif
};

#endif /* CONFIG_CAMERA_SENSOR_MOTOR_DETECT_METHOD_STATIC_STORE */

/**
 * @brief Get the array of camera motor detect functions.
 *
 * @param array_start_ptr Pointer to the start of the array.
 * @param array_end_ptr Pointer to the end of the array.
 */
void esp_cam_motor_detect_get_array(esp_cam_motor_detect_fn_t **array_start_ptr, esp_cam_motor_detect_fn_t **array_end_ptr)
{
#if CONFIG_CAMERA_SENSOR_MOTOR_DETECT_METHOD_STATIC_STORE
    int size = sizeof(__esp_cam_motor_detect_fn_array_start) / sizeof(__esp_cam_motor_detect_fn_array_start[0]);

    *array_start_ptr = (esp_cam_motor_detect_fn_t *)__esp_cam_motor_detect_fn_array_start;
    *array_end_ptr = (esp_cam_motor_detect_fn_t *)(__esp_cam_motor_detect_fn_array_start + size);
#else
    *array_start_ptr = (esp_cam_motor_detect_fn_t *)&__esp_cam_motor_detect_fn_array_start;
    *array_end_ptr = (esp_cam_motor_detect_fn_t *)&__esp_cam_motor_detect_fn_array_end;
#endif
}
