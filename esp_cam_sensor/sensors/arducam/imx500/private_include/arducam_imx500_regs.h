/*
 * SPDX-FileCopyrightText: 2026 Arducam Electronic Technology (Nanjing) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * imx500 register definitions.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define IMX500_REG_END      0xffff
#define IMX500_REG_DELAY    0xfffe

/* imx500 registers */
#define IMX500_REG_DEVICE_BASE                  0x0100
#define IMX500_REG_PIXFORMAT_BASE               0x0200
#define IMX500_REG_FORMAT_BASE                  0x0300
#define IMX500_REG_CTRL_BASE                    0x0400
#define IMX500_REG_SENSOR_BASE                  0x0500
#define IMX500_REG_IPC_BASE                     0x0600

#define IMX500_REG_STREAM_ON                    (IMX500_REG_DEVICE_BASE | 0x0000)
#define IMX500_REG_DEVICE_VERSION               (IMX500_REG_DEVICE_BASE | 0x0001)
#define IMX500_REG_SENSOR_ID                    (IMX500_REG_DEVICE_BASE | 0x0002)
#define IMX500_REG_DEVICE_ID                    (IMX500_REG_DEVICE_BASE | 0x0003)
#define IMX500_REG_FOCUS_MOTOR                  (IMX500_REG_DEVICE_BASE | 0x0004)
#define IMX500_REG_UNIQUE_ID                    (IMX500_REG_DEVICE_BASE | 0x0006)
#define IMX500_REG_SYSTEM_IDLE                  (IMX500_REG_DEVICE_BASE | 0x0007)

/* imx500 pixformat registers */
#define IMX500_REG_PIXFORMAT_INDEX              (IMX500_REG_PIXFORMAT_BASE | 0x0000)
#define IMX500_REG_PIXFORMAT_TYPE               (IMX500_REG_PIXFORMAT_BASE | 0x0001)
#define IMX500_REG_BAYER_ORDER                  (IMX500_REG_PIXFORMAT_BASE | 0x0002)
#define IMX500_REG_MIPI_LANES                   (IMX500_REG_PIXFORMAT_BASE | 0x0003)
#define IMX500_REG_FLIPS_DONT_CHANGE_ORDER      (IMX500_REG_PIXFORMAT_BASE | 0x0004)

/* imx500 resolution registers */
#define IMX500_REG_RESOLUTION_INDEX             (IMX500_REG_FORMAT_BASE | 0x0000)
#define IMX500_REG_FORMAT_WIDTH                 (IMX500_REG_FORMAT_BASE | 0x0001)
#define IMX500_REG_FORMAT_HEIGHT                (IMX500_REG_FORMAT_BASE | 0x0002)

/* imx500 control registers */
#define IMX500_REG_CTRL_INDEX                   (IMX500_REG_CTRL_BASE | 0x0000)
#define IMX500_REG_CTRL_ID                      (IMX500_REG_CTRL_BASE | 0x0001)
#define IMX500_REG_CTRL_MIN                     (IMX500_REG_CTRL_BASE | 0x0002)
#define IMX500_REG_CTRL_MAX                     (IMX500_REG_CTRL_BASE | 0x0003)
#define IMX500_REG_CTRL_STEP                    (IMX500_REG_CTRL_BASE | 0x0004)
#define IMX500_REG_CTRL_DEF                     (IMX500_REG_CTRL_BASE | 0x0005)
#define IMX500_REG_CTRL_VALUE                   (IMX500_REG_CTRL_BASE | 0x0006)

#define IMX500_REG_SENSOR_RD                    (IMX500_REG_SENSOR_BASE | 0x0001)
#define IMX500_REG_SENSOR_WR                    (IMX500_REG_SENSOR_BASE | 0x0002)

/* imx500 IPC registers */
#define IMX500_REG_IPC_TUNING_DATA              (IMX500_REG_IPC_BASE | 0x0015)
#define IMX500_REG_IPC_UPDATE_TUNING_DATA_FLAG  (IMX500_REG_IPC_BASE | 0x0016)
#define IMX500_REG_IPC_TUNING_DATA_CHECKSUM     (IMX500_REG_IPC_BASE | 0x0017)
#define IMX500_REG_IPC_TUNING_DATA_LENGTH       (IMX500_REG_IPC_BASE | 0x0018)
#define IMX500_REG_IPC_TUNING_DATA_READ_FLAG    (IMX500_REG_IPC_BASE | 0x0019)
#define IMX500_REG_IPC_GAIN_TABLE_LENGTH        (IMX500_REG_IPC_BASE | 0x001A)
#define IMX500_REG_IPC_GAIN_TABLE_DATA          (IMX500_REG_IPC_BASE | 0x001B)

#define ERROR_DATA                     0xFFFFFFFE

#ifdef __cplusplus
}
#endif
