/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "soc/gpio_sig_map.h"
#include "gpio_struct.h"
#include "soc/periph_addr.h"
#include "soc/i2c_reg.h"
#include "i2c_struct.h"

#include "soc/lp_io_mux_struct.h"
#include "soc/io_mux_struct.h"
#include "soc/core0_interrupt_reg.h"
#include "soc/peri1_clkrst_struct.h"
#include "soc/peri2_clkrst_struct.h"
#include "i2c.h"
#include "esp_log.h"

#define TEST_WITH_OS 1
#define TEST_INTERRUPT       (1)

static int i2c_num = 0;
#define I2C_INTR_SOURCE     (((CORE0_INTERRUPT_I2C_EXT0_INT_MAP_REG - DR_REG_CORE0_INTERRUPT_BASE) / 4) + i2c_num)

#if TEST_WITH_OS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_intr_alloc.h"
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
    // for (uint32_t tu = 0; tu < 10*t; tu++);
    ets_delay_us(t);
}
#endif

typedef struct {
    sem_t trans_complete;
    sem_t end;
    sem_t slave_stretch;
    sem_t nack;
} i2c_sem_t;

i2c_sem_t i2c_sem;


static void i2c_set_pin(int io_scl, int io_sda, int od_en)
{
    int io_num[2]     = {io_scl, io_sda};
    int io_out_sig[2] = {I2C0_SCL_PAD_OUT_IDX, I2C0_SDA_PAD_OUT_IDX};
    int io_in_sig[2]  = {I2C0_SCL_PAD_IN_IDX, I2C0_SDA_PAD_IN_IDX};

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

enum {
    I2C_OP_NULL = 0,
    I2C_OP_WRITE = 1,
    I2C_OP_STOP = 2,
    I2C_OP_READ = 3,
    I2C_OP_END = 4,
    I2C_OP_START = 6,
};

static inline void give_sem(sem_t *sem)
{
#if TEST_WITH_OS
    BaseType_t xPrioTask = 0;
    xSemaphoreGiveFromISR(*sem, (BaseType_t)xPrioTask);
#else
    *sem = 1;
#endif
}

static inline void wait_sem(sem_t *sem)
{
#if !TEST_INTERRUPT
while (1) {
    typeof(I2C0.int_status) status = I2C0.int_status;
    if (status.val == 0) {
        continue;
    }
    I2C0.int_clr.val = status.val;

    if (status.end_detect) {
        give_sem(&i2c_sem.end);
    }
    if(status.nack) {
        give_sem(&i2c_sem.nack);
    }
    if (status.trans_complete) {
        give_sem(&i2c_sem.trans_complete);
    }
    if (status.scl_st_to || status.scl_main_st_to || status.time_out) {
        I2C0.scl_sp_conf.scl_rst_slv_en = 0;
        I2C0.ctr.conf_upgate = 1;
        I2C0.scl_sp_conf.scl_rst_slv_num = 9;
        I2C0.scl_sp_conf.scl_rst_slv_en = 1;
        I2C0.ctr.conf_upgate = 1;
        I2C0.ctr.fsm_rst = 1;
        I2C0.ctr.fsm_rst = 0;
    }
    if(status.arbitration_lost) {
    }
    break;
}
#endif

#if TEST_WITH_OS
    xSemaphoreTake(*sem, (portTickType)portMAX_DELAY);
#else
    while (!*sem);
    *sem = 0;
#endif
}

static void i2c_isr(void *arg)
{
    (void)arg;
    typeof(I2C0.int_status) status = I2C0.int_status;
    if (status.val == 0) {
        return;
    }
    I2C0.int_clr.val = status.val;

    if (status.end_detect) {
        give_sem(&i2c_sem.end);
    }
    if(status.nack) {
        give_sem(&i2c_sem.nack);
    }
    if (status.trans_complete) {
        give_sem(&i2c_sem.trans_complete);
    }
    if (status.scl_st_to || status.scl_main_st_to || status.time_out) {
        I2C0.scl_sp_conf.scl_rst_slv_en = 0;
        I2C0.ctr.conf_upgate = 1;
        I2C0.scl_sp_conf.scl_rst_slv_num = 9;
        I2C0.scl_sp_conf.scl_rst_slv_en = 1;
        I2C0.ctr.conf_upgate = 1;
        I2C0.ctr.fsm_rst = 1;
        I2C0.ctr.fsm_rst = 0;
    }
    if(status.arbitration_lost) {
    }
}

int i2c_write(uint32_t addr, size_t len, uint8_t *data)
{
    int x = 0, y = 0;

    I2C0.command[0].val = 0;
    I2C0.command[0].op_code = I2C_OP_START; // start

    if (len <= 31) {
        I2C0.command[1].val = 0;
        I2C0.command[1].op_code = I2C_OP_WRITE; // write
        I2C0.command[1].ack_en = 1;
        I2C0.command[1].ack_exp = 0;
        I2C0.command[1].byte_num = 1 + len;
        I2C0.fifo_data.data = ((uint8_t)addr << 1) | 0x0;
        for(x = 0; x < len; x++) {
            I2C0.fifo_data.data = data[x];
        }

        I2C0.command[2].val = 0;
        I2C0.command[2].op_code = I2C_OP_STOP; // stop
        I2C0.ctr.trans_start = 1;
    } else {
        I2C0.command[1].val = 0;
        I2C0.command[1].op_code = I2C_OP_WRITE; // write
        I2C0.command[1].ack_en = 1;
        I2C0.command[1].ack_exp = 0;
        I2C0.command[1].byte_num = 32;
        I2C0.fifo_data.data = ((uint8_t)addr << 1) | 0x0;
        for(x = 0; x < 31; x++) {
            I2C0.fifo_data.data = data[x];
        }

        I2C0.command[2].val = 0;
        I2C0.command[2].op_code = I2C_OP_END; // end
        I2C0.ctr.trans_start = 1;
        x = 0;
        len = len - 31;
        for (x = 0; x < len / 32; x++) {
            wait_sem(&i2c_sem.end);
            I2C0.command[0].val = 0;
            I2C0.command[0].op_code = I2C_OP_WRITE;
            I2C0.command[0].ack_en = 1;
            I2C0.command[0].ack_exp = 0;
            I2C0.command[0].byte_num = 32;
            for(y = 0; y < 32; y++) {
                I2C0.fifo_data.data = data[31 + x * 32 + y];
            }

            if ((x == ((len / 32) - 1)) && (len % 32 == 0)) {
                I2C0.command[1].val = 0;
                I2C0.command[1].op_code = I2C_OP_STOP; // stop
            } else {
                I2C0.command[1].val = 0;
                I2C0.command[1].op_code = I2C_OP_END; // end
            }
            I2C0.ctr.trans_start = 1;
        }
        if (len % 32) {
            wait_sem(&i2c_sem.end);
            I2C0.command[0].val = 0;
            I2C0.command[0].op_code = I2C_OP_WRITE;
            I2C0.command[0].ack_en = 1;
            I2C0.command[0].ack_exp = 0;
            I2C0.command[0].byte_num = len % 32;
            for(y = 0; y < len % 32; y++) {
                I2C0.fifo_data.data = data[31 + x * 32 + y];
            }

            I2C0.command[1].val = 0;
            I2C0.command[1].op_code = I2C_OP_STOP; // stop
            I2C0.ctr.trans_start = 1;
        }
    }

    wait_sem(&i2c_sem.trans_complete);

    return 0;
}

int i2c_read(uint32_t addr, size_t len, uint8_t *data)
{
    int x = 0, y = 0;

    I2C0.command[0].val = 0;
    I2C0.command[0].op_code = I2C_OP_START; // start

    I2C0.command[1].val = 0;
    I2C0.command[1].op_code = I2C_OP_WRITE; // write
    I2C0.command[1].ack_en = 1;
    I2C0.command[1].ack_exp = 0;
    I2C0.command[1].byte_num = 1;
    I2C0.fifo_data.data = ((uint8_t)addr << 1) | 0x1;

    if (len <= 32) {
        if (len > 1) {
            I2C0.command[2].val = 0;
            I2C0.command[2].op_code = I2C_OP_READ; // read
            I2C0.command[2].ack_val = 0;
            I2C0.command[2].byte_num = len - 1;

            I2C0.command[3].val = 0;
            I2C0.command[3].op_code = I2C_OP_READ; // read
            I2C0.command[3].ack_val = 1;
            I2C0.command[3].byte_num = 1;

            I2C0.command[4].val = 0;
            I2C0.command[4].op_code = I2C_OP_STOP; // stop
        } else {
            I2C0.command[2].val = 0;
            I2C0.command[2].op_code = I2C_OP_READ; // read
            I2C0.command[2].ack_val = 1;
            I2C0.command[2].byte_num = 1;

            I2C0.command[3].val = 0;
            I2C0.command[3].op_code = I2C_OP_STOP; // stop
        }

        I2C0.ctr.trans_start = 1;
        wait_sem(&i2c_sem.trans_complete);
        for(x = 0; x < len; x++) {
            data[x] = I2C0.fifo_data.data;
        }
    } else {
        I2C0.command[2].val = 0;
        I2C0.command[2].op_code = I2C_OP_READ; // read
        I2C0.command[2].ack_val = 0;
        I2C0.command[2].byte_num = 32;

        I2C0.command[3].val = 0;
        I2C0.command[3].op_code = I2C_OP_END; // end
        I2C0.ctr.trans_start = 1;
        wait_sem(&i2c_sem.end);
        for(x = 0; x < 32; x++) {
            data[x] = I2C0.fifo_data.data;
        }
        x = 0;
        len = len - 32;
        for (x = 0; x < len / 32; x++) {
            I2C0.command[0].val = 0;
            I2C0.command[0].op_code = I2C_OP_READ; // read
            I2C0.command[0].ack_val = 0;
            I2C0.command[0].byte_num = 32;

            if ((x == ((len / 32) - 1)) && (len % 32 == 0)) {
                I2C0.command[0].val = 0;
                I2C0.command[0].op_code = I2C_OP_READ; // read
                I2C0.command[0].ack_val = 0;
                I2C0.command[0].byte_num = 31;

                I2C0.command[1].val = 0;
                I2C0.command[1].op_code = I2C_OP_READ; // read
                I2C0.command[1].ack_val = 1;
                I2C0.command[1].byte_num = 1;

                I2C0.command[2].val = 0;
                I2C0.command[2].op_code = I2C_OP_STOP; // stop
                I2C0.ctr.trans_start = 1;
                wait_sem(&i2c_sem.trans_complete);
            } else {
                I2C0.command[0].val = 0;
                I2C0.command[0].op_code = I2C_OP_READ; // read
                I2C0.command[0].ack_val = 0;
                I2C0.command[0].byte_num = 32;

                I2C0.command[1].val = 0;
                I2C0.command[1].op_code = I2C_OP_END; // end
                I2C0.ctr.trans_start = 1;
                wait_sem(&i2c_sem.end);
            }
            for(y = 0; y < 32; y++) {
                data[32 + x * 32 + y] = I2C0.fifo_data.data;
            }
        }
        if (len % 32) {
            if (len % 32 > 1) {
                I2C0.command[0].val = 0;
                I2C0.command[0].op_code = I2C_OP_READ; // read
                I2C0.command[0].ack_val = 0;
                I2C0.command[0].byte_num = len % 32 - 1;

                I2C0.command[1].val = 0;
                I2C0.command[1].op_code = I2C_OP_READ; // read
                I2C0.command[1].ack_val = 1;
                I2C0.command[1].byte_num = 1;

                I2C0.command[2].val = 0;
                I2C0.command[2].op_code = I2C_OP_STOP; // stop
            } else{
                I2C0.command[0].val = 0;
                I2C0.command[0].op_code = I2C_OP_READ; // read
                I2C0.command[0].ack_val = 1;
                I2C0.command[0].byte_num = 1;

                I2C0.command[1].val = 0;
                I2C0.command[1].op_code = I2C_OP_STOP; // stop
            }

            I2C0.ctr.trans_start = 1;
            wait_sem(&i2c_sem.trans_complete);
            for(y = 0; y < len % 32; y++) {
                data[32 + x * 32 + y] = I2C0.fifo_data.data;
            }
        }
    }
    return 0;
}

int i2c_write_mem(uint32_t addr, uint8_t reg, size_t len, uint8_t *data)
{
    int x = 0, y = 0;

    I2C0.command[0].val = 0;
    I2C0.command[0].op_code = I2C_OP_START; // start

    I2C0.command[1].val = 0;
    I2C0.command[1].op_code = I2C_OP_WRITE; // write
    I2C0.command[1].ack_en = 1;
    I2C0.command[1].ack_exp = 0;
    I2C0.command[1].byte_num = 2;
    I2C0.fifo_data.data = ((uint8_t)addr << 1) | 0x0;
    I2C0.fifo_data.data = reg;

    if (len <= 32) {
        I2C0.command[2].val = 0;
        I2C0.command[2].op_code = I2C_OP_WRITE; // write
        I2C0.command[2].ack_en = 1;
        I2C0.command[2].ack_exp = 0;
        I2C0.command[2].byte_num = len;
        for(x = 0; x < len; x++) {
            I2C0.fifo_data.data = data[x];
        }

        I2C0.command[3].val = 0;
        I2C0.command[3].op_code = I2C_OP_STOP; // stop
        I2C0.ctr.trans_start = 1;
    } else {
        I2C0.command[2].val = 0;
        I2C0.command[2].op_code = I2C_OP_WRITE; // write
        I2C0.command[2].ack_en = 1;
        I2C0.command[2].ack_exp = 0;
        I2C0.command[2].byte_num = 32;
        for(x = 0; x < 32; x++) {
            I2C0.fifo_data.data = data[x];
        }

        I2C0.command[3].val = 0;
        I2C0.command[3].op_code = I2C_OP_END; // end
        I2C0.ctr.trans_start = 1;
        x = 0;
        len = len - 32;
        for (x = 0; x < len / 32; x++) {
            wait_sem(&i2c_sem.end);
            I2C0.command[0].val = 0;
            I2C0.command[0].op_code = I2C_OP_WRITE;
            I2C0.command[0].ack_en = 1;
            I2C0.command[0].ack_exp = 0;
            I2C0.command[0].byte_num = 32;
            for(y = 0; y < 32; y++) {
                I2C0.fifo_data.data = data[32 + x * 32 + y];
            }

            if ((x == ((len / 32) - 1)) && (len % 32 == 0)) {
                I2C0.command[1].val = 0;
                I2C0.command[1].op_code = I2C_OP_STOP; // stop
            } else {
                I2C0.command[1].val = 0;
                I2C0.command[1].op_code = I2C_OP_END; // end
            }
            I2C0.ctr.trans_start = 1;
        }
        if (len % 32) {
            wait_sem(&i2c_sem.end);
            I2C0.command[0].val = 0;
            I2C0.command[0].op_code = I2C_OP_WRITE;
            I2C0.command[0].ack_en = 1;
            I2C0.command[0].ack_exp = 0;
            I2C0.command[0].byte_num = len % 32;
            for(y = 0; y < len % 32; y++) {
                I2C0.fifo_data.data = data[32 + x * 32 + y];
            }

            I2C0.command[1].val = 0;
            I2C0.command[1].op_code = I2C_OP_STOP; // stop
            I2C0.ctr.trans_start = 1;
        }
    }

    wait_sem(&i2c_sem.trans_complete);

    return 0;
}

int i2c_read_mem(uint32_t addr, uint8_t reg, size_t len, uint8_t *data)
{
    int x = 0, y = 0;

    I2C0.command[0].val = 0;
    I2C0.command[0].op_code = I2C_OP_START; // start

    I2C0.command[1].val = 0;
    I2C0.command[1].op_code = I2C_OP_WRITE; // write
    I2C0.command[1].ack_en = 1;
    I2C0.command[1].ack_exp = 0;
    I2C0.command[1].byte_num = 2;
    I2C0.fifo_data.data = ((uint8_t)addr << 1) | 0x0;
    I2C0.fifo_data.data = reg;

    I2C0.command[2].val = 0;
    I2C0.command[2].op_code = I2C_OP_START; // start

    I2C0.command[3].val = 0;
    I2C0.command[3].op_code = I2C_OP_WRITE; // write
    I2C0.command[3].ack_en = 1;
    I2C0.command[3].ack_exp = 0;
    I2C0.command[3].byte_num = 1;
    I2C0.fifo_data.data = ((uint8_t)addr << 1) | 0x1;

    if (len <= 32) {
        if (len > 1) {
            I2C0.command[4].val = 0;
            I2C0.command[4].op_code = I2C_OP_READ; // read
            I2C0.command[4].ack_val = 0;
            I2C0.command[4].byte_num = len - 1;

            I2C0.command[5].val = 0;
            I2C0.command[5].op_code = I2C_OP_READ; // read
            I2C0.command[5].ack_val = 1;
            I2C0.command[5].byte_num = 1;

            I2C0.command[6].val = 0;
            I2C0.command[6].op_code = I2C_OP_STOP; // stop
        } else {
            I2C0.command[4].val = 0;
            I2C0.command[4].op_code = I2C_OP_READ; // read
            I2C0.command[4].ack_val = 1;
            I2C0.command[4].byte_num = 1;

            I2C0.command[5].val = 0;
            I2C0.command[5].op_code = I2C_OP_STOP; // stop
        }

        I2C0.ctr.trans_start = 1;
        wait_sem(&i2c_sem.trans_complete);
        for(x = 0; x < len; x++) {
            data[x] = I2C0.fifo_data.data;
        }
    } else {
        I2C0.command[4].val = 0;
        I2C0.command[4].op_code = I2C_OP_READ; // read
        I2C0.command[4].ack_val = 0;
        I2C0.command[4].byte_num = 32;

        I2C0.command[5].val = 0;
        I2C0.command[5].op_code = I2C_OP_END; // end
        I2C0.ctr.trans_start = 1;
        wait_sem(&i2c_sem.end);
        for(x = 0; x < 32; x++) {
            data[x] = I2C0.fifo_data.data;
        }
        x = 0;
        len = len - 32;
        for (x = 0; x < len / 32; x++) {
            if ((x == ((len / 32) - 1)) && (len % 32 == 0)) {
                I2C0.command[0].val = 0;
                I2C0.command[0].op_code = I2C_OP_READ; // read
                I2C0.command[0].ack_val = 0;
                I2C0.command[0].byte_num = 31;

                I2C0.command[1].val = 0;
                I2C0.command[1].op_code = I2C_OP_READ; // read
                I2C0.command[1].ack_val = 1;
                I2C0.command[1].byte_num = 1;

                I2C0.command[2].val = 0;
                I2C0.command[2].op_code = I2C_OP_STOP; // stop
                I2C0.ctr.trans_start = 1;
                wait_sem(&i2c_sem.trans_complete);
            } else {
                I2C0.command[0].val = 0;
                I2C0.command[0].op_code = I2C_OP_READ; // read
                I2C0.command[0].ack_val = 0;
                I2C0.command[0].byte_num = 32;

                I2C0.command[1].val = 0;
                I2C0.command[1].op_code = I2C_OP_END; // end
                I2C0.ctr.trans_start = 1;
                wait_sem(&i2c_sem.end);
            }
            for(y = 0; y < 32; y++) {
                data[32 + x * 32 + y] = I2C0.fifo_data.data;
            }
        }
        if (len % 32) {
            if (len % 32 > 1) {
                I2C0.command[0].val = 0;
                I2C0.command[0].op_code = I2C_OP_READ; // read
                I2C0.command[0].ack_val = 0;
                I2C0.command[0].byte_num = len % 32 - 1;

                I2C0.command[1].val = 0;
                I2C0.command[1].op_code = I2C_OP_READ; // read
                I2C0.command[1].ack_val = 1;
                I2C0.command[1].byte_num = 1;

                I2C0.command[2].val = 0;
                I2C0.command[2].op_code = I2C_OP_STOP; // stop
            } else{
                I2C0.command[0].val = 0;
                I2C0.command[0].op_code = I2C_OP_READ; // read
                I2C0.command[0].ack_val = 1;
                I2C0.command[0].byte_num = 1;

                I2C0.command[1].val = 0;
                I2C0.command[1].op_code = I2C_OP_STOP; // stop
            }

            I2C0.ctr.trans_start = 1;
            wait_sem(&i2c_sem.trans_complete);
            for(y = 0; y < len % 32; y++) {
                data[32 + x * 32 + y] = I2C0.fifo_data.data;
            }
        }
    }

    return 0;
}

int i2c_write_mem16(uint32_t addr, uint16_t reg, size_t len, uint8_t *data)
{
    int x = 0, y = 0;

    uint8_t reg_byte[2] = {reg >> 8, reg & 0xFF};

    I2C0.command[0].val = 0;
    I2C0.command[0].op_code = I2C_OP_START; // start

    I2C0.command[1].val = 0;
    I2C0.command[1].op_code = I2C_OP_WRITE; // write
    I2C0.command[1].ack_en = 1;
    I2C0.command[1].ack_exp = 0;
    I2C0.command[1].byte_num = 3;
    I2C0.fifo_data.data = ((uint8_t)addr << 1) | 0x0;
    I2C0.fifo_data.data = reg_byte[0];
    I2C0.fifo_data.data = reg_byte[1];

    if (len <= 32) {
        I2C0.command[2].val = 0;
        I2C0.command[2].op_code = I2C_OP_WRITE; // write
        I2C0.command[2].ack_en = 1;
        I2C0.command[2].ack_exp = 0;
        I2C0.command[2].byte_num = len;
        for(x = 0; x < len; x++) {
            I2C0.fifo_data.data = data[x];
        }

        I2C0.command[3].val = 0;
        I2C0.command[3].op_code = I2C_OP_STOP; // stop
        I2C0.ctr.trans_start = 1;
    } else {
        I2C0.command[2].val = 0;
        I2C0.command[2].op_code = I2C_OP_WRITE; // write
        I2C0.command[2].ack_en = 1;
        I2C0.command[2].ack_exp = 0;
        I2C0.command[2].byte_num = 32;
        for(x = 0; x < 32; x++) {
            I2C0.fifo_data.data = data[x];
        }

        I2C0.command[3].val = 0;
        I2C0.command[3].op_code = I2C_OP_END; // end
        I2C0.ctr.trans_start = 1;
        x = 0;
        len = len - 32;
        for (x = 0; x < len / 32; x++) {
            wait_sem(&i2c_sem.end);
            I2C0.command[0].val = 0;
            I2C0.command[0].op_code = I2C_OP_WRITE;
            I2C0.command[0].ack_en = 1;
            I2C0.command[0].ack_exp = 0;
            I2C0.command[0].byte_num = 32;
            for(y = 0; y < 32; y++) {
                I2C0.fifo_data.data = data[32 + x * 32 + y];
            }

            if ((x == ((len / 32) - 1)) && (len % 32 == 0)) {
                I2C0.command[1].val = 0;
                I2C0.command[1].op_code = I2C_OP_STOP; // stop
            } else {
                I2C0.command[1].val = 0;
                I2C0.command[1].op_code = I2C_OP_END; // end
            }
            I2C0.ctr.trans_start = 1;
        }
        if (len % 32) {
            wait_sem(&i2c_sem.end);
            I2C0.command[0].val = 0;
            I2C0.command[0].op_code = I2C_OP_WRITE;
            I2C0.command[0].ack_en = 1;
            I2C0.command[0].ack_exp = 0;
            I2C0.command[0].byte_num = len % 32;
            for(y = 0; y < len % 32; y++) {
                I2C0.fifo_data.data = data[32 + x * 32 + y];
            }

            I2C0.command[1].val = 0;
            I2C0.command[1].op_code = I2C_OP_STOP; // stop
            I2C0.ctr.trans_start = 1;
        }
    }

    wait_sem(&i2c_sem.trans_complete);

    return 0;
}

int i2c_read_mem16(uint32_t addr, uint16_t reg, size_t len, uint8_t *data)
{
    int x = 0, y = 0;
    uint8_t reg_byte[2] = {reg >> 8, reg & 0xFF};

    I2C0.command[0].val = 0;
    I2C0.command[0].op_code = I2C_OP_START; // start

    I2C0.command[1].val = 0;
    I2C0.command[1].op_code = I2C_OP_WRITE; // write
    I2C0.command[1].ack_en = 1;
    I2C0.command[1].ack_exp = 0;
    I2C0.command[1].byte_num = 3;
    I2C0.fifo_data.data = ((uint8_t)addr << 1) | 0x0;
    I2C0.fifo_data.data = reg_byte[0];
    I2C0.fifo_data.data = reg_byte[1];

    I2C0.command[2].val = 0;
    I2C0.command[2].op_code = I2C_OP_START; // start

    I2C0.command[3].val = 0;
    I2C0.command[3].op_code = I2C_OP_WRITE; // write
    I2C0.command[3].ack_en = 1;
    I2C0.command[3].ack_exp = 0;
    I2C0.command[3].byte_num = 1;
    I2C0.fifo_data.data = ((uint8_t)addr << 1) | 0x1;

    if (len <= 32) {
        if (len > 1) {
            I2C0.command[4].val = 0;
            I2C0.command[4].op_code = I2C_OP_READ; // read
            I2C0.command[4].ack_val = 0;
            I2C0.command[4].byte_num = len - 1;

            I2C0.command[5].val = 0;
            I2C0.command[5].op_code = I2C_OP_READ; // read
            I2C0.command[5].ack_val = 1;
            I2C0.command[5].byte_num = 1;

            I2C0.command[6].val = 0;
            I2C0.command[6].op_code = I2C_OP_STOP; // stop
        } else {
            I2C0.command[4].val = 0;
            I2C0.command[4].op_code = I2C_OP_READ; // read
            I2C0.command[4].ack_val = 1;
            I2C0.command[4].byte_num = 1;

            I2C0.command[5].val = 0;
            I2C0.command[5].op_code = I2C_OP_STOP; // stop
        }

        I2C0.ctr.trans_start = 1;
        wait_sem(&i2c_sem.trans_complete);
        for(x = 0; x < len; x++) {
            data[x] = I2C0.fifo_data.data;
        }
    } else {
        I2C0.command[4].val = 0;
        I2C0.command[4].op_code = I2C_OP_READ; // read
        I2C0.command[4].ack_val = 0;
        I2C0.command[4].byte_num = 32;

        I2C0.command[5].val = 0;
        I2C0.command[5].op_code = I2C_OP_END; // end
        I2C0.ctr.trans_start = 1;
        wait_sem(&i2c_sem.end);
        for(x = 0; x < 32; x++) {
            data[x] = I2C0.fifo_data.data;
        }
        x = 0;
        len = len - 32;
        for (x = 0; x < len / 32; x++) {
            if ((x == ((len / 32) - 1)) && (len % 32 == 0)) {
                I2C0.command[0].val = 0;
                I2C0.command[0].op_code = I2C_OP_READ; // read
                I2C0.command[0].ack_val = 0;
                I2C0.command[0].byte_num = 31;

                I2C0.command[1].val = 0;
                I2C0.command[1].op_code = I2C_OP_READ; // read
                I2C0.command[1].ack_val = 1;
                I2C0.command[1].byte_num = 1;

                I2C0.command[2].val = 0;
                I2C0.command[2].op_code = I2C_OP_STOP; // stop
                I2C0.ctr.trans_start = 1;
                wait_sem(&i2c_sem.trans_complete);
            } else {
                I2C0.command[0].val = 0;
                I2C0.command[0].op_code = I2C_OP_READ; // read
                I2C0.command[0].ack_val = 0;
                I2C0.command[0].byte_num = 32;

                I2C0.command[1].val = 0;
                I2C0.command[1].op_code = I2C_OP_END; // end
                I2C0.ctr.trans_start = 1;
                wait_sem(&i2c_sem.end);
            }
            for(y = 0; y < 32; y++) {
                data[32 + x * 32 + y] = I2C0.fifo_data.data;
            }
        }
        if (len % 32) {
            if (len % 32 > 1) {
                I2C0.command[0].val = 0;
                I2C0.command[0].op_code = I2C_OP_READ; // read
                I2C0.command[0].ack_val = 0;
                I2C0.command[0].byte_num = len % 32 - 1;

                I2C0.command[1].val = 0;
                I2C0.command[1].op_code = I2C_OP_READ; // read
                I2C0.command[1].ack_val = 1;
                I2C0.command[1].byte_num = 1;

                I2C0.command[2].val = 0;
                I2C0.command[2].op_code = I2C_OP_STOP; // stop
            } else{
                I2C0.command[0].val = 0;
                I2C0.command[0].op_code = I2C_OP_READ; // read
                I2C0.command[0].ack_val = 1;
                I2C0.command[0].byte_num = 1;

                I2C0.command[1].val = 0;
                I2C0.command[1].op_code = I2C_OP_STOP; // stop
            }

            I2C0.ctr.trans_start = 1;
            wait_sem(&i2c_sem.trans_complete);
            for(y = 0; y < len % 32; y++) {
                data[32 + x * 32 + y] = I2C0.fifo_data.data;
            }
        }
    }

    return 0;
}

int i2c_write_reg(uint32_t addr, uint8_t reg, uint32_t len, ...)
{
    int ret = -1;
    va_list arg_ptr;
    uint8_t data[len];
    va_start(arg_ptr, len);
    for (int x = 0; x < len; x++) {
        data[x] = va_arg(arg_ptr, int);
    }
    va_end(arg_ptr);

    ret = i2c_write_mem(addr, reg, len, data);

    return ret;
}

int i2c_read_reg(uint32_t addr, uint8_t reg, uint32_t len, ...)
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

    ret = i2c_read_mem(addr, reg, len, data);

    for (int x = 0; x < len; x++) {
        *datap[x] = data[x];
    }

    return ret;
}

int i2c_write_reg16(uint32_t addr, uint16_t reg, uint32_t len, ...)
{
    int ret = -1;
    va_list arg_ptr;
    uint8_t data[len];
    va_start(arg_ptr, len);
    for (int x = 0; x < len; x++) {
        data[x] = va_arg(arg_ptr, int);
    }
    va_end(arg_ptr);

    ret = i2c_write_mem16(addr, reg, len, data);

    return ret;
}

int i2c_read_reg16(uint32_t addr, uint16_t reg, uint32_t len, ...)
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

    ret = i2c_read_mem16(addr, reg, len, data);

    for (int x = 0; x < len; x++) {
        *datap[x] = data[x];
    }

    return ret;
}

int i2c_prob()
{
    int nack = 0;
    int trans_complete = 0;
    uint8_t slave_addr = 0x0;
    delay_us(50000);
    while (slave_addr < 0x7f) {
        nack = 0;
        trans_complete = 0;
        I2C0.command[0].val = 0;
        I2C0.command[0].op_code = I2C_OP_START; // start
        I2C0.command[1].val = 0;
        I2C0.command[1].op_code = I2C_OP_WRITE; // write
        I2C0.command[1].ack_en = 1;
        I2C0.command[1].ack_exp = 0;
        I2C0.command[1].ack_val = 0;
        I2C0.command[1].byte_num = 1;
        I2C0.fifo_data.val = ((uint8_t)slave_addr << 1) | 0x0;
        I2C0.command[2].val = 0;
        I2C0.command[2].op_code = I2C_OP_STOP; // stop
        I2C0.ctr.trans_start = 1;

        while (1) {

#if TEST_WITH_OS
            if (xSemaphoreTake(i2c_sem.trans_complete, 10 / portTICK_RATE_MS) == pdTRUE) {
                trans_complete = 1;
            }

            if (xSemaphoreTake(i2c_sem.nack, 10 / portTICK_RATE_MS) == pdTRUE) {
                nack = 1;
            }
#else
            trans_complete = i2c_sem.trans_complete;
            i2c_sem.trans_complete = 0;
            nack = i2c_sem.nack;
            i2c_sem.nack = 0;
#endif
            if (trans_complete == 1 || nack == 1) {
                break;
            }
        }

        if (nack == 0) {
            // printf("slave_addr 0x%x\n", slave_addr);
            nack = 0;
            trans_complete = 0;
            return slave_addr;
        }
        slave_addr++;
    }
    return -1;
}

void i2c_init(int rate, int io_scl, int io_sda)
{
    int force_od = 1;

    i2c_set_pin(io_scl, io_sda, force_od);

    PERI1_CLKRST.peri1_clk_en_ctrl.i2c0_clk_en = 0;
    PERI1_CLKRST.peri1_clk_en_ctrl.i2c0_clk_en = 1;
    PERI1_CLKRST.peri1_rst_ctrl.i2c0_rstn = 0;
    PERI1_CLKRST.peri1_rst_ctrl.i2c0_rstn = 1;

    // div = 1;
    I2C0.clk_conf.sclk_div_num = 4 - 1;
    I2C0.clk_conf.sclk_div_b = 0;
    I2C0.clk_conf.sclk_div_a = 0;
    I2C0.clk_conf.sclk_sel = 0;
    I2C0.clk_conf.sclk_active = 1;

    uint32_t half_cycle = (40000000 / (I2C0.clk_conf.sclk_div_num + 1)) / rate / 2;

    //scl period
    I2C0.scl_low_period.period = half_cycle - 1;
    // default, scl_wait_high < scl_high
    I2C0.scl_high_period.period = half_cycle / 2 + 2;
    I2C0.scl_high_period.scl_wait_high_period = half_cycle - I2C0.scl_high_period.period;
    //sda sample
    I2C0.sda_hold.time = half_cycle / 2;
    // scl_wait_high < sda_sample <= scl_high
    I2C0.sda_sample.time = half_cycle / 2;
    //setup
    I2C0.scl_rstart_setup.time = half_cycle;
    I2C0.scl_stop_setup.time = half_cycle;
    //hold
    I2C0.scl_start_hold.time = half_cycle - 1;
    I2C0.scl_stop_hold.time = half_cycle;

    //default we set the timeout value to 10 bus cycles
    I2C0.timeout.time_out_value = 20;
    I2C0.timeout.time_out_en = 0;
    I2C0.scl_st_time_out.scl_st_to = 20;
    I2C0.scl_main_st_time_out.scl_main_st_to = 20;

    //Disable REF tick;
    I2C0.ctr.val = 0;
    I2C0.ctr.ms_mode = 1;
    I2C0.ctr.sda_force_out = force_od;
    I2C0.ctr.scl_force_out = force_od; // 0 i2c 内部od
    I2C0.ctr.slv_tx_auto_start_en = 0;

    I2C0.fifo_conf.nonfifo_en = 0;
    I2C0.fifo_conf.fifo_prt_en = 1;

    I2C0.ctr.tx_lsb_first = 0;
    I2C0.ctr.rx_lsb_first = 0;

    I2C0.fifo_conf.tx_fifo_rst = 1;
    I2C0.fifo_conf.tx_fifo_rst = 0;

    I2C0.fifo_conf.rx_fifo_rst = 1;
    I2C0.fifo_conf.rx_fifo_rst = 0;

    I2C0.filter_cfg.scl_thres = 15;
    I2C0.filter_cfg.sda_thres = 15;
    I2C0.filter_cfg.scl_en = 1;
    I2C0.filter_cfg.sda_en = 1;

    I2C0.ctr.conf_upgate = 1;

    I2C0.int_ena.val = 0;
    I2C0.int_clr.val = ~0;


#if TEST_WITH_OS
    i2c_sem.trans_complete = xSemaphoreCreateBinary();
    i2c_sem.end = xSemaphoreCreateBinary();
    i2c_sem.slave_stretch = xSemaphoreCreateBinary();
    i2c_sem.nack = xSemaphoreCreateBinary();
#if TEST_INTERRUPT
    esp_intr_alloc(I2C_INTR_SOURCE, ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM, i2c_isr, &i2c_num, NULL);
#endif
#else
    memset(&i2c_sem, 0, sizeof(i2c_sem_t));
#if TEST_INTERRUPT
    ut_isr_alloc(I2C_INTR_SOURCE, i2c_isr, NULL);
#endif
#endif

    I2C0.int_clr.val = ~0;
    I2C0.int_ena.nack = 1;
    I2C0.int_ena.time_out = 1; // 在传输过程中,当 I2C SCL 保持为高或为低电平的时间超过 I2C_TIME_OUT 个模块时钟后,即触发该中断。
    I2C0.int_ena.trans_complete = 1;
    I2C0.int_ena.arbitration_lost = 1; // 当I2C Master 的 SCL 为高电平,SDA 输出值与输入值不相等时,即触发该中断。
    I2C0.int_ena.end_detect = 1;
    I2C0.int_ena.scl_st_to = 1; // 当 I2C 状态机 SCL_FSM 保持某个状态超过 I2C_SCL_ST_TO[23:0] 个模块时钟周期时,触发此中断。
    I2C0.int_ena.scl_main_st_to = 1; // 当 I2C 主状态机 SCL_MAIN_FSM 保持某个状态超过I2C_SCL_MAIN_ST_TO[23:0] 个模块时钟周期时,触发此中断。

    I2C0.scl_stretch_conf.slave_scl_stretch_en = 0;
    I2C0.ctr.conf_upgate = 1;
    I2C0.scl_stretch_conf.slave_scl_stretch_clr = 1;
}
