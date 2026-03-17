/*
 * SPDX-FileCopyrightText: 2026 Arducam Electronic Technology (Nanjing) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * pivariety register definitions.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define PIVARIETY_REG_END      0xffff
#define PIVARIETY_REG_DELAY    0xfffe

/* pivariety registers */
#define PIVARIETY_REG_DEVICE_BASE                  0x0100
#define PIVARIETY_REG_PIXFORMAT_BASE               0x0200
#define PIVARIETY_REG_FORMAT_BASE                  0x0300
#define PIVARIETY_REG_CTRL_BASE                    0x0400
#define PIVARIETY_REG_SENSOR_BASE                  0x0500
#define PIVARIETY_REG_IPC_BASE                     0x0600

#define PIVARIETY_REG_STREAM_ON                    (PIVARIETY_REG_DEVICE_BASE | 0x0000)
#define PIVARIETY_REG_DEVICE_VERSION               (PIVARIETY_REG_DEVICE_BASE | 0x0001)
#define PIVARIETY_REG_SENSOR_ID                    (PIVARIETY_REG_DEVICE_BASE | 0x0002)
#define PIVARIETY_REG_DEVICE_ID                    (PIVARIETY_REG_DEVICE_BASE | 0x0003)
#define PIVARIETY_REG_FOCUS_MOTOR                  (PIVARIETY_REG_DEVICE_BASE | 0x0004)
#define PIVARIETY_REG_UNIQUE_ID                    (PIVARIETY_REG_DEVICE_BASE | 0x0006)
#define PIVARIETY_REG_SYSTEM_IDLE                  (PIVARIETY_REG_DEVICE_BASE | 0x0007)

/* pivariety pixformat registers */
#define PIVARIETY_REG_PIXFORMAT_INDEX              (PIVARIETY_REG_PIXFORMAT_BASE | 0x0000)
#define PIVARIETY_REG_PIXFORMAT_TYPE               (PIVARIETY_REG_PIXFORMAT_BASE | 0x0001)
#define PIVARIETY_REG_BAYER_ORDER                  (PIVARIETY_REG_PIXFORMAT_BASE | 0x0002)
#define PIVARIETY_REG_MIPI_LANES                   (PIVARIETY_REG_PIXFORMAT_BASE | 0x0003)
#define PIVARIETY_REG_FLIPS_DONT_CHANGE_ORDER      (PIVARIETY_REG_PIXFORMAT_BASE | 0x0004)

/* pivariety resolution registers */
#define PIVARIETY_REG_RESOLUTION_INDEX             (PIVARIETY_REG_FORMAT_BASE | 0x0000)
#define PIVARIETY_REG_FORMAT_WIDTH                 (PIVARIETY_REG_FORMAT_BASE | 0x0001)
#define PIVARIETY_REG_FORMAT_HEIGHT                (PIVARIETY_REG_FORMAT_BASE | 0x0002)

/* pivariety control registers */
#define PIVARIETY_REG_CTRL_INDEX                   (PIVARIETY_REG_CTRL_BASE | 0x0000)
#define PIVARIETY_REG_CTRL_ID                      (PIVARIETY_REG_CTRL_BASE | 0x0001)
#define PIVARIETY_REG_CTRL_MIN                     (PIVARIETY_REG_CTRL_BASE | 0x0002)
#define PIVARIETY_REG_CTRL_MAX                     (PIVARIETY_REG_CTRL_BASE | 0x0003)
#define PIVARIETY_REG_CTRL_STEP                    (PIVARIETY_REG_CTRL_BASE | 0x0004)
#define PIVARIETY_REG_CTRL_DEF                     (PIVARIETY_REG_CTRL_BASE | 0x0005)
#define PIVARIETY_REG_CTRL_VALUE                   (PIVARIETY_REG_CTRL_BASE | 0x0006)

#define PIVARIETY_REG_SENSOR_RD                    (PIVARIETY_REG_SENSOR_BASE | 0x0001)
#define PIVARIETY_REG_SENSOR_WR                    (PIVARIETY_REG_SENSOR_BASE | 0x0002)

/* pivariety IPC registers */
#define PIVARIETY_REG_IPC_TUNING_DATA              (PIVARIETY_REG_IPC_BASE | 0x0015)
#define PIVARIETY_REG_IPC_UPDATE_TUNING_DATA_FLAG  (PIVARIETY_REG_IPC_BASE | 0x0016)
#define PIVARIETY_REG_IPC_TUNING_DATA_CHECKSUM     (PIVARIETY_REG_IPC_BASE | 0x0017)
#define PIVARIETY_REG_IPC_TUNING_DATA_LENGTH       (PIVARIETY_REG_IPC_BASE | 0x0018)
#define PIVARIETY_REG_IPC_TUNING_DATA_READ_FLAG    (PIVARIETY_REG_IPC_BASE | 0x0019)
#define PIVARIETY_REG_IPC_GAIN_TABLE_LENGTH        (PIVARIETY_REG_IPC_BASE | 0x001A)
#define PIVARIETY_REG_IPC_GAIN_TABLE_DATA          (PIVARIETY_REG_IPC_BASE | 0x001B)

#define ERROR_DATA                     0xFFFFFFFE

#ifdef __cplusplus
}
#endif
