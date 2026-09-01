/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * GC2053 register definitions.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Per the user-supplied init script
 * The page selector 0xFE selects one of the 4 register pages.
 * (release_V1_GC2053_MIPI_2lane_base24M_30fps_20181206.txt):
 *   - Page 0 : 0x03..0x0e are window/region (row/col start, etc.),
 *              0x10 has no special meaning here, 0x17 is mirror.
 *   - Page 3 : PLL divider / multiplier.
 *   - 0x3e   : global enable: bit7=lane_ena, bit4=MIPI_ena, bit0=double_lane_en.
 *              This is the register that actually starts / stops the
 *              MIPI output, NOT 0x10 / 0x04 / 0x05 / 0x06.
 *
 * Important: the previous driver revision incorrectly assumed
 *   0x04 = power on, 0x05 = clock en, 0x06 = PLL en, 0x10 = stream en.
 * That assumption is WRONG for the script this driver must follow:
 *   0x03=0x04, 0x04=0x60, 0x05=0x04, 0x06=0x4c are window/row/col
 *   configuration, and overwriting them with 0x01 corrupts the
 *   active region. Stream control must use 0x3e instead.
 */
#define GC2053_REG_SOFT_RESET       0x03
/* 0x04, 0x05, 0x06, 0x10 are window/region config in this init -
 * do NOT treat them as power / clock / PLL / stream enables. */
#define GC2053_REG_MIRROR           0x17  /* page 0, bit1=VFLIP, bit0=HMIRROR */

/* Real stream / lane enable (page 0). 0x91 = 1001 0001:
 *   bit7 = lane_ena       (1)  - starts/stops MIPI output
 *   bit4 = MIPI_ena       (1)
 *   bit0 = double_lane_en (1)
 * Writing 0x00 disables the lane. */
#define GC2053_REG_STREAM_EN        0x3e

/* Legacy aliases kept only for source compatibility with the older
 * (incorrect) driver revision. They are NOT used by the current
 * init script. */
#define GC2053_REG_POWER_ON         0x04
#define GC2053_REG_CLK_EN           0x05
#define GC2053_REG_PLL_EN           0x06

/* Legacy / optional controls used by the original factory init. */
#define GC2053_REG_GLOBAL_EN        0x3e
#define GC2053_REG_SLEEP_MODE       0x3f

#define GC2053_REG_SHUTTER_TIME_H       0x03
#define GC2053_REG_SHUTTER_TIME_L       0x04

#define GC2053_REG_END    0xff
/*
 * DELAY is a sentinel value for gc2053_write_array(). It MUST NOT collide
 * with any real register address. Page select is 0xFE on this sensor, so
 * 0xFE cannot be reused as the delay marker. Use 0xFD (unused by the chip).
 */
#define GC2053_REG_DELAY  0xbb

/* chip id is on page 0 */
#define GC2053_REG_CHIP_ID_HIGH   0xf0
#define GC2053_REG_CHIP_ID_LOW    0xf1
#define GC2053_REG_PAGE_SELECT    0xfe

#ifdef __cplusplus
}
#endif
