/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/****************************************************
Copyright (C), 2026, MetaSilicon Tech. Co., Ltd.
All rights reserved.
****************************************************/

#pragma once

#include "mat130a_types.h"
#include "mat130a_regs.h"
#include "mat130a_dvp_8bit_24Minput_1280x720_yuv422_uyvy_25fps.h"

/* Datasheet: RSTN rising edge to system-ready is 110-115 ms at 24 MHz. */
#define MAT130A_STARTUP_DELAY_MS 120U

/* Allow the internal firmware to complete stream state transitions. */
#define MAT130A_STREAM_ON_DELAY_MS  15U
#define MAT130A_STREAM_OFF_DELAY_MS 45U
