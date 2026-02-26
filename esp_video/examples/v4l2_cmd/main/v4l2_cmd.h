/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_video_device.h"
#include "example_video_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR
#define V4L2_CTL_DEFAULT_DEV    ESP_VIDEO_DVP_DEVICE_NAME
#elif CONFIG_EXAMPLE_ENABLE_SPI_CAM_0_SENSOR
#define V4L2_CTL_DEFAULT_DEV    ESP_VIDEO_SPI_DEVICE_0_NAME
#elif CONFIG_EXAMPLE_ENABLE_USB_UVC_CAM_SENSOR
#define V4L2_CTL_DEFAULT_DEV    ESP_VIDEO_USB_UVC_DEVICE_NAME(0)
#else
#define V4L2_CTL_DEFAULT_DEV    ESP_VIDEO_MIPI_CSI_DEVICE_NAME
#endif

#define EXAMPLE_V4L2_CMD_CHECK(ins)                         \
    do {                                                    \
        if (arg_parse(argc, argv, (void **)&ins)){          \
            arg_print_errors(stderr, ins.end, argv[0]);     \
            return -1;                                      \
        }                                                   \
    } while (0)

#ifdef CONFIG_EXAMPLE_V4L2_CMD_CTL
void v4l2_cmd_ctl_register(void);
#endif

#ifdef CONFIG_EXAMPLE_V4L2_CMD_BF
void v4l2_cmd_bf_register(void);
#endif

#ifdef CONFIG_EXAMPLE_V4L2_CMD_CCM
void v4l2_cmd_ccm_register(void);
#endif

#ifdef CONFIG_EXAMPLE_V4L2_CMD_GAMMA
void v4l2_cmd_gamma_register(void);
#endif


#if EXAMPLE_TINYUSB_MSC_STORAGE
/**
 * @brief Get the storage handle
 *
 * @return Storage handle
 */
example_storage_handle_t example_storage_get_handle(void);
#endif

/**
 * @brief Decode the device name from the string
 *
 * @param dev_str Device name string
 * @param buffer Buffer to store the decoded device name
 * @param buffer_size Buffer size
 *
 * @return 0 on success, -1 on failure
 */
int decode_dev_name(const char *dev_str, char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif
