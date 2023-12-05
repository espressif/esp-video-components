/*
 * SPDX-FileCopyrightText: 2017-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _SOC_GPIO_STRUCT_H_
#define _SOC_GPIO_STRUCT_H_

#include "inttypes.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t bt_select;
    uint32_t out;
    uint32_t out_w1ts;
    uint32_t out_w1tc;
    uint32_t out1;
    uint32_t out1_w1ts;
    uint32_t out1_w1tc;
    union {
        struct {
            uint32_t sel                           :    8;
            uint32_t reserved8                     :    24;
        };
        uint32_t val;
    } sdio_select;
    uint32_t enable;
    uint32_t enable_w1ts;
    uint32_t enable_w1tc;
    uint32_t enable1;
    uint32_t enable1_w1ts;
    uint32_t enable1_w1tc;
    union {
        struct {
            uint32_t strapping                     :    16;
            uint32_t reserved16                    :    16;
        };
        uint32_t val;
    } strap;
    uint32_t in;
    uint32_t in1;
    uint32_t status;
    uint32_t status_w1ts;
    uint32_t status_w1tc;
    uint32_t status1;
    uint32_t status1_w1ts;
    uint32_t status1_w1tc;
    uint32_t intr0;
    uint32_t intr1;
    uint32_t intr2;
    uint32_t intr3;
    uint32_t intr0_1;
    uint32_t intr1_1;
    uint32_t intr2_1;
    uint32_t intr3_1;
    union {
        struct {
            uint32_t sync2_bypass                  :    2;
            uint32_t pad_driver                    :    1;
            uint32_t sync1_bypass                  :    2;
            uint32_t reserved5                     :    2;
            uint32_t int_type                      :    3;
            uint32_t wakeup_enable                 :    1;
            uint32_t config                        :    2;
            uint32_t int_ena                       :    5;
            uint32_t reserved18                    :    14;
        };
        uint32_t val;
    } pin[64];
    uint32_t status_next;
    uint32_t status_next1;
    union {
        struct {
            uint32_t func_sel                      :    7;  /*0~63:input from gpio, 64:input constant 0,96:input constant 1*/
            uint32_t sig_in_inv                    :    1;
            uint32_t sig_in_sel                    :    1;
            uint32_t reserved9                     :    23;
        };
        uint32_t val;
    } func_in_sel[256];
    union {
        struct {
            uint32_t func_sel                      :    9;
            uint32_t inv_sel                       :    1;
            uint32_t oen_sel                       :    1;
            uint32_t oen_inv_sel                   :    1;
            uint32_t reserved12                    :    20;
        };
        uint32_t val;
    } func_out_sel[64];
    uint32_t reserved_684;
    uint32_t reserved_688;
    uint32_t reserved_68c;
    uint32_t reserved_690;
    uint32_t reserved_694;
    uint32_t reserved_698;
    uint32_t reserved_69c;
    uint32_t reserved_6a0;
    uint32_t reserved_6a4;
    uint32_t reserved_6a8;
    uint32_t reserved_6ac;
    uint32_t reserved_6b0;
    uint32_t reserved_6b4;
    uint32_t reserved_6b8;
    uint32_t reserved_6bc;
    uint32_t reserved_6c0;
    uint32_t reserved_6c4;
    uint32_t reserved_6c8;
    uint32_t reserved_6cc;
    uint32_t reserved_6d0;
    uint32_t reserved_6d4;
    uint32_t reserved_6d8;
    uint32_t reserved_6dc;
    uint32_t reserved_6e0;
    uint32_t reserved_6e4;
    uint32_t reserved_6e8;
    uint32_t reserved_6ec;
    uint32_t reserved_6f0;
    uint32_t reserved_6f4;
    union {
        struct {
            uint32_t clk_en                        :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } clock_gate;
    union {
        struct {
            uint32_t date                          :    28;
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } date;
} gpio_dev_t;

extern gpio_dev_t GPIO;
#ifdef __cplusplus
}
#endif



#endif /*_SOC_GPIO_STRUCT_H_ */
