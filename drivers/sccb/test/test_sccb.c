/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * test environment UT_T2_I2C:
 * please prepare two ESP32-WROVER-KIT board.
 * Then connect GPIO18 and GPIO18, GPIO19 and GPIO19 between these two boards.
 */
#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "test_utils.h"
#include "unity_config.h"
#include "esp_log.h"
#include "sccb.h"

#ifndef CONFIG_SCCB_BASED_I3C_ENABLED
#define SCCB_PORT_NUM I2C_NUM_0   /*!< I2C port number for master dev */
#define SCCB_SCL_IO    8          /*!< gpio number for I2C master clock */
#define SCCB_SDA_IO    7          /*!< gpio number for I2C master data  */
#else
#define SCCB_PORT_NUM I3C_NUM_0   /*!< I3C port number for master dev */
#define SCCB_SCL_IO    19         /*!< gpio number for I3C master clock */
#define SCCB_SDA_IO    18         /*!< gpio number for I3C master data  */
#endif
#define FREQ_HZ          400000   /*!< clock frequency */

TEST_CASE("SCCB init test", "[SCCB]")
{
    for (int i = 0; i < 2; i++) {
        TEST_ESP_OK(sccb_init(SCCB_PORT_NUM, SCCB_SDA_IO, SCCB_SCL_IO, FREQ_HZ));
        TEST_ESP_OK(sccb_deinit(SCCB_PORT_NUM));
    }
}
