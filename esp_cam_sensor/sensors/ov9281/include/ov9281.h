/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_cam_sensor_types.h"
#include "ov9281_types.h"

/* SID works for the OV9281 I2C slave address is
* that:
*
* SID 0 = 0xc0, 0xe0
* SID 1 = 0x20, 0xe0
*
* Address 0xe0 is programmable via register 0x302B
* (OV9281_SC_CTRL_SCCB_ID_ADDR).
*
* So, the scheme to assign addresses to an (almost) arbitrary
* number of sensors is to consider 0x20 to be the "off" address.
* Start each sensor with SID as 1 so that they appear to be off.
*
* Then, to assign an address to one sensor:
*
* 0. Set corresponding SID to 0 (now only that sensor responds
*    to 0xc0).
* 1. Use 0xc0 to program the address from the default programmable
*    address of 0xe0 to the new address.
* 2. Set corresponding SID back to 1 (so it no longer responds
*    to 0xc0).
*/
#if CONFIG_CAMERA_OV9281_SID_LOW
#define OV9281_SCCB_ADDR   0x60
#else
#define OV9281_SCCB_ADDR   0xe0
#endif

#define OV9281_PID         0x9281
#define OV9281_SENSOR_NAME "OV9281"

/**
 * @brief Power on camera sensor device and detect the device connected to the designated sccb bus.
 *
 * @param[in] config Configuration related to device power-on and detection.
 * @return
 *      - Camera device handle on success, otherwise, failed.
 */
esp_cam_sensor_device_t *ov9281_detect(esp_cam_sensor_config_t *config);

#ifdef __cplusplus
}
#endif
