/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "soc/i3c_mst_struct.h"
#include "soc/i3c_mst_mem_struct.h"
#include "soc/lp_i3c_mst_struct.h"
#include "soc/lp_i3c_mst_mem_struct.h"
#include "soc/gpio_sig_map.h"
#include "gpio_struct.h"
#include "soc/lp_io_mux_struct.h"
#include "soc/io_mux_struct.h"
#include "soc/core0_interrupt_reg.h"
#include "soc/peri1_clkrst_struct.h"
#include "soc/peri2_clkrst_struct.h"
#include "soc/lp_sys_struct.h"
#include "esp_log.h"
#include "i2c.h"

#define TEST_WITH_OS      (1)
#define TEST_INTERRUPT    (0)
#define TEST_LP_I3C       (0)
#if TEST_LP_I3C
#define TEST_I3C_MST      (LP_I3C_MST)
#define TEST_I3C_MST_MEM  (LP_I3C_MST_MEM)
#else
#define TEST_I3C_MST      (I3C_MST)
#define TEST_I3C_MST_MEM  (I3C_MST_MEM)
#endif

#if TEST_WITH_OS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
typedef SemaphoreHandle_t sem_t;
static void delay_us(uint32_t t)
{
    ets_delay_us(t);
}
#else 
typedef uint8_t sem_t;
#define ets_printf printf
static void delay_us(uint32_t t)
{
    for (uint32_t tu = 0; tu < 10*t; tu++);
}
#endif


typedef struct {
    union {
        struct {
            uint32_t cmd_attr   : 3;
            uint32_t tid        : 4;
            uint32_t cmd        : 8;
            uint32_t reserved15 : 1;
            uint32_t dev_indx   : 5;
            uint32_t reserved21 : 5;
            uint32_t dev_cnt    : 4;
            uint32_t roc        : 1;
            uint32_t toc        : 1;
        } addr;
        struct {
            uint32_t cmd_attr   : 3;
            uint32_t tid        : 4;
            uint32_t cmd        : 8;
            uint32_t cp         : 1;
            uint32_t dev_indx   : 5;
            uint32_t reserved21 : 2;
            uint32_t bc         : 3;
            uint32_t mode       : 3;
            uint32_t rnw        : 1;
            uint32_t roc        : 1;
            uint32_t toc        : 1;
        } immediate;
        struct {
            uint32_t cmd_attr   : 3;
            uint32_t tid        : 4;
            uint32_t cmd        : 8;
            uint32_t cp         : 1;
            uint32_t dev_indx   : 5;
            uint32_t reserved21 : 5;
            uint32_t mode       : 3;
            uint32_t rnw        : 1;
            uint32_t roc        : 1;
            uint32_t toc        : 1;
        } regular;
        uint32_t val;
    } cmd_l;
    union {
        struct {
            uint32_t reserved0 : 32;
        } addr;
        struct {
            uint32_t byte1 : 8;
            uint32_t byte2 : 8;
            uint32_t byte3 : 8;
            uint32_t byte4 : 8;
        } immediate;
        struct {
            uint32_t reserved0 : 16;
            uint32_t dl        : 16;
        } regular;
        uint32_t val;
    } cmd_h;
} i3c_mst_command_t;

typedef struct {
    union {
        struct {
            uint32_t static_addr  : 7;
            uint32_t reserved7    : 9;
            uint32_t dynamic_addr : 8;
            uint32_t reserved24   : 5;
            uint32_t d            : 2;
            uint32_t mode         : 1; // 0
        } setdasa;
        struct {
            uint32_t reserved0    : 16;
            uint32_t dynamic_addr : 7;
            uint32_t dyn          : 1;
            uint32_t reserved24   : 5;
            uint32_t d            : 2;
            uint32_t mode         : 1; // 0
        } entdaa;
        struct {
            uint32_t static_addr : 7;
            uint32_t reserved7   : 22;
            uint32_t d           : 2;
            uint32_t mode        : 1; // 1
        } i2c;
        uint32_t val;
    } dat;
} i3c_mst_addr_table_t;

typedef union {
    struct {
        uint32_t dl         : 16;
        uint32_t reserved16 : 8;
        uint32_t tid        : 4;
        uint32_t err_sts    : 4;
    };
    uint32_t val;
} i3c_mst_response_data_t;

static int i3c_num = 0;
#define I3C_INTR_SOURCE     (((CORE0_INTERRUPT_TEST_I3C_MST_INT_MAP_REG - DR_REG_CORE0_INTERRUPT_BASE) / 4) + i3c_num)

int i3c_write(uint32_t addr, size_t len, uint8_t *data)
{
    if (len > 256) {
        return -1;
    }
    int x = 0, y = 0;
    uint32_t i3c_mst_tx_data[64] = {0};

    i3c_mst_addr_table_t addr_table[1] = {
        {.dat.i2c = {.static_addr = addr, .d = 0x3, .mode = 0x1,}},
    };

    i3c_mst_command_t command[1] = {
        {.cmd_l.regular = {.cmd_attr = 0x0, .tid = 0x0, .cmd = 0x0, .cp = 0x0, .dev_indx = 0x0, .mode = 0x0, .rnw = 0x0, .roc = 0x0, .toc = 0x1}, .cmd_h.regular = {.dl = len}},
    };

    TEST_I3C_MST.reset_ctrl.val = ~(0x1);
    TEST_I3C_MST.reset_ctrl.val = 0;

    TEST_I3C_MST_MEM.dev_addr_table[0].val = addr_table[0].dat.val;

    TEST_I3C_MST_MEM.command_buf_port = command[0].cmd_l.val;
    TEST_I3C_MST_MEM.command_buf_port = command[0].cmd_h.val;

    for (x = 0; x < len / 4; x++) {
        i3c_mst_tx_data[x] =   (data[4 * x + 0] << 0) | (data[4 * x + 1] << 8) | (data[4 * x + 2] << 16) | (data[4 * x + 3] << 24);
        TEST_I3C_MST_MEM.tx_data_port = i3c_mst_tx_data[x];
    }
    if (len % 4) {
        for (y = 0; y < len % 4; y++) {
            i3c_mst_tx_data[x] |= data[4 * x + y] << (8 * y);
        }
        TEST_I3C_MST_MEM.tx_data_port = i3c_mst_tx_data[x];
    }

    TEST_I3C_MST.int_clr.val = ~0;
    TEST_I3C_MST.device_ctrl.trans_start = 1;
    TEST_I3C_MST.device_ctrl.trans_start = 0;

    while (!TEST_I3C_MST.int_st.transfer_complete);

    if (TEST_I3C_MST.int_st.transfer_err) {
        return -1;
    } else {
        return 0;
    }
}

int i3c_read(uint32_t addr, size_t len, uint8_t *data)
{
    if (len > 256) {
        return -1;
    }
    int x = 0, y = 0;

    uint32_t i3c_mst_rx_data[64] = {0};

    i3c_mst_addr_table_t addr_table[1] = {
        {.dat.i2c = {.static_addr = addr, .d = 0x3, .mode = 0x1,}},
    };

    i3c_mst_command_t command[2] = {
        {.cmd_l.regular = {.cmd_attr = 0x0, .tid = 0x0, .cmd = 0x0, .cp = 0x0, .dev_indx = 0x0, .mode = 0x0, .rnw = 0x1, .roc = 0x0, .toc = 0x1}, .cmd_h.regular = {.dl = len}},
    };

    TEST_I3C_MST.reset_ctrl.val = ~(0x1);
    TEST_I3C_MST.reset_ctrl.val = 0;

    // printf("table: 0x%x\n", &TEST_I3C_MST_MEM.dev_addr_table[0]);

    TEST_I3C_MST_MEM.dev_addr_table[0].val = addr_table[0].dat.val;

    TEST_I3C_MST_MEM.command_buf_port = command[0].cmd_l.val;
    TEST_I3C_MST_MEM.command_buf_port = command[0].cmd_h.val;

    TEST_I3C_MST.int_clr.val = ~0;
    TEST_I3C_MST.device_ctrl.trans_start = 1;
    TEST_I3C_MST.device_ctrl.trans_start = 0;

    while (!TEST_I3C_MST.int_st.transfer_complete);
    
    for (x = 0; x < len / 4; x++) {
        i3c_mst_rx_data[x] = TEST_I3C_MST_MEM.rx_data_port;
        // printf("i3c_mst_rx_data[%d]: 0x%x\n", x, i3c_mst_rx_data[x]);
        for (y = 0; y < 4; y++) {
            data[4*x + y] = (i3c_mst_rx_data[x] >> (8 * y)) & 0xFF;
        }
    }
    if (len % 4) {
        i3c_mst_rx_data[x] = TEST_I3C_MST_MEM.rx_data_port;
        // printf("i3c_mst_rx_data[%d]: 0x%x\n", x, i3c_mst_rx_data[x]);
        for (y = 0; y < len % 4; y++) {
            data[4*x + y] = (i3c_mst_rx_data[x] >> (8 * y)) & 0xFF;
        }
    }

    if (TEST_I3C_MST.int_st.transfer_err) {
        return -1;
    } else {
        return 0;
    }
}

int i3c_write_mem(uint32_t addr, uint8_t reg, size_t len, uint8_t *data)
{
    if (len > 254) {
        return -1;
    }
    int x = 0, y = 0;
    uint32_t i3c_mst_tx_data[64] = {0};

    i3c_mst_addr_table_t addr_table[1] = {
        {.dat.i2c = {.static_addr = addr, .d = 0x0, .mode = 0x1,}},
    };

    i3c_mst_command_t command[1] = {
        {.cmd_l.regular = {.cmd_attr = 0x0, .tid = 0x0, .cmd = 0x0, .cp = 0x0, .dev_indx = 0x0, .mode = 0x0, .rnw = 0x0, .roc = 0x0, .toc = 0x1}, .cmd_h.regular = {.dl = (len + 1)}},
    };

    TEST_I3C_MST.reset_ctrl.val = ~(0x1);
    TEST_I3C_MST.reset_ctrl.val = 0;

    TEST_I3C_MST_MEM.dev_addr_table[0].val = addr_table[0].dat.val;

    TEST_I3C_MST_MEM.command_buf_port = command[0].cmd_l.val;
    TEST_I3C_MST_MEM.command_buf_port = command[0].cmd_h.val;
    
    if (len >= 3) {
        i3c_mst_tx_data[0] = reg | (data[0] << 8) | (data[1] << 16) | (data[2] << 24);
        TEST_I3C_MST_MEM.tx_data_port = i3c_mst_tx_data[0];
        x = 1;
        for (x = 1; x < (len - 3) / 4; x++) {
            i3c_mst_tx_data[x] =   (data[4 * x - 1] << 0) | (data[4 * x - 0] << 8) | (data[4 * x + 1] << 16) | (data[4 * x + 2] << 24);
            TEST_I3C_MST_MEM.tx_data_port = i3c_mst_tx_data[x];
        }
        if ((len - 3) % 4) {
            for (y = 0; y < (len - 3) % 4; y++) {
                i3c_mst_tx_data[x] |= data[4 * x - 1 + y] << (8 * y);
            }
            TEST_I3C_MST_MEM.tx_data_port = i3c_mst_tx_data[x];
        }
    } else if (len == 2) {
        i3c_mst_tx_data[0] = reg | (data[0] << 8) | (data[1] << 16);
        TEST_I3C_MST_MEM.tx_data_port = i3c_mst_tx_data[0];
    } else if (len == 1) {
        i3c_mst_tx_data[0] = reg | (data[0] << 8);
        TEST_I3C_MST_MEM.tx_data_port = i3c_mst_tx_data[0];
    } else {
        i3c_mst_tx_data[0] = reg;
        TEST_I3C_MST_MEM.tx_data_port = i3c_mst_tx_data[0];
    }

    TEST_I3C_MST.int_clr.val = ~0;
    TEST_I3C_MST.device_ctrl.trans_start = 1;
    TEST_I3C_MST.device_ctrl.trans_start = 0;

    while (!TEST_I3C_MST.int_st.transfer_complete);

    if (TEST_I3C_MST.int_st.transfer_err) {
        return -1;
    } else {
        return 0;
    }
}

int i3c_read_mem(uint32_t addr, uint8_t reg, size_t len, uint8_t *data)
{
    if (len > 254) {
        return -1;
    }
    int x = 0, y = 0;
    int size = len;

    uint32_t i3c_mst_tx_data[1] = {0};
    uint32_t i3c_mst_rx_data[64] = {0};

    i3c_mst_addr_table_t addr_table[1] = {
        {.dat.i2c = {.static_addr = addr, .d = 0x0, .mode = 0x1,}},
    };

    i3c_mst_command_t command[2] = {
        {.cmd_l.regular = {.cmd_attr = 0x0, .tid = 0x0, .cmd = 0x0, .cp = 0x0, .dev_indx = 0x0, .mode = 0x0, .rnw = 0x0, .roc = 0x0, .toc = 0x0}, .cmd_h.regular = {.dl = 0x1}},
        {.cmd_l.regular = {.cmd_attr = 0x0, .tid = 0x0, .cmd = 0x0, .cp = 0x0, .dev_indx = 0x0, .mode = 0x0, .rnw = 0x1, .roc = 0x0, .toc = 0x1}, .cmd_h.regular = {.dl = size}},
    };

    TEST_I3C_MST.reset_ctrl.val = ~(0x1);
    TEST_I3C_MST.reset_ctrl.val = 0;
    
    // printf("table: 0x%x\n", &TEST_I3C_MST_MEM.dev_addr_table[0]);

    TEST_I3C_MST_MEM.dev_addr_table[0].val = addr_table[0].dat.val;

    TEST_I3C_MST_MEM.command_buf_port = command[0].cmd_l.val;
    TEST_I3C_MST_MEM.command_buf_port = command[0].cmd_h.val;
    TEST_I3C_MST_MEM.command_buf_port = command[1].cmd_l.val;
    TEST_I3C_MST_MEM.command_buf_port = command[1].cmd_h.val;

    i3c_mst_tx_data[0] = reg;
    TEST_I3C_MST_MEM.tx_data_port = i3c_mst_tx_data[0];

    TEST_I3C_MST.int_clr.val = ~0;
    TEST_I3C_MST.device_ctrl.trans_start = 1;
    TEST_I3C_MST.device_ctrl.trans_start = 0;

    while (!TEST_I3C_MST.int_st.transfer_complete);
    
    for (x = 0; x < len / 4; x++) {
        i3c_mst_rx_data[x] = TEST_I3C_MST_MEM.rx_data_port;
        // printf("i3c_mst_rx_data[%d]: 0x%x\n", x, i3c_mst_rx_data[x]);
        for (y = 0; y < 4; y++) {
            data[4*x + y] = (i3c_mst_rx_data[x] >> (8 * y)) & 0xFF;
        }
    }
    if (len % 4) {
        i3c_mst_rx_data[x] = TEST_I3C_MST_MEM.rx_data_port;
        // printf("i3c_mst_rx_data[%d]: 0x%x\n", x, i3c_mst_rx_data[x]);
        for (y = 0; y < len % 4; y++) {
            data[4*x + y] = (i3c_mst_rx_data[x] >> (8 * y)) & 0xFF;
        }
    }

    if (TEST_I3C_MST.int_st.transfer_err) {
        return -1;
    } else {
        return 0;
    }
}

int i3c_write_mem16(uint32_t addr, uint16_t reg, size_t len, uint8_t *data)
{
    if (len > 254) {
        return -1;
    }
    int x = 0, y = 0;
    uint32_t i3c_mst_tx_data[64] = {0};

    i3c_mst_addr_table_t addr_table[1] = {
        {.dat.i2c = {.static_addr = addr, .d = 0x0, .mode = 0x1,}},
    };

    i3c_mst_command_t command[1] = {
        {.cmd_l.regular = {.cmd_attr = 0x0, .tid = 0x0, .cmd = 0x0, .cp = 0x0, .dev_indx = 0x0, .mode = 0x0, .rnw = 0x0, .roc = 0x0, .toc = 0x1}, .cmd_h.regular = {.dl = (len + 2)}},
    };

    TEST_I3C_MST.reset_ctrl.val = ~(0x1);
    TEST_I3C_MST.reset_ctrl.val = 0;

    TEST_I3C_MST_MEM.dev_addr_table[0].val = addr_table[0].dat.val;

    TEST_I3C_MST_MEM.command_buf_port = command[0].cmd_l.val;
    TEST_I3C_MST_MEM.command_buf_port = command[0].cmd_h.val;
    
    if (len >= 2) {
        i3c_mst_tx_data[0] = ((reg << 8) & 0xFF00) | ((reg >> 8) & 0xFF) | (data[0] << 16) | (data[1] << 24);
        TEST_I3C_MST_MEM.tx_data_port = i3c_mst_tx_data[0];
        x = 1;
        for (x = 1; x < (len - 2) / 4; x++) {
            i3c_mst_tx_data[x] =   (data[4 * x - 2] << 0) | (data[4 * x - 1] << 8) | (data[4 * x - 0] << 16) | (data[4 * x + 1] << 24);
            TEST_I3C_MST_MEM.tx_data_port = i3c_mst_tx_data[x];
        }
        if ((len - 2) % 4) {
            for (y = 0; y < (len - 2) % 4; y++) {
                i3c_mst_tx_data[x] |= data[4 * x - 2 + y] << (8 * y);
            }
            TEST_I3C_MST_MEM.tx_data_port = i3c_mst_tx_data[x];
        }
    } else if (len == 1) {
        i3c_mst_tx_data[0] = ((reg << 8) & 0xFF00) | ((reg >> 8) & 0xFF) | (data[0] << 16);
        TEST_I3C_MST_MEM.tx_data_port = i3c_mst_tx_data[0];
    } else {
        i3c_mst_tx_data[0] = ((reg << 8) & 0xFF00) | ((reg >> 8) & 0xFF);
        TEST_I3C_MST_MEM.tx_data_port = i3c_mst_tx_data[0];
    }

    TEST_I3C_MST.int_clr.val = ~0;
    TEST_I3C_MST.device_ctrl.trans_start = 1;
    TEST_I3C_MST.device_ctrl.trans_start = 0;

    while (!TEST_I3C_MST.int_st.transfer_complete);

    if (TEST_I3C_MST.int_st.transfer_err) {
        return -1;
    } else {
        return 0;
    }
}

int i3c_read_mem16(uint32_t addr, uint16_t reg, size_t len, uint8_t *data)
{
    if (len > 254) {
        return -1;
    }
    int x = 0, y = 0;
    int size = len;

    uint32_t i3c_mst_tx_data[1] = {0};
    uint32_t i3c_mst_rx_data[64] = {0};

    i3c_mst_addr_table_t addr_table[1] = {
        {.dat.i2c = {.static_addr = addr, .d = 0x0, .mode = 0x1,}},
    };

    i3c_mst_command_t command[2] = {
        {.cmd_l.regular = {.cmd_attr = 0x0, .tid = 0x0, .cmd = 0x0, .cp = 0x0, .dev_indx = 0x0, .mode = 0x0, .rnw = 0x0, .roc = 0x0, .toc = 0x0}, .cmd_h.regular = {.dl = 0x2}},
        {.cmd_l.regular = {.cmd_attr = 0x0, .tid = 0x0, .cmd = 0x0, .cp = 0x0, .dev_indx = 0x0, .mode = 0x0, .rnw = 0x1, .roc = 0x0, .toc = 0x1}, .cmd_h.regular = {.dl = size}},
    };

    TEST_I3C_MST.reset_ctrl.val = ~(0x1);
    TEST_I3C_MST.reset_ctrl.val = 0;
    
    // printf("table: 0x%x\n", &TEST_I3C_MST_MEM.dev_addr_table[0]);

    TEST_I3C_MST_MEM.dev_addr_table[0].val = addr_table[0].dat.val;

    TEST_I3C_MST_MEM.command_buf_port = command[0].cmd_l.val;
    TEST_I3C_MST_MEM.command_buf_port = command[0].cmd_h.val;
    TEST_I3C_MST_MEM.command_buf_port = command[1].cmd_l.val;
    TEST_I3C_MST_MEM.command_buf_port = command[1].cmd_h.val;

    i3c_mst_tx_data[0] = ((reg << 8) & 0xFF00) | ((reg >> 8) & 0xFF);
    TEST_I3C_MST_MEM.tx_data_port = i3c_mst_tx_data[0];

    TEST_I3C_MST.int_clr.val = ~0;
    TEST_I3C_MST.device_ctrl.trans_start = 1;
    TEST_I3C_MST.device_ctrl.trans_start = 0;

    while (!TEST_I3C_MST.int_st.transfer_complete);
    
    for (x = 0; x < len / 4; x++) {
        i3c_mst_rx_data[x] = TEST_I3C_MST_MEM.rx_data_port;
        // printf("i3c_mst_rx_data[%d]: 0x%x\n", x, i3c_mst_rx_data[x]);
        for (y = 0; y < 4; y++) {
            data[4*x + y] = (i3c_mst_rx_data[x] >> (8 * y)) & 0xFF;
        }
    }
    if (len % 4) {
        i3c_mst_rx_data[x] = TEST_I3C_MST_MEM.rx_data_port;
        // printf("i3c_mst_rx_data[%d]: 0x%x\n", x, i3c_mst_rx_data[x]);
        for (y = 0; y < len % 4; y++) {
            data[4*x + y] = (i3c_mst_rx_data[x] >> (8 * y)) & 0xFF;
        }
    }

    if (TEST_I3C_MST.int_st.transfer_err) {
        return -1;
    } else {
        return 0;
    }
}

int i3c_write_reg16(uint32_t addr, uint16_t reg, uint32_t len, ...)
{
    int ret = -1;
    va_list arg_ptr; 
    uint8_t data[len]; 
    va_start(arg_ptr, len);
    for (int x = 0; x < len; x++) {
        data[x] = va_arg(arg_ptr, int);
    }
    va_end(arg_ptr); 

    ret = i3c_write_mem16(addr, reg, len, data);

    return ret;
}

int i3c_read_reg16(uint32_t addr, uint16_t reg, uint32_t len, ...)
{
    int ret = -1;
    va_list arg_ptr; 
    uint8_t data[len];
    uint8_t *datap[len]; 
    va_start(arg_ptr, len);
    for (int x = 0; x < len; x++) {
        datap[x] = va_arg(arg_ptr, int);
    }
    va_end(arg_ptr); 

    ret = i3c_read_mem16(addr, reg, len, data);

    for (int x = 0; x < len; x++) {
        *datap[x] = data[x];
    }

    return ret;
}

static void i3c_set_pin(int io_scl, int io_sda, int od_en)
{
    int io_num[2]     = {io_scl, io_sda};
    int io_out_sig[2] = {I3C_MST_SCL_PAD_OUT_IDX, I3C_MST_SDA_PAD_OUT_IDX};
    int io_in_sig[2]  = {I3C_MST_SCL_PAD_IN_IDX, I3C_MST_SDA_PAD_IN_IDX};

    for (int x = 0; x < 2; x++) {
        if (io_num[x] < 24) {
            LP_IO_MUX.pad[io_num[x]].mux_sel = 0;
        }
        IO_MUX.gpio[io_num[x]].mcu_oe = 0;
        IO_MUX.gpio[io_num[x]].mcu_sel = 1;
        IO_MUX.gpio[io_num[x]].mcu_ie = 0;
        IO_MUX.gpio[io_num[x]].fun_ie = 1;
        IO_MUX.gpio[io_num[x]].fun_wpu = 1; // pull up

        if (io_num[x] < 32) {
            GPIO.enable_w1ts = 1 << io_num[x];
        } else {
            GPIO.enable1_w1ts = 1 << (io_num[x] - 32);
        }

        GPIO.pin[io_num[x]].pad_driver = od_en;
        GPIO.func_out_sel[io_num[x]].func_sel = io_out_sig[x];
        GPIO.func_out_sel[io_num[x]].inv_sel = 0;
        GPIO.func_out_sel[io_num[x]].oen_sel = 0;
        GPIO.func_out_sel[io_num[x]].oen_inv_sel = 0;
        GPIO.func_in_sel[io_in_sig[x]].func_sel = io_num[x];
        GPIO.func_in_sel[io_in_sig[x]].sig_in_inv = 0;
        GPIO.func_in_sel[io_in_sig[x]].sig_in_sel = 1;
    }
}

int i3c_prob()
{
    int x = 0, y = 0;
    uint8_t addr = 0x0;

    i3c_mst_addr_table_t addr_table[1] = {
        {.dat.i2c = {.static_addr = 0x0, .d = 0x0, .mode = 0x1,}},
    };

    i3c_mst_command_t command[1] = {
        {.cmd_l.regular = {.cmd_attr = 0x0, .tid = 0x0, .cmd = 0x0, .cp = 0x0, .dev_indx = 0x0, .mode = 0x0, .rnw = 0x0, .roc = 0x0, .toc = 0x1}, .cmd_h.regular = {.dl = 0}},
    };

    for (addr = 0x1; addr <= 0x7f; addr++) {
        printf("addr: 0x%x\n", addr);

        TEST_I3C_MST.reset_ctrl.val = ~(0x1);
        TEST_I3C_MST.reset_ctrl.val = 0;

        addr_table[0].dat.i2c.static_addr = addr;
        TEST_I3C_MST_MEM.dev_addr_table[0].val = addr_table[0].dat.val;
        TEST_I3C_MST_MEM.command_buf_port = command[0].cmd_l.val;
        TEST_I3C_MST_MEM.command_buf_port = command[0].cmd_h.val;

        TEST_I3C_MST.int_clr.val = ~0;
        TEST_I3C_MST.device_ctrl.trans_start = 1;
        TEST_I3C_MST.device_ctrl.trans_start = 0;

        while (!TEST_I3C_MST.int_st.transfer_complete) {
            printf("mst_main_fsm_state: 0x%x, i2c_mode_fsm_state: 0x%x\n", TEST_I3C_MST.present_state0.mst_main_fsm_state, TEST_I3C_MST.present_state0.i2c_mode_fsm_state);
        }
        printf("mst_main_fsm_state: 0x%x, i2c_mode_fsm_state: 0x%x\n", TEST_I3C_MST.present_state0.mst_main_fsm_state, TEST_I3C_MST.present_state0.i2c_mode_fsm_state);
        if (TEST_I3C_MST.int_st.transfer_err == 0) {
            printf("active addr: 0x%x\n", addr);
            return addr;
        }
    }

    return -1;
}

static void i3c_mst_module_enable(uint32_t *hw)
{
    if (hw == &I3C_MST) {
        PERI2_CLKRST.peri2_clk_en_ctrl.i3c_clk_en = 0;
        PERI2_CLKRST.peri2_clk_en_ctrl.i3c_clk_en = 1;
        PERI2_CLKRST.peri2_rst_ctrl.i3c_apb_rstn = 0;
        PERI2_CLKRST.peri2_rst_ctrl.i3c_apb_rstn = 1;
    } else {
        LP_SYS.lp_clk_ctrl.lp_i3c_mst_clk_en = 0;
        LP_SYS.lp_clk_ctrl.lp_i3c_mst_clk_en = 1;
        LP_SYS.lp_rst_ctrl.lp_i3c_mst_rstn = 0;
        LP_SYS.lp_rst_ctrl.lp_i3c_mst_rstn = 1;
    }
}

void i3c_init(int rate, int io_scl, int io_sda)
{
    i3c_mst_module_enable(&TEST_I3C_MST);

    i3c_set_pin(io_scl, io_sda, 0);
    
    // TEST_I3C_MST CONFIG
    TEST_I3C_MST.reset_ctrl.val = ~0;
    TEST_I3C_MST.reset_ctrl.val = 0;
    TEST_I3C_MST.device_ctrl.ba_include = 0;

    int div = 8;
    TEST_I3C_MST.scl_i3c_od_time.i3c_od_high_period = (6 * div - 1);
    TEST_I3C_MST.scl_i3c_od_time.i3c_od_low_period = (26 * div - 1);
    TEST_I3C_MST.scl_i3c_pp_time.i3c_pp_high_period = (6 * div - 1);
    TEST_I3C_MST.scl_i3c_pp_time.i3c_pp_low_period = (10 * div - 1);
    TEST_I3C_MST.scl_i2c_fm_time.i2c_fm_high_period = (80 * div - 1);
    TEST_I3C_MST.scl_i2c_fm_time.i2c_fm_low_period = (80 * div - 1);
    TEST_I3C_MST.scl_i2c_fmp_time.i2c_fmp_high_period = (40 * div - 1);
    TEST_I3C_MST.scl_i2c_fmp_time.i2c_fmp_low_period = (40 * div - 1);
    TEST_I3C_MST.sda_sample_time.sda_od_sample_time = (1 * div - 1);
    TEST_I3C_MST.sda_sample_time.sda_pp_sample_time = (1 * div - 1);
    TEST_I3C_MST.sda_hold_time.sda_od_tx_hold_time = (2 * div - 1);
    TEST_I3C_MST.sda_hold_time.sda_pp_tx_hold_time = (1 * div - 1);
    TEST_I3C_MST.scl_rstart_setup.scl_rstart_setup_time = (9 * div - 1);
    TEST_I3C_MST.scl_start_hold.scl_start_hold_time = (9 * div - 1);
    TEST_I3C_MST.scl_stop_hold.scl_stop_hold_time = (18 * div - 1);
    TEST_I3C_MST.scl_stop_setup.scl_stop_setup_time = (18 * div - 1);

    // TEST_I3C_MST INT ENA
    TEST_I3C_MST.int_clr.val = ~0;
    TEST_I3C_MST.int_ena.transfer_complete = 1;
    TEST_I3C_MST.int_ena.transfer_err = 1;
}
