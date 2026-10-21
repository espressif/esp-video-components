/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */


//This is a simple build test. Can't run actually.
//TODO: make it a target test
#include "sdkconfig.h"
#include "esp_sccb_intf.h"
#include "esp_sccb_i2c.h"
#if ESP_SCCB_I3C_SUPPORTED
#include "esp_sccb_i3c.h"
#endif

extern void test_op(esp_sccb_io_handle_t handle);

void app_main(void)
{
    sccb_i2c_config_t config = {};
    esp_sccb_io_handle_t handle = NULL;
    sccb_new_i2c_io(NULL, &config, &handle);
    test_op(handle);

#if ESP_SCCB_I3C_SUPPORTED
    sccb_i3c_config_t i3c_config = {};
    esp_sccb_io_handle_t i3c_handle = NULL;
    sccb_new_i3c_io(NULL, &i3c_config, &i3c_handle);
    esp_sccb_transmit_reg_a8v8(i3c_handle, 0, 0);
    esp_sccb_del_i3c_io(i3c_handle);
#endif
}
