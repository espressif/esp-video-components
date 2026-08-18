/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "linux/videodev2.h"
#include "esp_cam_sensor_types.h"
#include "esp_cam_motor_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Restart video configuration.
 */
struct v4l2_restart_config {
    int type;                   /*!< enum v4l2_buf_type */
    bool restart_sensor;        /*!< Restart sensor */
};

/**
 * @brief Event callback function.
 *
 * @note The callback function must be in IRAM, any functions which may block must not be called in the callback
 *       function, such as malloc, free, printf, etc. Otherwise, the system may crash.
 *
 * @param callback_func Callback function, if return true, the hardware interrupt will be disabled
 *                      and send event V4L2_EVENT_ESP_MIPI_CSI_INTERRUPT_DISABLE to user, otherwise do nothing
 * @param user_data     User data for callback function
 */
struct v4l2_event_callback {
    bool (*callback_func)(void *user_data, uint32_t err_mask);
    void *user_data;
};

#define V4L2_FMT_STR                    "%c%c%c%c"
#define V4L2_FMT_STR_ARG(fmt)           (uint8_t)(((fmt) >> 0)  & 0xFF), \
                                        (uint8_t)(((fmt) >> 8)  & 0xFF), \
                                        (uint8_t)(((fmt) >> 16) & 0xFF), \
                                        (uint8_t)(((fmt) >> 24) & 0xFF)

#define VIDIOC_S_SENSOR_FMT _IOWR('V',  BASE_VIDIOC_PRIVATE + 1, esp_cam_sensor_format_t)
#define VIDIOC_G_SENSOR_FMT _IOWR('V',  BASE_VIDIOC_PRIVATE + 2, esp_cam_sensor_format_t)

#define VIDIOC_SET_OWNER    _IOWR('V',  BASE_VIDIOC_PRIVATE + 3, int)

#define VIDIOC_S_MOTOR_FMT  _IOWR('V',  BASE_VIDIOC_PRIVATE + 4, esp_cam_motor_format_t)
#define VIDIOC_G_MOTOR_FMT  _IOWR('V',  BASE_VIDIOC_PRIVATE + 5, esp_cam_motor_format_t)

#define VIDIOC_S_DQBUF_TIMEOUT  _IOWR('V',  BASE_VIDIOC_PRIVATE + 6, struct timeval)
#define VIDIOC_G_DQBUF_TIMEOUT  _IOWR('V',  BASE_VIDIOC_PRIVATE + 7, struct timeval)

/**
 * @brief Restart video hardware
 *
 * @note Only MIPI-CSI video device support this command
 * @note It must be called after video device is streamed on and before video device is stopped
 *
 * @param config    Restart configuration
 */
#define VIDIOC_RESTART      _IOWR('V',  BASE_VIDIOC_PRIVATE + 8, struct v4l2_restart_config)

/**
 * @brief Set event callback
 *
 * @param callback  Event callback structure
 */
#define VIDIOC_S_EVENT_CALLBACK  _IOW('V',  BASE_VIDIOC_PRIVATE + 9, struct v4l2_event_callback)

#define V4L2_CID_CAMERA_AE_LEVEL        (V4L2_CID_CAMERA_CLASS_BASE + 40)
#define V4L2_CID_CAMERA_STATS           (V4L2_CID_CAMERA_CLASS_BASE + 41)
#define V4L2_CID_CAMERA_GROUP           (V4L2_CID_CAMERA_CLASS_BASE + 42)
#define V4L2_CID_MOTOR_START_TIME       (V4L2_CID_CAMERA_CLASS_BASE + 43)

/**
 * @brief Use this class to call esp_cam_sensor ioctl commands directly, this is only
 * used for camera sensor, not for motor controller.
 *
 * @note Please note that this class only supports "p_u8" and "size" fields of v4l2_ext_control,
 * other fields are not supported.
 */
#define V4L2_CTRL_CLASS_ESP_CAM_IOCTL   (0x00a70000)

/**
 * @brief BGR565 format for JPEG decoder video device.
 */
#define V4L2_PIX_FMT_BGR565             v4l2_fourcc('B', 'G', 'R', 'P')


/**
 * @brief Video event unsubscribed event.
 */
#define V4L2_EVENT_ESP_VIDEO_EVENT_UNSUBSCRIBED (V4L2_EVENT_PRIVATE_START + 1)

/**
 * @brief MIPI CSI error event.
 *
 * This event is used to report MIPI CSI error.
 */
#define V4L2_EVENT_ESP_MIPI_CSI_ERROR   (V4L2_EVENT_PRIVATE_START + 2)

#define ESP_MIPI_CSI_HOST_ERR_PHY       (1U << 0)    /*!< PHY error. */
#define ESP_MIPI_CSI_HOST_ERR_PACKET    (1U << 1)    /*!< Packet error. */
#define ESP_MIPI_CSI_HOST_ERR_FRAME     (1U << 2)    /*!< Frame boundary or sequence error. */
#define ESP_MIPI_CSI_HOST_ERR_CRC       (1U << 3)    /*!< Frame or payload CRC error. */
#define ESP_MIPI_CSI_HOST_ERR_DATA_ID   (1U << 4)    /*!< Unrecognized or unsupported data type detected. */

/**
 * @brief MIPI CSI interrupt disable event.
 *
 * @note This event should not be subscribed by users, it is used internally by driver
 * @note Hardware interrupt will be disabled when this event is received, so users should restart the hardware if needed
 *
 * This event is used to report MIPI CSI interrupt disable.
 */
#define V4L2_EVENT_ESP_MIPI_CSI_INTERRUPT_DISABLE (V4L2_EVENT_PRIVATE_START + 3)

/**
 * @brief MIPI CSI error event data.
 */
typedef struct v4l2_event_esp_mipi_csi_error {
    uint32_t host_err_mask;          /*!< Host error mask */
} v4l2_event_esp_mipi_csi_error_t;

#ifdef __cplusplus
}
#endif
