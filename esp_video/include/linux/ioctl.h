/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Define the macro to use the lwip ioctl macros to avoid conflicts with the esp-idf ioctl macros.
 */
#ifdef _IO
#define USING_LWIP_IOCTL_MACROS
#endif /* _IO */

#ifndef USING_LWIP_IOCTL_MACROS
#define IOCPARM_MASK        0x7fUL
#define IOC_VOID            0x20000000UL
#define IOC_OUT             0x40000000UL
#define IOC_IN              0x80000000UL
#define IOC_INOUT           (IOC_IN | IOC_OUT)

#define _IO(x, y)           ((long)(IOC_VOID | ((x) << 8) | (y)))
#define _IOR(x, y, t)       ((long)(IOC_OUT  | ((sizeof(t) & IOCPARM_MASK) << 16) | ((x) << 8) | (y)))
#define _IOW(x, y, t)       ((long)(IOC_IN   | ((sizeof(t) & IOCPARM_MASK) << 16) | ((x) << 8) | (y)))
#endif /* USING_LWIP_IOCTL_MACROS */

/**
 * @brief LWIP does not define the _IOWR macro, so we need to define it here.
 */
#ifndef _IOWR
#define _IOWR(x, y, t)      ((long)(IOC_INOUT | ((sizeof(t) & IOCPARM_MASK) << 16) | ((x) << 8) | (y)))
#endif /* _IOWR */

#ifdef __cplusplus
}
#endif
