/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
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
#define DEVICE_REG_BASE                  0x0100
#define PIXFORMAT_REG_BASE               0x0200
#define FORMAT_REG_BASE                  0x0300
#define CTRL_REG_BASE                    0x0400
#define SENSOR_REG_BASE                  0x0500
#define IPC_REG_BASE                     0x0600

#define STREAM_ON                        (DEVICE_REG_BASE | 0x0000)
#define DEVICE_VERSION_REG               (DEVICE_REG_BASE | 0x0001)
#define SENSOR_ID_REG                    (DEVICE_REG_BASE | 0x0002)
#define DEVICE_ID_REG                    (DEVICE_REG_BASE | 0x0003)
#define FOCUS_MOTOR_REG                  (DEVICE_REG_BASE | 0x0004)
#define FIRMWARE_SENSOR_ID_REG           (DEVICE_REG_BASE | 0x0005)
#define UNIQUE_ID_REG                    (DEVICE_REG_BASE | 0x0006)
#define SYSTEM_IDLE_REG                  (DEVICE_REG_BASE | 0x0007) 

/* pivariety pixformat registers */
#define PIXFORMAT_INDEX_REG              (PIXFORMAT_REG_BASE | 0x0000)
#define PIXFORMAT_TYPE_REG               (PIXFORMAT_REG_BASE | 0x0001)
#define BAYER_ORDER_REG                  (PIXFORMAT_REG_BASE | 0x0002)
#define MIPI_LANES_REG                   (PIXFORMAT_REG_BASE | 0x0003)
#define FLIPS_DONT_CHANGE_ORDER_REG      (PIXFORMAT_REG_BASE | 0x0004)

/* pivariety resolution registers */
#define RESOLUTION_INDEX_REG             (FORMAT_REG_BASE | 0x0000)
#define FORMAT_WIDTH_REG                 (FORMAT_REG_BASE | 0x0001)
#define FORMAT_HEIGHT_REG                (FORMAT_REG_BASE | 0x0002)

/* pivariety control registers */
#define CTRL_INDEX_REG                   (CTRL_REG_BASE | 0x0000)
#define CTRL_ID_REG                      (CTRL_REG_BASE | 0x0001)
#define CTRL_MIN_REG                     (CTRL_REG_BASE | 0x0002)
#define CTRL_MAX_REG                     (CTRL_REG_BASE | 0x0003)
#define CTRL_STEP_REG                    (CTRL_REG_BASE | 0x0004)
#define CTRL_DEF_REG                     (CTRL_REG_BASE | 0x0005)
#define CTRL_VALUE_REG                   (CTRL_REG_BASE | 0x0006)

#define SENSOR_RD_REG                    (SENSOR_REG_BASE | 0x0001)
#define SENSOR_WR_REG                    (SENSOR_REG_BASE | 0x0002)

/* pivariety IPC registers */
#define IPC_TUNING_DATA_REG              (IPC_REG_BASE | 0x0015)
#define IPC_UPDATE_TUNING_DATA_FLAG_REG  (IPC_REG_BASE | 0x0016)
#define IPC_TUNING_DATA_CHECKSUM_REG     (IPC_REG_BASE | 0x0017)
#define IPC_TUNING_DATA_LENGTH_REG       (IPC_REG_BASE | 0x0018)
#define IPC_TUNING_DATA_READ_FLAG_REG    (IPC_REG_BASE | 0x0019)
#define IPC_GAIN_TABLE_LENGTH_REG        (IPC_REG_BASE | 0x001A)
#define IPC_GAIN_TABLE_DATA_REG          (IPC_REG_BASE | 0x001B)

#define ERROR_DATA                     0xFFFFFFFE

#ifdef __cplusplus
}
#endif
