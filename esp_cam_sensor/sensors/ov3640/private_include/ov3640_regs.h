/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * OV3640 register definitions.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define OV3640_REG_DLY                  0xFFFF
#define OV3640_REGLIST_TAIL             0x0000
#define OV3640_REG_CHIP_ID_H            0x300A
#define OV3640_REG_CHIP_ID_L            0x300B
#define OV3640_REG_SDE_CTRL             0x3355
#define OV3640_REG_SGNSET               0x3354
#define OV3640_REG_YBRIGHT              0x335E
#define OV3640_REG_YOFFSET              0x335C
#define OV3640_REG_YGAIN                0x335D
#define OV3640_REG_UREG                 0x335A
#define OV3640_REG_VREG                 0x335B
#define OV3640_REG_HISTO7               0x3047
#define OV3640_REG_WPT_HISH             0x3018
#define OV3640_REG_BPT_HISL             0x3019
#define OV3640_REG_VPT                  0x301A

#ifdef __cplusplus
}
#endif
