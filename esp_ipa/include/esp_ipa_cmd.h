/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_IPA_IOCPARM_MASK        (0x0fffUL)
#define ESP_IPA_IOC_VOID            (0x20000000UL)
#define ESP_IPA_IOC_OUT             (0x40000000UL)
#define ESP_IPA_IOC_IN              (0x80000000UL)
#define ESP_IPA_IOC_INOUT           (ESP_IPA_IOC_IN | ESP_IPA_IOC_OUT)

#define ESP_IPA_IO(x, y)            ((long)(ESP_IPA_IOC_VOID | ((x) << 8) | (y)))
#define ESP_IPA_IOR(x, y, t)        ((long)(ESP_IPA_IOC_OUT  | ((sizeof(t) & ESP_IPA_IOCPARM_MASK) << 16) | ((x) << 8) | (y)))
#define ESP_IPA_IOW(x, y, t)        ((long)(ESP_IPA_IOC_IN   | ((sizeof(t) & ESP_IPA_IOCPARM_MASK) << 16) | ((x) << 8) | (y)))

#define ESP_IPA_IOC_SIZE(cmd)       (((cmd) >> 16) & ESP_IPA_IOCPARM_MASK)
#define ESP_IPA_IOC_TYPE(cmd)       (((cmd) >> 8) & 0xff)

#define ESP_IPA_IOC_TYPE_AGC        1

#define ESP_IPA_AGC_S_STATUS        ESP_IPA_IOW(ESP_IPA_IOC_TYPE_AGC,  0, int)
#define ESP_IPA_AGC_G_STATUS        ESP_IPA_IOR(ESP_IPA_IOC_TYPE_AGC,  1, int)

#ifdef __cplusplus
}
#endif
