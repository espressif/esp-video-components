/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sdkconfig.h>
#include "mt9d111_regs.h"
#include "mt9d111_types.h"

#define YUV422_UVYV 0x0000
#define RGB565_LE 0x0022

#define YUV422_YUYV 0x0002
#define RGB565_BE 0x0020

#ifdef __cplusplus
extern "C" {
#endif

/** Soft Reset
  * 1. Bypass the PLL, R0x65:0=0xA000, if it is currently used
  * 2. Perform MCU reset by setting R0xC3:1=0x0501
  * 3. Enable soft reset by setting R0x0D:0=0x0021. Bit 0 is used for
  *    the sensor core reset while bit 5 refers to SOC reset.
  * 4. Disable soft reset by setting R0x0D:0=0x0000
  * 5. Wait 24 clock cycles before using the two-wire serial interface
  */
static const mt9d111_reginfo_t mt9d111_soft_reset_regs[] = {
    // Reset
    {MT9D111_REG_WRITE_PAGE, 0x0000},   // Page Register
    {0x65, 0xA000},     // bypassed PLL (prepare for soft reset)

    {MT9D111_REG_WRITE_PAGE, 0x0001},   // Page Register
    {0xC3, 0x0501},     // MCU_BOOT_MODE (MCU reset)
    {0xC3, 0x0500},     // MCU_BOOT_MODE (MCU reset)

    {MT9D111_REG_WRITE_PAGE, 0x0000},   // Page Register
    {0x0D, 0x0021},     // RESET_REG (enable soft reset)
    {0x0D, 0x0000},     // RESET_REG (disable soft reset)
    {MT9D111_REG_DELAY, 10},//DELAY=10
};

#include "mt9d111_dvp_8bit_20Minput_320x240_rgb565_be_10fps.h"
#include "mt9d111_dvp_8bit_20Minput_320x240_rgb565_le_10fps.h"

#include "mt9d111_dvp_8bit_20Minput_800x600_rgb565_be_10fps.h"
#include "mt9d111_dvp_8bit_20Minput_800x600_rgb565_le_10fps.h"

#include "mt9d111_dvp_8bit_20Minput_800x600_yuv422_uyvy_8fps.h"
#include "mt9d111_dvp_8bit_20Minput_800x600_yuv422_yuyv_8fps.h"

#include "mt9d111_dvp_8bit_20Minput_800x600_yuv422_uyvy_14fps.h"
#include "mt9d111_dvp_8bit_20Minput_800x600_yuv422_yuyv_14fps.h"

#include "mt9d111_dvp_8bit_24Minput_800x600_yuv422_uyvy_16fps.h"
#include "mt9d111_dvp_8bit_24Minput_800x600_yuv422_yuyv_16fps.h"

#ifdef __cplusplus
}
#endif
