/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sccb.h"
#include "soc/peri2_clkrst_struct.h"

#define MIPI_SCCB_FRE       (100000)
// #define MIPI_SCCB_SCL_IO    (18)
// #define MIPI_SCCB_SDA_IO    (19)
// If pin_sccb_sda is -1, use the already configured I2C bus by number
// void sccb_i2c_bus_init(int pin_scl, int pin_sda, int sccb_port)
void sccb_bus_init(int pin_scl, int pin_sda)
{
    PERI2_CLKRST.i3c_mst_clk_div_ctrl.i3c_mst_clk_sel = 1;
    PERI2_CLKRST.i3c_mst_clk_div_ctrl.i3c_mst_clk_div_num = 1 - 1;
    sccb_init(MIPI_SCCB_FRE, pin_scl, pin_sda);
    ESP_LOGI("SCCB", "Init Done");
}

esp_err_t sccb_i2c_init(int pin_sda, int pin_scl, int port)
{
    // Todo
    return ESP_OK;
}

esp_err_t sccb_i3c_init(int pin_sda, int pin_scl, int port)
{
    // Todo
    return ESP_OK;
}

#if 0
esp_err_t sccb_i2c_write(uint8_t data)
{
    // Todo
    return ESP_OK;
}

uint8_t sccb_i2c_read(uint8_t reg)
{
    // Todo
    return ESP_OK;
}
#endif
