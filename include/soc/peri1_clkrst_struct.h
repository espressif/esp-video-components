// Copyright 2017-2018 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef _SOC_PERI1_CLKRST_STRUCT_H_
#define _SOC_PERI1_CLKRST_STRUCT_H_


#ifdef __cplusplus
extern "C" {
#endif

typedef volatile struct {
    uint32_t peri1_clk_ver_date;
    union {
        struct {
            uint32_t gtimer_clk_en                 :    1;  /*clock enable*/
            uint32_t stimer_clk_en                 :    1;  /*clock enable*/
            uint32_t timergrp0_clk_en              :    1;  /*clock enable*/
            uint32_t timergrp1_clk_en              :    1;  /*clock enable*/
            uint32_t pwm0_clk_en                   :    1;  /*clock enable*/
            uint32_t pwm1_clk_en                   :    1;  /*clock enable*/
            uint32_t can0_clk_en                   :    1;  /*clock enable*/
            uint32_t can1_clk_en                   :    1;  /*clock enable*/
            uint32_t can2_clk_en                   :    1;  /*clock enable*/
            uint32_t i2c0_clk_en                   :    1;  /*clock enable*/
            uint32_t i2c1_clk_en                   :    1;  /*clock enable*/
            uint32_t usb_device_clk_en             :    1;  /*clock enable*/
            uint32_t ledc_clk_en                   :    1;  /*clock enable*/
            uint32_t timergrp2_clk_en              :    1;  /*clock enable*/
            uint32_t timergrp3_clk_en              :    1;  /*clock enable*/
            uint32_t rmt_clk_en                    :    1;  /*clock enable*/
            uint32_t usb_device_ext_phy_sel        :    1;  /*usb device phy clock sel*/
            uint32_t rng_clk_en                    :    1;  /*clock enable*/
            uint32_t clk_en                        :    1;
            uint32_t reserved19                    :    13;
        };
        uint32_t val;
    } peri1_clk_en_ctrl;
    union {
        struct {
            uint32_t gtimer_rstn                   :    1;  /*software reset: low active*/
            uint32_t stimer_rstn                   :    1;  /*software reset: low active*/
            uint32_t timergrp0_rstn                :    1;  /*software reset: low active*/
            uint32_t timergrp1_rstn                :    1;  /*software reset: low active*/
            uint32_t pwm0_rstn                     :    1;  /*software reset: low active*/
            uint32_t pwm1_rstn                     :    1;  /*software reset: low active*/
            uint32_t can0_rstn                     :    1;  /*software reset: low active*/
            uint32_t can1_rstn                     :    1;  /*software reset: low active*/
            uint32_t can2_rstn                     :    1;  /*software reset: low active*/
            uint32_t i2c0_rstn                     :    1;  /*software reset: low active*/
            uint32_t i2c1_rstn                     :    1;  /*software reset: low active*/
            uint32_t usb_device_rstn               :    1;  /*software reset: low active*/
            uint32_t ledc_rstn                     :    1;  /*software reset: low active*/
            uint32_t timergrp2_rstn                :    1;  /*software reset: low active*/
            uint32_t timergrp3_rstn                :    1;  /*software reset: low active*/
            uint32_t rmt_rstn                      :    1;  /*software reset: low active*/
            uint32_t rng_rstn                      :    1;  /*software reset: low active*/
            uint32_t reserved17                    :    1;
            uint32_t i3c_slave_rstn                :    1;  /*software reset: low active*/
            uint32_t reserved19                    :    13;
        };
        uint32_t val;
    } peri1_rst_ctrl;
    union {
        struct {
            uint32_t gtimer_force_norst            :    1;  /*software force no reset */
            uint32_t stimer_force_norst            :    1;  /*software force no reset */
            uint32_t timergrp0_force_norst         :    1;  /*software force no reset */
            uint32_t timergrp1_force_norst         :    1;  /*software force no reset */
            uint32_t pwm0_force_norst              :    1;  /*software force no reset */
            uint32_t pwm1_force_norst              :    1;  /*software force no reset */
            uint32_t can0_force_norst              :    1;  /*software force no reset */
            uint32_t can1_force_norst              :    1;  /*software force no reset */
            uint32_t can2_force_norst              :    1;  /*software force no reset */
            uint32_t i2c0_force_norst              :    1;  /*software force no reset */
            uint32_t i2c1_force_norst              :    1;  /*software force no reset */
            uint32_t usb_device_force_norst        :    1;  /*software force no reset */
            uint32_t ledc_force_norst              :    1;  /*software force no reset */
            uint32_t timergrp2_force_norst         :    1;  /*software force no reset */
            uint32_t timergrp3_force_norst         :    1;  /*software force no reset */
            uint32_t rmt_force_norst               :    1;  /*software force no reset */
            uint32_t rng_force_norst               :    1;  /*software force no reset */
            uint32_t reserved17                    :    1;  /* */
            uint32_t i3c_slave_force_norst         :    1;  /*software force no reset */
            uint32_t reserved19                    :    13;
        };
        uint32_t val;
    } peri1_force_norst_ctrl;
    union {
        struct {
            uint32_t wdt_hpcore0_rst_len           :    8;  /*hp core0 reset length*/
            uint32_t hpcore0_stall_wait_num        :    8;  /*hp core0 stall wait delay */
            uint32_t hpcore0_stall_en              :    1;  /*stall hp core0 before reset*/
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } hpwdt_core0_rst_ctrl;
    union {
        struct {
            uint32_t wdt_hpcore1_rst_len           :    8;  /*hp core1 reset length*/
            uint32_t hpcore1_stall_wait_num        :    8;  /*hp core1 stall wait delay */
            uint32_t hpcore1_stall_en              :    1;  /*stall hp core1 before reset*/
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } hpwdt_core1_rst_ctrl;
    uint32_t reserved_18;
    uint32_t reserved_1c;
    union {
        struct {
            uint32_t en                            :    1;
            uint32_t bit_out                       :    1;
            uint32_t bit_in                        :    1;
            uint32_t reserved3                     :    29;
        };
        uint32_t val;
    } eco_cell_en_and_data;
    uint32_t rnd_data;
    union {
        struct {
            uint32_t hp_peri1_rdn_eco_en           :    1;
            uint32_t hp_peri1_rdn_eco_result       :    1;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } hp_peri1_rdn_eco_cs;
    uint32_t hp_peri1_rdn_eco_low;
    uint32_t hp_peri1_rdn_eco_high;
} peri1_clkrst_dev_t;
extern peri1_clkrst_dev_t PERI1_CLKRST;
#ifdef __cplusplus
}
#endif



#endif /*_SOC_PERI1_CLKRST_STRUCT_H_ */
