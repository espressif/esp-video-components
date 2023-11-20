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
#ifndef _SOC_LP_SYS_STRUCT_H_
#define _SOC_LP_SYS_STRUCT_H_


#ifdef __cplusplus
extern "C" {
#endif

typedef volatile struct {
    uint32_t reserved_0;
    uint32_t reserved_4;
    uint32_t reserved_8;
    uint32_t reserved_c;
    uint32_t reserved_10;
    uint32_t reserved_14;
    uint32_t reserved_18;
    uint32_t reserved_1c;
    uint32_t ver_date;
    union {
        struct {
            uint32_t sosc_clk_div                  :    8;  /*100K RC oscillator clock divider number*/
            uint32_t sosc_clk_div_vld              :    1;  /*100K RC oscillator clock divider number update value bit*/
            uint32_t reserved9                     :    23;
        };
        uint32_t val;
    } slow_osc_ctrl;
    union {
        struct {
            uint32_t fosc_div_num                  :    3;  /*20M RC oscillator clock divider number update value bit*/
            uint32_t reserved3                     :    5;
            uint32_t fosc_div_vld                  :    1;  /*20M RC oscillator clock divider number update value bit*/
            uint32_t fosc_clk_force_on             :    1;  /*20M RC oscillator clock gating cell force open bit*/
            uint32_t reserved10                    :    22;
        };
        uint32_t val;
    } fast_osc_ctrl;
    union {
        struct {
            uint32_t slow_clk_src_sel              :    2;  /*LP system slow clock source select:
2'b10: 20M RC oscillator divide 256
2'b01: 32K XTAL
2'b00: 100K RC oscillator divided clock*/
            uint32_t reserved2                     :    6;
            uint32_t fast_clk_src_sel              :    1;  /*LP system fast clock source select:
1'b1:20M RC oscillator
1'b0:40M XTAL divide 2*/
            uint32_t reserved9                     :    7;
            uint32_t ena_sw_sel_sys_clk            :    1;  /*Enable Software to select lp_sys_clk source: 
1'b0: lp_sys_clk source is selected by HW, this is the default setting
1'b1: lp_sys_clk source is selected by software*/
            uint32_t sw_sys_clk_src_sel            :    1;  /*lp_sys_clk source selection by software:
1'b0: LP system slow clock
1'b1: LP system fast clock*/
            uint32_t reserved18                    :    14;
        };
        uint32_t val;
    } clk_sel_ctrl;
    union {
        struct {
            uint32_t hp_xtal_force_on              :    1;  /*40M XTAL clock gating cell force open bit*/
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } lp_xtal_ctrl;
    union {
        struct {
            uint32_t slp_req                       :    1;  /*sleep request*/
            uint32_t slp_mode                      :    2;  /*identify sleep mode of the next coming sleep request:
2'b00: HP light sleep
2'b01: HP deep sleep
2'b10: LP light sleep
2'b11: LP deep sleep*/
            uint32_t glitch_rst_en                 :    1;  /*glitch reset enable*/
            uint32_t sys_sw_rst                    :    1;  /*digital system software reset bit*/
            uint32_t force_download_boot           :    1;
            uint32_t lp_fib_sel                    :    3;
            uint32_t pmu_main_st                   :    6;  /*PMU main status*/
            uint32_t io_mux_reset_disable          :    1;  /*reset disable bit for LP IOMUX*/
            uint32_t swd_bypass_rst                :    1;  /*SUPER WATCH dog reset bypass*/
            uint32_t efuse_relaod_after_reset      :    1;  /*Auto update contents in otp to memory and ctrl signals after reset*/
            uint32_t lightslp_keep_xtal_on         :    1;  /*Keep XTAL on under LP lightsleep mode*/
            uint32_t deepslp_keep_xtal_on          :    1;  /*Keep XTAL on under LP deepsleep mode*/
            uint32_t lightslp_keep_fosc_on         :    1;  /*Keep fast oscillator on under LP lightsleep mode*/
            uint32_t deepslp_keep_fosc_on          :    1;  /*Keep fast oscillator on under LP deepsleep mode*/
            uint32_t reserved22                    :    2;
            uint32_t lp_deepslp_dcdc_ls            :    1;  /*DCDC off & DCM enter lightsleep under LP deepsleep mode*/
            uint32_t lp_deepslp_dcdc_ds            :    1;  /*DCDC off & DCM enter deepsleep under LP deepsleep mode*/
            uint32_t lp_lightslp_dcdc_ls           :    1;  /*DCDC off & DCM enter lightsleep under LP lightsleep mode*/
            uint32_t lp_lightslp_dcdc_ds           :    1;  /*DCDC off & DCM enter deepsleep under LP lightsleep mode*/
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } sys_ctrl;
    union {
        struct {
            uint32_t clk_en                        :    1;
            uint32_t lp_intr_clk_en                :    1;  /*clock enable of LP INTC*/
            uint32_t lp_mb_clk_en                  :    1;  /*clock enable of LP mailbox*/
            uint32_t lp_efuse_clk_en               :    1;  /*clock enable of LP EFUSE*/
            uint32_t lp_touch_clk_en               :    1;  /*clock enable of LP touch*/
            uint32_t lp_wdt_clk_en                 :    1;  /*clock enable of LP watchdog*/
            uint32_t lp_rtc_clk_en                 :    1;  /*clock enable of LP TIMER*/
            uint32_t lp_uart_clk_en                :    1;  /*clock enable of LP UART*/
            uint32_t lp_i2s_clk_en                 :    1;  /*clock enable of LP I2S*/
            uint32_t lp_spi_clk_en                 :    1;  /*clock enable of LP SPI*/
            uint32_t lp_adc_clk_en                 :    1;  /*clock enable of LP ADC*/
            uint32_t lp_dac_clk_en                 :    1;  /*clock enable of LP DAC*/
            uint32_t lp_ts_clk_en                  :    1;  /*clock enable of LP temp sensor*/
            uint32_t lp_i2c_clk_en                 :    1;  /*clock enable of LP I2C*/
            uint32_t lp_fosc_hp_clk_en             :    1;  /*clock enable of 20M RC clock from LP to HP*/
            uint32_t lp_fosc_d256_hp_clk_en        :    1;  /*clock enable of 20M RC divided clock from LP to HP*/
            uint32_t lp_xtal_hp_clk_en             :    1;  /*clock enable of XTAL clock from LP to HP*/
            uint32_t xtal32k_gpio_sel              :    1;  /*clock source select for 32K xtal*/
            uint32_t stimer_clk_en                 :    1;  /*clock enable for STIMER*/
            uint32_t stimer_core_clk_sel           :    1;  /*clock select for STIMER core clock*/
            uint32_t lp_i3c_slv_clk_en             :    1;  /*clock enable for i3c slave clock */
            uint32_t lp_i3c_mst_clk_en             :    1;  /*clock enable for i3c master clock */
            uint32_t reserved22                    :    10;
        };
        uint32_t val;
    } lp_clk_ctrl;
    union {
        struct {
            uint32_t lp_intr_rstn                  :    1;  /*LP INTC reset*/
            uint32_t lp_mb_rstn                    :    1;  /*LP mailbox reset*/
            uint32_t lp_touch_rstn                 :    1;  /*LP touch reset*/
            uint32_t lp_wdt_rstn                   :    1;  /*LP watchdog reset*/
            uint32_t lp_rtc_rstn                   :    1;  /*LP TIMER reset*/
            uint32_t lp_uart_rstn                  :    1;  /*LP UART reset*/
            uint32_t lp_i2s_rstn                   :    1;  /*LP I2S reset*/
            uint32_t lp_spi_rstn                   :    1;  /*LP SPI reset*/
            uint32_t lp_adc_rstn                   :    1;  /*LP ADC reset*/
            uint32_t lp_dac_rstn                   :    1;  /*LP DAC reset*/
            uint32_t lp_ts_rstn                    :    1;  /*LP temp sensor reset*/
            uint32_t lp_i2c_rstn                   :    1;  /*LP I2C reset*/
            uint32_t lp_cpu_rstn                   :    1;  /*LP CPU core reset*/
            uint32_t lp_tcm_rstn                   :    1;  /*LP TCM reset*/
            uint32_t stimer_rstn                   :    1;  /*Stimer reset*/
            uint32_t i2c_mst_rstn                  :    1;  /*Stimer reset*/
            uint32_t ana_rst_bypass                :    1;  /*analog source reset bypass : wdt,brown out,super wdt,glitch*/
            uint32_t sys_rst_bypass                :    1;  /*system source reset bypass : software reset,hp wdt,lp wdt,efuse*/
            uint32_t lp_i3c_slv_rstn               :    1;  /*i3c slave reset */
            uint32_t lp_i3c_mst_rstn               :    1;  /*i3c master reset*/
            uint32_t lp_icm_rstn                   :    1;  /*Lp icm reset*/
            uint32_t efuse_force_norst             :    1;  /*efuse force no reset control*/
            uint32_t reserved22                    :    10;
        };
        uint32_t val;
    } lp_rst_ctrl;
    union {
        struct {
            uint32_t hpcore0_sw_rst                :    1;  /*hp core0 SW reset*/
            uint32_t lp_wdt_hpcore0_rst_en         :    1;
            uint32_t lp_wdt_hpcore0_rst_len        :    3;
            uint32_t hpcore0_stall_wait_num        :    5;
            uint32_t hpcore0_stall_en              :    1;
            uint32_t hpcore0_stat_vector_sel       :    1;  /*1'b1: boot from HP TCM ROM: 0x30000400
1'b0: boot from LP TCM RAM:  0x50088400*/
            uint32_t reserved12                    :    20;
        };
        uint32_t val;
    } hp_core0_rst_ctrl;
    union {
        struct {
            uint32_t hpcore1_sw_rst                :    1;  /*hp core1 SW reset*/
            uint32_t lp_wdt_hpcore1_rst_en         :    1;
            uint32_t lp_wdt_hpcore1_rst_len        :    3;
            uint32_t hpcore1_stall_wait_num        :    5;
            uint32_t hpcore1_stall_en              :    1;
            uint32_t hpcore1_stat_vector_sel       :    1;  /*1'b1: boot from HP TCM ROM: 0x30000400
1'b0: boot from LP TCM RAM:  0x50088400*/
            uint32_t reserved12                    :    20;
        };
        uint32_t val;
    } hp_core1_rst_ctrl;
    union {
        struct {
            uint32_t hpcore2_sw_rst                :    1;  /*hp core2 SW reset*/
            uint32_t lp_wdt_hpcore2_rst_en         :    1;
            uint32_t lp_wdt_hpcore2_rst_len        :    3;
            uint32_t hpcore2_stall_wait_num        :    5;
            uint32_t hpcore2_stall_en              :    1;
            uint32_t hpcore2_stat_vector_sel       :    1;  /*1'b1: boot from HP TCM ROM: 0x30000400
1'b0: boot from LP TCM RAM:  0x50088400*/
            uint32_t reserved12                    :    20;
        };
        uint32_t val;
    } hp_core2_rst_ctrl;
    union {
        struct {
            uint32_t hpcore3_sw_rst                :    1;  /*hp core3 SW reset*/
            uint32_t lp_wdt_hpcore3_rst_en         :    1;
            uint32_t lp_wdt_hpcore3_rst_len        :    3;
            uint32_t hpcore3_stall_wait_num        :    5;
            uint32_t hpcore3_stall_en              :    1;
            uint32_t hpcore3_stat_vector_sel       :    1;  /*1'b1: boot from HP TCM ROM: 0x30000400
1'b0: boot from LP TCM RAM:  0x50088400*/
            uint32_t reserved12                    :    20;
        };
        uint32_t val;
    } hp_core3_rst_ctrl;
    union {
        struct {
            uint32_t hpcore0_sw_stall_code         :    8;  /*HP core0 software stall when set to 8'h86*/
            uint32_t hpcore1_sw_stall_code         :    8;  /*HP core1 software stall when set to 8'h86*/
            uint32_t hpcore2_sw_stall_code         :    8;  /*HP core2 software stall when set to 8'h86*/
            uint32_t hpcore3_sw_stall_code         :    8;  /*HP core3 software stall when set to 8'h86*/
        };
        uint32_t val;
    } hp_stall_ctrl;
    union {
        struct {
            uint32_t hpcore3_reset_cause           :    6;  /*6'h1: POR reset
6'h2: external reset
6'h3: digital system software reset
6'h5: PMU HP system power down reset
6'h6: PMU HP CPU power down reset
6'h7: HP system reset from HP watchdog
6'h9: HP system reset from LP watchdog
6'hb: HP core reset from HP watchdog
6'hc: HP core software reset
6'hd: HP core reset from LP watchdog
6'hf: brown out reset
6'h10: LP watchdog chip reset
6'h12: super watch dog reset
6'h13: glitch reset
6'h14: efuse crc error reset
6'h15: HP sdio chip reset
6'h16: HP usb jtag chip reset
6'h17: HP usb uart chip reset*/
            uint32_t hpcore3_reset_flag            :    1;
            uint32_t hpcore3_reset_flag_clr        :    1;
            uint32_t hpcore2_reset_cause           :    6;  /*6'h1: POR reset
6'h2: external reset
6'h3: digital system software reset
6'h5: PMU HP system power down reset
6'h6: PMU HP CPU power down reset
6'h7: HP system reset from HP watchdog
6'h9: HP system reset from LP watchdog
6'hb: HP core reset from HP watchdog
6'hc: HP core software reset
6'hd: HP core reset from LP watchdog
6'hf: brown out reset
6'h10: LP watchdog chip reset
6'h12: super watch dog reset
6'h13: glitch reset
6'h14: efuse crc error reset
6'h15: HP sdio chip reset
6'h16: HP usb jtag chip reset
6'h17: HP usb uart chip reset*/
            uint32_t hpcore2_reset_flag            :    1;
            uint32_t hpcore2_reset_flag_clr        :    1;
            uint32_t hpcore1_reset_cause           :    6;  /*6'h1: POR reset
6'h2: external reset
6'h3: digital system software reset
6'h5: PMU HP system power down reset
6'h6: PMU HP CPU power down reset
6'h7: HP system reset from HP watchdog
6'h9: HP system reset from LP watchdog
6'hb: HP core reset from HP watchdog
6'hc: HP core software reset
6'hd: HP core reset from LP watchdog
6'hf: brown out reset
6'h10: LP watchdog chip reset
6'h12: super watch dog reset
6'h13: glitch reset
6'h14: efuse crc error reset
6'h15: HP sdio chip reset
6'h16: HP usb jtag chip reset
6'h17: HP usb uart chip reset*/
            uint32_t hpcore1_reset_flag            :    1;
            uint32_t hpcore1_reset_flag_clr        :    1;
            uint32_t hpcore0_reset_cause           :    6;  /*6'h1: POR reset
6'h2: external reset
6'h3: digital system software reset
6'h5: PMU HP system power down reset
6'h6: PMU HP CPU power down reset
6'h7: HP system reset from HP watchdog
6'h9: HP system reset from LP watchdog
6'hb: HP core reset from HP watchdog
6'hc: HP core software reset
6'hd: HP core reset from LP watchdog
6'hf: brown out reset
6'h10: LP watchdog chip reset
6'h12: super watch dog reset
6'h13: glitch reset
6'h14: efuse crc error reset
6'h15: HP sdio chip reset
6'h16: HP usb jtag chip reset
6'h17: HP usb uart chip reset*/
            uint32_t hpcore0_reset_flag            :    1;
            uint32_t hpcore0_reset_flag_clr        :    1;
        };
        uint32_t val;
    } hp_core_rst_status;
    union {
        struct {
            uint32_t hp_wakeup_src_en              :    9;  /*Bit[0]: hp uart0 wakeup enable
Bit[1]: hp uart1 wakeup enable
Bit[2]: hp uart2 wakeup enable
Bit[3]: hp uart3 wakeup enable
Bit[4]: hp uart4 wakeup enable
Bit[5]: usb wakeup enable
Bit[6]: hp gpio wakeup enable
Bit[7]: hp software wakeup enable
Bit[8]: sdio slave wakeup enable*/
            uint32_t reserved9                     :    3;
            uint32_t hp_wakeup_cause               :    9;  /*Bit[0]: hp uart0 wakeup
Bit[1]: hp uart1 wakeup
Bit[2]: hp uart2 wakeup
Bit[3]: hp uart3 wakeup
Bit[4]: hp uart4 wakeup
Bit[5]: usb wakeup
Bit[6]: hp gpio wakeup
Bit[7]: hp software wakeup
Bit[8]: sdio slave wakeup*/
            uint32_t reserved21                    :    3;
            uint32_t hp_wakeup_cause_clr           :    1;  /*HP wakeup cause clear*/
            uint32_t hp_lp_wakeup                  :    1;  /*HP software wakeup*/
            uint32_t gpio_wakeup_filter            :    1;
            uint32_t ext_wakeup0_lv                :    1;
            uint32_t ext_wakeup1_lv                :    1;
            uint32_t reserved29                    :    3;
        };
        uint32_t val;
    } hp_wakeup_ctrl;
    union {
        struct {
            uint32_t lp_wakeup_src_en              :    15;  /*Bit[14]: monitor_timer3_wakeup enable  
Bit[13]: monitor_timer2_wakeup enable
Bit[12]: monitor_timer1_wakeup enable
Bit[11]: monitor_timer0_wakeup enable
Bit[10]: lp gpio wakeup enable
Bit[9]: lp_uart_wakeup enable
Bit[8]: lp_tsens_wakeup enable
Bit[7]: lp_spi_wakeup enable
Bit[6]: lp_i2s_wakeup enable
Bit[5]: lp_i2c_wakeup enable
Bit[4]: lp_adc_wakeup enable
Bit[3]: lp_touch_wakeup enable
Bit[2]: lp_tmr_wakeup enable
Bit[1]: lp_ext1_wakeup enable
Bit[0]: lp_ext0_wakeup enable*/
            uint32_t reserved15                    :    1;
            uint32_t lp_wakeup_cause               :    15;  /*Bit[30]: monitor_timer3_wakeup
Bit[29]: monitor_timer2_wakeup
Bit[28]: monitor_timer1_wakeup
Bit[27]: monitor_timer0_wakeup
Bit[26]: lp gpio wakeup
Bit[25]: lp_uart_wakeup
Bit[24]: lp_tsens_wakeup
Bit[23]: lp_spi_wakeup
Bit[22]: lp_i2s_wakeup
Bit[21]: lp_i2c_wakeup
Bit[20]: lp_adc_wakeup
Bit[19]: lp_touch_wakeup
Bit[18]: lp_tmr_wakeup
Bit[17]: lp_ext1_wakeup
Bit[16]: lp_ext0_wakeup*/
            uint32_t lp_wakeup_cause_clr           :    1;
        };
        uint32_t val;
    } lp_wakeup_ctrl;
    union {
        struct {
            uint32_t hp_lightslp_reject_en         :    1;
            uint32_t hp_deepslp_reject_en          :    1;
            uint32_t hp_sleep_reject_src_en        :    9;  /*Bit[0]: hp uart0 wakeup active reject enable
Bit[1]: hp uart1 wakeup active reject enable
Bit[2]: hp uart2 wakeup active reject enable
Bit[3]: hp uart3 wakeup active reject enable
Bit[4]: hp uart4 wakeup active reject enable
Bit[5]: hp usb wakeup active reject enable
Bit[6]: hp gpio wakeup active reject enable
Bit[7]: hp software wakeup active reject enable
Bit[8]: hp sdio slave wakeup active reject enable*/
            uint32_t reserved11                    :    5;
            uint32_t hp_slp_reject_cause           :    9;  /*Bit[0]: hp uart0 wakeup active
Bit[1]: hp uart1 wakeup active
Bit[2]: hp uart2 wakeup active
Bit[3]: hp uart3 wakeup active
Bit[4]: hp uart4 wakeup active
Bit[5]: hp usb wakeup active
Bit[6]: hp gpio wakeup active
Bit[7]: hp software wakeup active
Bit[8]: hp sdio slave wakeup active*/
            uint32_t reserved25                    :    6;
            uint32_t hp_slp_reject_cause_clr       :    1;
        };
        uint32_t val;
    } hp_sleep_rej_ctrl;
    union {
        struct {
            uint32_t lp_lightslp_reject_en         :    1;
            uint32_t lp_deepslp_reject_en          :    1;
            uint32_t lp_sleep_reject_src_en        :    11;  /*Bit[10]: lp_gpio_wakeup active reject enable
Bit[9]: lp_uart_wakeup active reject enable
Bit[8]: lp_tsens_wakeup active reject enable
Bit[7]: lp_spi_wakeup active reject enable
Bit[6]: lp_i2s_wakeup active reject enable
Bit[5]: lp_i2c_wakeup active reject enable
Bit[4]: lp_adc_wakeup active reject enable
Bit[3]: lp_touch_wakeup active reject enable
Bit[2]: lp_tmr_wakeup active reject enable
Bit[1]: lp_ext1_wakeup active reject enable
Bit[0]: lp_ext0_wakeup active reject enable*/
            uint32_t lp_slp_reject_cause           :    11;  /*Bit[10]: lp_gpio_wakeup active
Bit[9]: lp_uart_wakeup active
Bit[8]: lp_tsens_wakeup active
Bit[7]: lp_spi_wakeup active
Bit[6]: lp_i2s_wakeup active
Bit[5]: lp_i2c_wakeup active
Bit[4]: lp_adc_wakeup active
Bit[3]: lp_touch_wakeup active
Bit[2]: lp_tmr_wakeup active
Bit[1]: lp_ext1_wakeup active
Bit[0]: lp_ext0_wakeup active*/
            uint32_t lp_slp_reject_cause_clr       :    1;
            uint32_t reserved25                    :    7;
        };
        uint32_t val;
    } lp_sleep_rej_ctrl;
    union {
        struct {
            uint32_t ana_dig_force_iso             :    1;
            uint32_t ana_dig_force_noiso           :    1;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } ana_i2c_iso_ctrl;
    union {
        struct {
            uint32_t apll_cal_slp_start            :    1;
            uint32_t apll_force_pd                 :    1;
            uint32_t apll_force_pu                 :    1;
            uint32_t reserved3                     :    29;
        };
        uint32_t val;
    } apll_pwr_ctrl;
    union {
        struct {
            uint32_t bias_buf_deepslp_xpd          :    1;
            uint32_t bias_buf_idle_xpd             :    1;
            uint32_t bias_buf_monitor_xpd          :    1;
            uint32_t bias_buf_wait_xtal_xpd        :    1;
            uint32_t bias_deepslp                  :    1;
            uint32_t bias_monitor                  :    1;
            uint32_t dbg_atten_deepslp             :    4;
            uint32_t dbg_atten_monitor             :    4;
            uint32_t pd_cur_deepslp                :    1;
            uint32_t pd_cur_monitor                :    1;
            uint32_t reserved16                    :    16;
        };
        uint32_t val;
    } bias_pwr_ctrl;
    union {
        struct {
            uint32_t fosc_div_enb                  :    1;
            uint32_t fosc_div                      :    2;
            uint32_t fosc_dfreq                    :    10;
            uint32_t fosc_enb                      :    1;
            uint32_t fosc_force_pd                 :    1;
            uint32_t fosc_force_pu                 :    1;
            uint32_t reserved16                    :    16;
        };
        uint32_t val;
    } fast_rc_pwr_ctrl;
    union {
        struct {
            uint32_t hp_cpu_force_iso              :    1;  /*hp cpu power domain isolation force on*/
            uint32_t hp_cpu_force_noiso            :    1;  /*hp cpu power domain isolation force off*/
            uint32_t hp_cpu_force_pd               :    1;  /*hp cpu power domain force power off*/
            uint32_t hp_cpu_force_pu               :    1;  /*hp cpu power domain force power on*/
            uint32_t hp_cpu_pd_en                  :    1;  /*hp cpu power domain power down enable*/
            uint32_t hp_cpu_stall_wait_num         :    10;  /*hp cpu stall wait cycle num when light sleep*/
            uint32_t reserved15                    :    17;
        };
        uint32_t val;
    } hp_cpu_pwr_ctrl;
    union {
        struct {
            uint32_t hp_pad_force_unhold           :    1;  /*hp pad hold force off*/
            uint32_t hp_pad_force_hold             :    1;  /*hp pad hold force on*/
            uint32_t hp_pad_autohold_en            :    1;  /*hp pad auto-hold enable*/
            uint32_t clr_hp_pad_autohold           :    1;  /*hp pad autohold clear*/
            uint32_t hp_pad_force_noiso            :    1;  /*hp pad isolation force off*/
            uint32_t hp_pad_force_iso              :    1;  /*hp pad isolation force on*/
            uint32_t reserved6                     :    26;
        };
        uint32_t val;
    } hp_pad_pwr_ctrl;
    union {
        struct {
            uint32_t hp_peri_force_iso             :    1;  /*hp peripheral power domain isolation force on*/
            uint32_t hp_peri_force_noiso           :    1;  /*hp peripheral power domain isolation force off*/
            uint32_t hp_peri_force_pd              :    1;  /*hp peripheral power domain force power off*/
            uint32_t hp_peri_force_pu              :    1;  /*hp peripheral power domain force power on*/
            uint32_t hp_peri_pd_en                 :    1;  /*hp peripheral power domain power down enable*/
            uint32_t reserved5                     :    27;
        };
        uint32_t val;
    } hp_peri_pwr_ctrl;
    union {
        struct {
            uint32_t hp_top_force_iso              :    1;  /*hp top power domain isolation force off*/
            uint32_t hp_top_force_noiso            :    1;  /*hp top power domain isolation force off*/
            uint32_t hp_top_force_pd               :    1;  /*hp top power domain force power off*/
            uint32_t hp_top_force_pu               :    1;  /*hp top power domain force power on*/
            uint32_t hp_top_pd_en                  :    1;  /*hp top power domain power down enable*/
            uint32_t xtal_pd_en                    :    1;  /*HP XTAL power down enable*/
            uint32_t pll_pd_en                     :    1;  /*HP system PLL power down enable*/
            uint32_t hp_sleep_wait_num             :    10;  /*minimum sleep cycles for HP*/
            uint32_t ana_dig_en_cal                :    1;  /*for hp LDO*/
            uint32_t reserved18                    :    6;
            uint32_t hp_deepslp_dcdc_ls            :    1;  /*DCDC off & DCM enter lightsleep under HP deepsleep mode*/
            uint32_t hp_deepslp_dcdc_ds            :    1;  /*DCDC off & DCM enter deepsleep under HP deepsleep mode*/
            uint32_t hp_deepslp_dcdc_off           :    1;  /* both DCDC & DCM off under HP deepsleep mode*/
            uint32_t hp_lightslp_dcdc_ls           :    1;  /*DCDC off & DCM enter lightsleep under HP lightsleep mode*/
            uint32_t hp_lightslp_dcdc_ds           :    1;  /*DCDC off & DCM enter deepsleep under HP lightsleep mode*/
            uint32_t reserved29                    :    3;
        };
        uint32_t val;
    } hp_top_pwr_ctrl;
    union {
        struct {
            uint32_t hp_xtal_stable_wait_num       :    10;  /*wait cycle number for XTAL stable*/
            uint32_t hp_power_stable_wait_num      :    10;  /*wait cycle number for HP power stable*/
            uint32_t hp_pll_stable_wait_num        :    10;  /*wait cycle number for system PLL stable*/
            uint32_t reserved30                    :    2;
        };
        uint32_t val;
    } hp_wakeup_wait_num;
    union {
        struct {
            uint32_t lp_cpu_force_iso              :    1;  /*LP CPU power domain isolation force on*/
            uint32_t lp_cpu_force_noiso            :    1;  /*LP CPU power domain isolation force off*/
            uint32_t lp_cpu_force_pd               :    1;  /*LP CPU power domain force power down*/
            uint32_t lp_cpu_force_pu               :    1;  /*LP CPU power domain force power on*/
            uint32_t lp_cpu_pd_en                  :    1;  /*LP CPU power domain power down enable*/
            uint32_t lp_cpu_stall_wait_num         :    10;  /*stall cycle number for LP light sleep*/
            uint32_t lp_sleep_wait_num             :    10;  /*minimum sleep cycles for LP*/
            uint32_t lp_ldo_force_pu               :    1;  /*LP LDO power force power on*/
            uint32_t reserved26                    :    6;
        };
        uint32_t val;
    } lp_cpu_pwr_ctrl;
    union {
        struct {
            uint32_t lp_peri0_force_iso            :    1;  /*LP peripheral 0 power domain isolation force on*/
            uint32_t lp_peri0_force_noiso          :    1;  /*LP peripheral 0 power domain isolation force off*/
            uint32_t lp_peri0_force_pd             :    1;  /*LP peripheral 0 power domain force power off*/
            uint32_t lp_peri0_force_pu             :    1;  /*LP peripheral 0 power domain force power on*/
            uint32_t lp_peri0_pd_en                :    1;  /*LP peripheral 0 power domain power down enable*/
            uint32_t lp_pad_force_hold             :    1;
            uint32_t reserved6                     :    26;
        };
        uint32_t val;
    } lp_peri0_pwr_ctrl;
    union {
        struct {
            uint32_t lp_peri1_force_iso            :    1;  /*LP peripheral 1 power domain force isolation on*/
            uint32_t lp_peri1_force_noiso          :    1;  /*LP peripheral 1 power domain force isolation off*/
            uint32_t lp_peri1_force_pd             :    1;  /*LP peripheral 1 power domain force power off*/
            uint32_t lp_peri1_force_pu             :    1;  /*LP peripheral 1 power domain force power on*/
            uint32_t lp_peri1_pd_en                :    1;  /*LP peripheral 1 power domain power down enable*/
            uint32_t reserved5                     :    27;
        };
        uint32_t val;
    } lp_peri1_pwr_ctrl;
    union {
        struct {
            uint32_t lp_fosc_stable_wait_num       :    10;  /*wait cycle number for LP fast clock stable*/
            uint32_t lp_power_stable_wait_num      :    10;  /*wait cycle number for LP power stable*/
            uint32_t reserved20                    :    12;
        };
        uint32_t val;
    } lp_wakeup_wait_num;
    union {
        struct {
            uint32_t monitor_timer0_en             :    1;  /*monitor timer0 enable*/
            uint32_t monitor_timer1_en             :    1;  /*monitor timer1 enable*/
            uint32_t monitor_timer2_en             :    1;  /*monitor timer2 enable*/
            uint32_t monitor_timer3_en             :    1;  /*monitor timer3 enable*/
            uint32_t rtc_cali_en                   :    1;  /*calibration enable for RTC*/
            uint32_t monitor_timer0_work_done      :    1;  /*monitor work done, start next sleep cycle of timer0*/
            uint32_t monitor_timer1_work_done      :    1;  /*monitor work done, start next sleep cycle of timer1*/
            uint32_t monitor_timer2_work_done      :    1;  /*monitor work done, start next sleep cycle of timer2*/
            uint32_t monitor_timer3_work_done      :    1;  /*monitor work done, start next sleep cycle of timer3*/
            uint32_t reserved9                     :    7;
            uint32_t monitor_timer0_wakeup_int     :    1;  /*moniter timer0 wakeup interrupt*/
            uint32_t monitor_timer1_wakeup_int     :    1;  /*moniter timer1 wakeup interrupt*/
            uint32_t monitor_timer2_wakeup_int     :    1;  /*moniter timer2 wakeup interrupt */
            uint32_t monitor_timer3_wakeup_int     :    1;  /*moniter timer3 wakeup interrupt*/
            uint32_t reserved20                    :    4;
            uint32_t monitor_timer0_int_clr        :    1;  /*write 1 to clear monitor timer3 wakeup interrupt*/
            uint32_t monitor_timer1_int_clr        :    1;  /*write 1 to clear monitor timer2 wakeup interrupt*/
            uint32_t monitor_timer2_int_clr        :    1;  /*write 1 to clear monitor timer1 wakeup interrupt*/
            uint32_t monitor_timer3_int_clr        :    1;  /*write 1 to clear monitor timer0 wakeup interrupt*/
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } moni_timer_ctrl;
    uint32_t moni_timer0_slp_wait_num;
    uint32_t moni_timer1_slp_wait_num;
    uint32_t moni_timer2_slp_wait_num;
    uint32_t moni_timer3_slp_wait_num;
    uint32_t rtc_cali_slp_wait_num;
    uint32_t reserved_b4;
    union {
        struct {
            uint32_t sleep_por_force_on            :    1;
            uint32_t sleep_por_force_off           :    1;
            uint32_t sar_i2c_xpd                   :    1;
            uint32_t reserved3                     :    29;
        };
        uint32_t val;
    } ana_i2c_pwr_ctrl;
    union {
        struct {
            uint32_t spll_cal_slp_start            :    1;
            uint32_t spll_force_pd                 :    1;
            uint32_t spll_force_pu                 :    1;
            uint32_t spll_i2c_force_pd             :    1;
            uint32_t spll_i2c_force_pu             :    1;
            uint32_t reserved5                     :    11;
            uint32_t cpll_cal_slp_start            :    1;
            uint32_t cpll_force_pd                 :    1;
            uint32_t cpll_force_pu                 :    1;
            uint32_t cpll_i2c_force_pd             :    1;
            uint32_t cpll_i2c_force_pu             :    1;
            uint32_t reserved21                    :    11;
        };
        uint32_t val;
    } spll_pwr_ctrl;
    union {
        struct {
            uint32_t xtal_32k_xpd                  :    1;
            uint32_t xtal_32k_force                :    1;
            uint32_t xtal_32k_dbuf                 :    1;
            uint32_t xtal_32k_dgm                  :    3;
            uint32_t xtal_32k_dres                 :    3;
            uint32_t xtal_32k_dac                  :    3;
            uint32_t rc32k_xpd                     :    1;
            uint32_t rc32k_dfreq                   :    10;
            uint32_t reserved23                    :    9;
        };
        uint32_t val;
    } slow_xtal_pwr_ctrl;
    union {
        struct {
            uint32_t xtal_force_pd                 :    1;
            uint32_t xtal_force_pu                 :    1;
            uint32_t xtal_ext_ctr_en               :    1;
            uint32_t xtal_ext_ctr_lv               :    1;
            uint32_t xtal_en_wait                  :    4;
            uint32_t xtal_fall_delay_num           :    10;
            uint32_t reserved18                    :    14;
        };
        uint32_t val;
    } xtal_pwr_ctrl;
    union {
        struct {
            uint32_t brown_out_ena                 :    1;
            uint32_t brown_out_cnt_clr             :    1;
            uint32_t brown_out_rst_ena             :    1;
            uint32_t brown_out_rst_sel             :    1;
            uint32_t brown_out_close_flash_ena     :    1;
            uint32_t brown_out_rst_wait_num        :    10;
            uint32_t brown_out_int_wait_num        :    10;
            uint32_t brown_out_ana_rst_en          :    1;
            uint32_t brown_out_det                 :    1;
            uint32_t reserved27                    :    5;
        };
        uint32_t val;
    } brown_out_ctrl;
    uint32_t lp_core_boot_addr;
    union {
        struct {
            uint32_t ext_wakeup1_sel               :    24;  /*Bitmap to select RTC pads for ext wakeup1*/
            uint32_t ext_wakeup1_status_clr        :    1;  /*clear ext wakeup1 status*/
            uint32_t reserved25                    :    7;
        };
        uint32_t val;
    } ext_wakeup1;
    union {
        struct {
            uint32_t ext_wakeup1_status            :    24;  /*ext wakeup1 status*/
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } ext_wakeup1_status;
    union {
        struct {
            uint32_t deepslp_tcm_rom_sd            :    1;
            uint32_t deepslp_tcm_rom_ls            :    1;
            uint32_t deepslp_tcm_ram_sd            :    1;
            uint32_t deepslp_tcm_ram_ds            :    1;
            uint32_t deepslp_tcm_ram_ls            :    1;
            uint32_t lp_tcm_rom_clk_force_on       :    1;
            uint32_t reserved6                     :    1;
            uint32_t lp_tcm_ram_clk_force_on       :    1;
            uint32_t lightslp_tcm_rom_sd           :    1;
            uint32_t lightslp_tcm_rom_ls           :    1;
            uint32_t lightslp_tcm_ram_sd           :    1;
            uint32_t lightslp_tcm_ram_ds           :    1;
            uint32_t lightslp_tcm_ram_ls           :    1;
            uint32_t reserved13                    :    19;
        };
        uint32_t val;
    } lp_tcm_pwr_ctrl;
    uint32_t boot_addr_hp_lp;
    uint32_t lp_store0;
    uint32_t lp_store1;
    uint32_t lp_store2;
    uint32_t lp_store3;
    uint32_t lp_store4;
    uint32_t lp_store5;
    uint32_t lp_store6;
    uint32_t lp_store7;
    union {
        struct {
            uint32_t lp_slp_wakeup_int_ena         :    1;  /*enable sleep wakeup interrupt*/
            uint32_t lp_slp_reject_int_ena         :    1;  /*enable sleep reject interrupt*/
            uint32_t hp_slp_wakeup_int_ena         :    1;  /*enable sleep wakeup interrupt*/
            uint32_t hp_slp_reject_int_ena         :    1;  /*enable sleep reject interrupt*/
            uint32_t brown_out_int_ena             :    1;  /*enable brown out interrupt*/
            uint32_t glitch_det_int_ena            :    1;  /*enable glitch detect interrupt*/
            uint32_t ana_0p1a_cnt_target0_reach_0_int_ena:    1;  /*reg_0p1a_0_counter after xpd reach target0*/
            uint32_t ana_0p1a_cnt_target1_reach_0_int_ena:    1;  /*reg_0p1a_1_counter after xpd reach target1*/
            uint32_t ana_0p1a_cnt_target0_reach_1_int_ena:    1;  /*reg_0p1a_0 counter after xpd reach target0*/
            uint32_t ana_0p1a_cnt_target1_reach_1_int_ena:    1;  /*reg_0p1a_1_counter after xpd reach target1*/
            uint32_t ana_0p2a_cnt_target0_reach_0_int_ena:    1;  /*reg_0p2a_0 counter after xpd reach target0*/
            uint32_t ana_0p2a_cnt_target1_reach_0_int_ena:    1;  /*reg_0p2a_1_counter after xpd reach target1*/
            uint32_t ana_0p2a_cnt_target0_reach_1_int_ena:    1;  /*reg_0p2a_0 counter after xpd reach target0*/
            uint32_t ana_0p2a_cnt_target1_reach_1_int_ena:    1;  /*reg_0p2a_1_counter after xpd reach target1*/
            uint32_t ana_0p3a_cnt_target0_reach_0_int_ena:    1;  /*reg_0p3a_0 counter after xpd reach target0*/
            uint32_t ana_0p3a_cnt_target1_reach_0_int_ena:    1;  /*reg_0p3a_1_counter after xpd reach target1*/
            uint32_t ana_0p3a_cnt_target0_reach_1_int_ena:    1;  /*reg_0p3a_0_counter after xpd reach target0*/
            uint32_t ana_0p3a_cnt_target1_reach_1_int_ena:    1;  /*reg_0p3a_1_counter after xpd reach target1*/
            uint32_t reserved18                    :    14;
        };
        uint32_t val;
    } pmu_int_ena;
    union {
        struct {
            uint32_t lp_slp_wakeup_int_raw         :    1;  /*sleep wakeup interrupt raw*/
            uint32_t lp_slp_reject_int_raw         :    1;  /*sleep reject interrupt raw*/
            uint32_t hp_slp_wakeup_int_raw         :    1;  /*sleep wakeup interrupt raw*/
            uint32_t hp_slp_reject_int_raw         :    1;  /*sleep reject interrupt raw*/
            uint32_t brown_out_int_raw             :    1;  /*brown out interrupt raw*/
            uint32_t glitch_det_int_raw            :    1;  /*glitch detect interrupt raw*/
            uint32_t ana_0p1a_cnt_target0_reach_0_int_raw:    1;  /*reg_0p1a_0_counter after xpd reach target0*/
            uint32_t ana_0p1a_cnt_target1_reach_0_int_raw:    1;  /*reg_0p1a_1_counter after xpd reach target1*/
            uint32_t ana_0p1a_cnt_target0_reach_1_int_raw:    1;  /*reg_0p1a_0 counter after xpd reach target0*/
            uint32_t ana_0p1a_cnt_target1_reach_1_int_raw:    1;  /*reg_0p1a_1_counter after xpd reach target1*/
            uint32_t ana_0p2a_cnt_target0_reach_0_int_raw:    1;  /*reg_0p2a_0 counter after xpd reach target0*/
            uint32_t ana_0p2a_cnt_target1_reach_0_int_raw:    1;  /*reg_0p2a_1_counter after xpd reach target1*/
            uint32_t ana_0p2a_cnt_target0_reach_1_int_raw:    1;  /*reg_0p2a_0 counter after xpd reach target0*/
            uint32_t ana_0p2a_cnt_target1_reach_1_int_raw:    1;  /*reg_0p2a_1_counter after xpd reach target1*/
            uint32_t ana_0p3a_cnt_target0_reach_0_int_raw:    1;  /*reg_0p3a_0 counter after xpd reach target0*/
            uint32_t ana_0p3a_cnt_target1_reach_0_int_raw:    1;  /*reg_0p3a_1_counter after xpd reach target1*/
            uint32_t ana_0p3a_cnt_target0_reach_1_int_raw:    1;  /*reg_0p3a_0_counter after xpd reach target0*/
            uint32_t ana_0p3a_cnt_target1_reach_1_int_raw:    1;  /*reg_0p3a_1_counter after xpd reach target1*/
            uint32_t reserved18                    :    14;
        };
        uint32_t val;
    } pmu_int_raw;
    union {
        struct {
            uint32_t lp_slp_wakeup_int_st          :    1;  /*sleep wakeup interrupt state*/
            uint32_t lp_slp_reject_int_st          :    1;  /*sleep reject interrupt state*/
            uint32_t hp_slp_wakeup_int_st          :    1;  /*sleep wakeup interrupt state*/
            uint32_t hp_slp_reject_int_st          :    1;  /*sleep reject interrupt state*/
            uint32_t brown_out_int_st              :    1;  /*brown out interrupt state*/
            uint32_t glitch_det_int_st             :    1;  /*glitch detect interrupt state*/
            uint32_t ana_0p1a_cnt_target0_reach_0_int_st:    1;  /*reg_0p1a_0_counter after xpd reach target0*/
            uint32_t ana_0p1a_cnt_target1_reach_0_int_st:    1;  /*reg_0p1a_1_counter after xpd reach target1*/
            uint32_t ana_0p1a_cnt_target0_reach_1_int_st:    1;  /*reg_0p1a_0 counter after xpd reach target0*/
            uint32_t ana_0p1a_cnt_target1_reach_1_int_st:    1;  /*reg_0p1a_1_counter after xpd reach target1*/
            uint32_t ana_0p2a_cnt_target0_reach_0_int_st:    1;  /*reg_0p2a_0 counter after xpd reach target0*/
            uint32_t ana_0p2a_cnt_target1_reach_0_int_st:    1;  /*reg_0p2a_1_counter after xpd reach target1*/
            uint32_t ana_0p2a_cnt_target0_reach_1_int_st:    1;  /*reg_0p2a_0 counter after xpd reach target0*/
            uint32_t ana_0p2a_cnt_target1_reach_1_int_st:    1;  /*reg_0p2a_1_counter after xpd reach target1*/
            uint32_t ana_0p3a_cnt_target0_reach_0_int_st:    1;  /*reg_0p3a_0 counter after xpd reach target0*/
            uint32_t ana_0p3a_cnt_target1_reach_0_int_st:    1;  /*reg_0p3a_1_counter after xpd reach target1*/
            uint32_t ana_0p3a_cnt_target0_reach_1_int_st:    1;  /*reg_0p3a_0_counter after xpd reach target0*/
            uint32_t ana_0p3a_cnt_target1_reach_1_int_st:    1;  /*reg_0p3a_1_counter after xpd reach target1*/
            uint32_t reserved18                    :    14;
        };
        uint32_t val;
    } pmu_int_st;
    union {
        struct {
            uint32_t lp_slp_wakeup_int_clr         :    1;  /*Clear sleep wakeup interrupt state*/
            uint32_t lp_slp_reject_int_clr         :    1;  /*Clear sleep reject interrupt state*/
            uint32_t hp_slp_wakeup_int_clr         :    1;  /*Clear sleep wakeup interrupt state*/
            uint32_t hp_slp_reject_int_clr         :    1;  /*Clear sleep reject interrupt state*/
            uint32_t brown_out_int_clr             :    1;  /*CLear brown out interrupt state*/
            uint32_t glitch_det_int_clr            :    1;  /*CLear glitch det interrupt state*/
            uint32_t ana_0p1a_cnt_target0_reach_0_int_clr:    1;  /*reg_0p1a_0_counter after xpd reach target0*/
            uint32_t ana_0p1a_cnt_target1_reach_0_int_clr:    1;  /*reg_0p1a_1_counter after xpd reach target1*/
            uint32_t ana_0p1a_cnt_target0_reach_1_int_clr:    1;  /*reg_0p1a_0 counter after xpd reach target0*/
            uint32_t ana_0p1a_cnt_target1_reach_1_int_clr:    1;  /*reg_0p1a_1_counter after xpd reach target1*/
            uint32_t ana_0p2a_cnt_target0_reach_0_int_clr:    1;  /*reg_0p2a_0 counter after xpd reach target0*/
            uint32_t ana_0p2a_cnt_target1_reach_0_int_clr:    1;  /*reg_0p2a_1_counter after xpd reach target1*/
            uint32_t ana_0p2a_cnt_target0_reach_1_int_clr:    1;  /*reg_0p2a_0 counter after xpd reach target0*/
            uint32_t ana_0p2a_cnt_target1_reach_1_int_clr:    1;  /*reg_0p2a_1_counter after xpd reach target1*/
            uint32_t ana_0p3a_cnt_target0_reach_0_int_clr:    1;  /*reg_0p3a_0 counter after xpd reach target0*/
            uint32_t ana_0p3a_cnt_target1_reach_0_int_clr:    1;  /*reg_0p3a_1_counter after xpd reach target1*/
            uint32_t ana_0p3a_cnt_target0_reach_1_int_clr:    1;  /*reg_0p3a_0_counter after xpd reach target0*/
            uint32_t ana_0p3a_cnt_target1_reach_1_int_clr:    1;  /*reg_0p3a_1_counter after xpd reach target1*/
            uint32_t reserved18                    :    14;
        };
        uint32_t val;
    } pmu_int_clr;
    uint32_t reserved_110;
    uint32_t reserved_114;
    uint32_t reserved_118;
    uint32_t reserved_11c;
    union {
        struct {
            uint32_t probe_a_mod_sel               :    16;
            uint32_t probe_a_top_sel               :    8;
            uint32_t probe_l_sel                   :    2;
            uint32_t probe_h_sel                   :    2;
            uint32_t probe_global_en               :    1;
            uint32_t reserved29                    :    3;
        };
        uint32_t val;
    } lp_probea_ctrl;
    union {
        struct {
            uint32_t probe_b_mod_sel               :    16;
            uint32_t probe_b_top_sel               :    8;
            uint32_t probe_b_en                    :    1;
            uint32_t reserved25                    :    7;
        };
        uint32_t val;
    } lp_probeb_ctrl;
    uint32_t lp_probe_out;
    union {
        struct {
            uint32_t dbg_req                       :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } lp_core_dbg_req;
    uint32_t lp_core_dbg_dm_halt_addr;
    uint32_t lp_core_dbg_dm_exception_addr;
    uint32_t lp_core_dbg_x2;
    uint32_t lp_core_dbg_x10;
    uint32_t lp_core_dbg_x11;
    uint32_t lp_core_dbg_pc;
    uint32_t reserved_148;
    union {
        struct {
            uint32_t ana_hp_vss_drv_b_0            :    6;
            uint32_t ana_hp_vss_drv_b_1            :    6;
            uint32_t ana_hp_vss_drv_b_2            :    6;
            uint32_t ana_hp_vss_drv_b_3            :    6;
            uint32_t ana_hp_vss_drv_b_4            :    6;
            uint32_t reserved30                    :    2;
        };
        uint32_t val;
    } vss_ctrl0;
    union {
        struct {
            uint32_t ana_hp_vss_drv_b_5            :    6;
            uint32_t ana_lp_vss_drv_b              :    6;
            uint32_t reserved12                    :    20;
        };
        uint32_t val;
    } vss_ctrl1;
    union {
        struct {
            uint32_t ana_0p1a_xpd_0                :    1;
            uint32_t ana_0p1a_xpd_1                :    1;
            uint32_t ana_0p1a_mul_0                :    3;
            uint32_t ana_0p1a_mul_1                :    3;
            uint32_t ana_0p1a_en_vdet_0            :    1;
            uint32_t ana_0p1a_en_vdet_1            :    1;
            uint32_t ana_0p1a_en_cur_lim_0         :    1;
            uint32_t ana_0p1a_en_cur_lim_1         :    1;
            uint32_t ana_0p1a_tieh_sel_0           :    3;
            uint32_t ana_0p1a_tieh_sel_1           :    3;
            uint32_t ana_0p1a_tieh_0               :    1;
            uint32_t ana_0p1a_tieh_1               :    1;
            uint32_t force_ana_0p1a_tieh_sel_0     :    1;
            uint32_t force_ana_0p1a_tieh_sel_1     :    1;
            uint32_t ana_0p1a_dref_0               :    4;
            uint32_t ana_0p1a_dref_1               :    4;
            uint32_t reserved30                    :    2;
        };
        uint32_t val;
    } io_ldo_ctrl0;
    union {
        struct {
            uint32_t ana_0p2a_xpd_0                :    1;
            uint32_t ana_0p2a_xpd_1                :    1;
            uint32_t ana_0p2a_mul_0                :    3;
            uint32_t ana_0p2a_mul_1                :    3;
            uint32_t ana_0p2a_en_vdet_0            :    1;
            uint32_t ana_0p2a_en_vdet_1            :    1;
            uint32_t ana_0p2a_en_cur_lim_0         :    1;
            uint32_t ana_0p2a_en_cur_lim_1         :    1;
            uint32_t ana_0p2a_tieh_sel_0           :    3;
            uint32_t ana_0p2a_tieh_sel_1           :    3;
            uint32_t ana_0p2a_tieh_0               :    1;
            uint32_t ana_0p2a_tieh_1               :    1;
            uint32_t force_ana_0p2a_tieh_sel_0     :    1;
            uint32_t force_ana_0p2a_tieh_sel_1     :    1;
            uint32_t ana_0p2a_dref_0               :    4;
            uint32_t ana_0p2a_dref_1               :    4;
            uint32_t reserved30                    :    2;
        };
        uint32_t val;
    } io_ldo_ctrl1;
    union {
        struct {
            uint32_t ana_0p3a_xpd_0                :    1;
            uint32_t ana_0p3a_xpd_1                :    1;
            uint32_t ana_0p3a_mul_0                :    3;
            uint32_t ana_0p3a_mul_1                :    3;
            uint32_t ana_0p3a_en_vdet_0            :    1;
            uint32_t ana_0p3a_en_vdet_1            :    1;
            uint32_t ana_0p3a_en_cur_lim_0         :    1;
            uint32_t ana_0p3a_en_cur_lim_1         :    1;
            uint32_t ana_0p3a_tieh_sel_0           :    3;
            uint32_t ana_0p3a_tieh_sel_1           :    3;
            uint32_t ana_0p3a_tieh_0               :    1;
            uint32_t ana_0p3a_tieh_1               :    1;
            uint32_t force_ana_0p3a_tieh_sel_0     :    1;
            uint32_t force_ana_0p3a_tieh_sel_1     :    1;
            uint32_t ana_0p3a_dref_0               :    4;
            uint32_t ana_0p3a_dref_1               :    4;
            uint32_t reserved30                    :    2;
        };
        uint32_t val;
    } io_ldo_ctrl2;
    uint32_t reserved_160;
    union {
        struct {
            uint32_t lp_moni_peri0_en              :    1;  /*LP peri0 monitor wakeup enable*/
            uint32_t lp_moni_peri1_en              :    1;  /*LP peri1 monitor wakeup enable*/
            uint32_t lp_moni_clk_switch_en         :    1;  /*LP peri monitor fast clock switch enable*/
            uint32_t reserved3                     :    5;
            uint32_t lp_moni_power_stable_wait_num :    10;  /*wait cycle number for LP peri power stable*/
            uint32_t reserved18                    :    14;
        };
        uint32_t val;
    } lp_moni_ctrl;
    uint32_t lp_moni_slp_wait_num;
    uint32_t lp_moni_work_wait_num;
    union {
        struct {
            uint32_t lpcore_reset_cause            :    6;  /*6'h1: POR reset
6'h2: external reset
6'h6: LP core reset from LP watchdog
6'h9: PMU LP CPU power down reset
6'hf: brown out reset
6'h10: LP watchdog chip reset
6'h12: super watch dog reset
6'h13: glitch reset
6'h14: software reset from HP*/
            uint32_t lpcore_reset_flag             :    1;
            uint32_t lpcore_reset_flag_clr         :    1;
            uint32_t reserved8                     :    24;
        };
        uint32_t val;
    } lp_core_rst_status;
    union {
        struct {
            uint32_t f2s_apb_postw_en              :    1;  /*fast to slow clock apb2apb sync bridge post write enable, post write can speed up write, but will take time to update value to register */
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } f2s_apb_brg_cntl;
    uint32_t reserved_178;
    uint32_t reserved_17c;
    union {
        struct {
            uint32_t sw_hw_usb_phy_sel             :    1;
            uint32_t sw_usb_phy_sel                :    1;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } usb_ctrl;
    union {
        struct {
            uint32_t systimer_target0_intr_en      :    1;  /*systimer target0 intr enable*/
            uint32_t systimer_target1_intr_en      :    1;  /*systimer target1 intr enable*/
            uint32_t systimer_target2_intr_en      :    1;  /*systimer target2 intr enable*/
            uint32_t hp_cpu_intr_sync_en           :    1;  /*hp cpu intr sync enabe*/
            uint32_t adc_intr_en                   :    1;  /* adc intr enable*/
            uint32_t i2c_intr_en                   :    1;  /* i2c intr enable*/
            uint32_t i2s_intr_en                   :    1;  /*i2s intr enable*/
            uint32_t spi_intr_en                   :    1;  /*spi  int enable*/
            uint32_t tsens_intr_en                 :    1;  /*tsens intr enable*/
            uint32_t lp_uart_intr_en               :    1;  /*lp_uart intr enable*/
            uint32_t lp_gpio_intr_en               :    1;  /* lp gpio intr enable*/
            uint32_t mb_lp_intr_en                 :    1;  /*mb_lp intr enable*/
            uint32_t efuse_intr_en                 :    1;  /* efuse intr enable*/
            uint32_t wdt_intr_en                   :    1;  /* wdt intr enable*/
            uint32_t swd_intr_en                   :    1;  /*swd intr enable*/
            uint32_t rtc_tmr_intr_en               :    1;  /* rtc tmr intr enabe*/
            uint32_t lp_i3c_slv_intr_en            :    1;  /* lp_i3c slv intr enable*/
            uint32_t lp_i3c_mst_intr_en            :    1;  /*lp_i3c mst intr enable*/
            uint32_t pmu_intr_en                   :    1;  /*pmu intr enable*/
            uint32_t xtal32k_intr_en               :    1;  /*xtal32k intr enable*/
            uint32_t reserved20                    :    12;
        };
        uint32_t val;
    } lp_int_en;
    union {
        struct {
            uint32_t systimer_target0_intr_st      :    1;  /*systimer target0 intr status*/
            uint32_t systimer_target1_intr_st      :    1;  /*systimer target1 intr status*/
            uint32_t systimer_target2_intr_st      :    1;  /*systimer target2 intr status*/
            uint32_t hp_cpu_intr_sync_st           :    1;  /*hp cpu intr sync status*/
            uint32_t adc_intr_st                   :    1;  /* adc intr status*/
            uint32_t i2c_intr_st                   :    1;  /* i2c intr status*/
            uint32_t i2s_intr_st                   :    1;  /*i2s intr status*/
            uint32_t spi_intr_st                   :    1;  /*spi  int status*/
            uint32_t tsens_intr_st                 :    1;  /*tsens intr status*/
            uint32_t lp_uart_intr_st               :    1;  /*lp_uart intr status*/
            uint32_t lp_gpio_intr_st               :    1;  /* lp gpio intr status*/
            uint32_t mb_lp_intr_st                 :    1;  /*mb_lp intr status*/
            uint32_t efuse_intr_st                 :    1;  /* efuse intr status*/
            uint32_t wdt_intr_st                   :    1;  /* wdt intr status*/
            uint32_t swd_intr_st                   :    1;  /*swd intr status*/
            uint32_t rtc_tmr_intr_st               :    1;  /* rtc tmr intr status*/
            uint32_t lp_i3c_slv_intr_st            :    1;  /* lp_i3c slv intr status*/
            uint32_t lp_i3c_mst_intr_st            :    1;  /*lp_i3c mst intr status*/
            uint32_t pmu_intr_st                   :    1;  /*pmu intr status*/
            uint32_t xtal32k_intr_st               :    1;  /*xtal32k intr status*/
            uint32_t reserved20                    :    12;
        };
        uint32_t val;
    } lp_int_st;
    union {
        struct {
            uint32_t lp_tcm_ram_mem_aux_ctrl       :    14;  /*{ {TEST1} , {RME} , {RM[3:0]} , {RA[1:0]} , {WA[2:0]} , {WPULSE[2:0]} }*/
            uint32_t reserved14                    :    18;
        };
        uint32_t val;
    } lp_tcm_ram_mem_aux_ctrl;
    union {
        struct {
            uint32_t ana_0p1a_target0_0            :    8;  /*set a target value to triger interrupt*/
            uint32_t ana_0p1a_target1_0            :    8;  /*set a target value to triger interrupt*/
            uint32_t ana_0p1a_ldo_cnt_prescaler_sel_0:    1;  /*0: 20MHz prescaler with 16 equals 0.8us 1:20MHz prescaler with 32 equals 1.6us*/
            uint32_t ana_0p1a_xpd_0                :    1;  /* set 1 after set ana_reg_0p1a_xpd_0*/
            uint32_t ana_0p1a_tieh_0_pos_en        :    1;  /* set 1 to enable tieh negedge triger ldo counter*/
            uint32_t ana_0p1a_tieh_0_neg_en        :    1;  /* set 1 to enable tieh posedge triger ldo counter*/
            uint32_t reserved20                    :    12;
        };
        uint32_t val;
    } reg_0p1a_0_cnt_ctrl;
    union {
        struct {
            uint32_t ana_0p1a_target0_1            :    8;  /*set a target value to triger interrupt*/
            uint32_t ana_0p1a_target1_1            :    8;  /*set a target value to triger interrupt*/
            uint32_t ana_0p1a_ldo_cnt_prescaler_sel_1:    1;  /*0: 20MHz prescaler with 16 equals 0.8us 1:20MHz prescaler with 32 equals 1.6us*/
            uint32_t ana_0p1a_xpd_1                :    1;  /* set 1 after set ana_reg_0p1a_xpd_1*/
            uint32_t ana_0p1a_tieh_1_pos_en        :    1;  /* set 1 to enable tieh negedge triger ldo counter*/
            uint32_t ana_0p1a_tieh_1_neg_en        :    1;  /* set 1 to enable tieh posedge triger ldo counter*/
            uint32_t reserved20                    :    12;
        };
        uint32_t val;
    } reg_0p1a_1_cnt_ctrl;
    union {
        struct {
            uint32_t ana_0p2a_target0_0            :    8;  /*set a target value to triger interrupt*/
            uint32_t ana_0p2a_target1_0            :    8;  /*set a target value to triger interrupt*/
            uint32_t ana_0p2a_ldo_cnt_prescaler_sel_0:    1;  /*0: 20MHz prescaler with 16 equals 0.8us 1:20MHz prescaler with 32 equals 1.6us*/
            uint32_t ana_0p2a_xpd_0                :    1;  /* set 1 after set ana_reg_0p2a_xpd_0*/
            uint32_t ana_0p2a_tieh_0_pos_en        :    1;  /* set 1 to enable tieh negedge triger ldo counter*/
            uint32_t ana_0p2a_tieh_0_neg_en        :    1;  /* set 1 to enable tieh posedge triger ldo counter*/
            uint32_t reserved20                    :    12;
        };
        uint32_t val;
    } reg_0p2a_0_cnt_ctrl;
    union {
        struct {
            uint32_t ana_0p2a_target0_1            :    8;  /*set a target value to triger interrupt*/
            uint32_t ana_0p2a_target1_1            :    8;  /*set a target value to triger interrupt*/
            uint32_t ana_0p2a_ldo_cnt_prescaler_sel_1:    1;  /*0: 20MHz prescaler with 16 equals 0.8us 1:20MHz prescaler with 32 equals 1.6us*/
            uint32_t ana_0p2a_xpd_1                :    1;  /* set 1 after set ana_reg_0p2a_xpd_1*/
            uint32_t ana_0p2a_tieh_1_pos_en        :    1;  /* set 1 to enable tieh negedge triger ldo counter*/
            uint32_t ana_0p2a_tieh_1_neg_en        :    1;  /* set 1 to enable tieh posedge triger ldo counter*/
            uint32_t reserved20                    :    12;
        };
        uint32_t val;
    } reg_0p2a_1_cnt_ctrl;
    union {
        struct {
            uint32_t ana_0p3a_target0_0            :    8;  /*set a target value to triger interrupt*/
            uint32_t ana_0p3a_target1_0            :    8;  /*set a target value to triger interrupt*/
            uint32_t ana_0p3a_ldo_cnt_prescaler_sel_0:    1;  /*0: 20MHz prescaler with 16 equals 0.8us 1:20MHz prescaler with 32 equals 1.6us*/
            uint32_t ana_0p3a_xpd_0                :    1;  /* set 1 after set ana_reg_0p3a_xpd_0*/
            uint32_t ana_0p3a_tieh_0_pos_en        :    1;  /* set 1 to enable tieh negedge triger ldo counter*/
            uint32_t ana_0p3a_tieh_0_neg_en        :    1;  /* set 1 to enable tieh posedge triger ldo counter*/
            uint32_t reserved20                    :    12;
        };
        uint32_t val;
    } reg_0p3a_0_cnt_ctrl;
    union {
        struct {
            uint32_t ana_0p3a_target0_1            :    8;  /*set a target value to triger interrupt*/
            uint32_t ana_0p3a_target1_1            :    8;  /*set a target value to triger interrupt*/
            uint32_t ana_0p3a_ldo_cnt_prescaler_sel_1:    1;  /*0: 20MHz prescaler with 16 equals 0.8us 1:20MHz prescaler with 32 equals 1.6us*/
            uint32_t ana_0p3a_xpd_1                :    1;  /* set 1 after set ana_reg_0p3a_xpd_1*/
            uint32_t ana_0p3a_tieh_1_pos_en        :    1;  /* set 1 to enable tieh negedge triger ldo counter*/
            uint32_t ana_0p3a_tieh_1_neg_en        :    1;  /* set 1 to enable tieh posedge triger ldo counter*/
            uint32_t reserved20                    :    12;
        };
        uint32_t val;
    } reg_0p3a_1_cnt_ctrl;
    union {
        struct {
            uint32_t sdio_slave_ldo_ready          :    1;  /*Set 1 to tell sdio slave that ldo is ready*/
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } sdio_slave_ldo_ready;
    union {
        struct {
            uint32_t ana_xpd_pad_group             :    8;  /*Set 1 to power up pad group*/
            uint32_t ana_sdio_pll_xpd              :    1;  /*Set 1 to power up sdio pll*/
            uint32_t ana_mspi_phy_xpd              :    1;  /*Set 1 to power up mspi dqs phy*/
            uint32_t reserved10                    :    22;
        };
        uint32_t val;
    } ana_xpd_pad_group;
    union {
        struct {
            uint32_t lp_tcm_ram_rdn_eco_en         :    1;
            uint32_t lp_tcm_ram_rdn_eco_result     :    1;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } lp_tcm_ram_rdn_eco_cs;
    uint32_t lp_tcm_ram_rdn_eco_low;
    uint32_t lp_tcm_ram_rdn_eco_high;
    union {
        struct {
            uint32_t lp_tcm_rom_rdn_eco_en         :    1;
            uint32_t lp_tcm_rom_rdn_eco_result     :    1;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } lp_tcm_rom_rdn_eco_cs;
    uint32_t lp_tcm_rom_rdn_eco_low;
    uint32_t lp_tcm_rom_rdn_eco_high;
    uint32_t reserved_1c8;
    uint32_t reserved_1cc;
    union {
        struct {
            uint32_t dcdc_on_req                   :    1;  /*SW trigger dcdc on*/
            uint32_t dcdc_off_req                  :    1;  /*SW trigger dcdc off*/
            uint32_t dcdc_lightslp_req             :    1;  /*SW trigger dcdc enter lightsleep*/
            uint32_t dcdc_deepslp_req              :    1;  /*SW trigger dcdc enter deepsleep*/
            uint32_t reserved4                     :    4;
            uint32_t dcdc_on_force_pu              :    1;
            uint32_t dcdc_on_force_pd              :    1;
            uint32_t dcdc_fb_res_force_pu          :    1;
            uint32_t dcdc_fb_res_force_pd          :    1;
            uint32_t dcdc_ls_force_pu              :    1;
            uint32_t dcdc_ls_force_pd              :    1;
            uint32_t dcdc_ds_force_pu              :    1;
            uint32_t dcdc_ds_force_pd              :    1;
            uint32_t dcm_cur_st                    :    8;
            uint32_t ana_dcdc_vset                 :    5;
            uint32_t ana_dcdc_en_amux_test         :    1;  /*Enable analog mux to pull PAD TEST_DCDC voltage signal*/
            uint32_t reserved30                    :    2;
        };
        uint32_t val;
    } lp_dcm_ctrl;
    union {
        struct {
            uint32_t dcdc_pre_delay                :    8;  /*DCDC pre-on/post off delay*/
            uint32_t dcdc_res_off_delay            :    8;  /*DCDC fb res off delay*/
            uint32_t dcdc_stable_delay             :    10;  /*DCDC stable delay*/
            uint32_t reserved26                    :    6;
        };
        uint32_t val;
    } lp_dcm_wait_delay;
    union {
        struct {
            uint32_t dcm_gpio_drv                  :    4;  /* DCDC GPIO 1/2 drv, bit0 for gpio1, bit1 for gpio2*/
            uint32_t dcm_gpio_hold                 :    2;  /*DCDC GPIO 1/2 hold*/
            uint32_t dcm_gpio_rde                  :    2;  /*DCDC GPIO 1/2 rde*/
            uint32_t dcm_gpio_rue                  :    2;  /*DCDC GPIO 1/2 rue*/
            uint32_t reserved10                    :    22;
        };
        uint32_t val;
    } lp_dcdc_pad_ctrl;
    union {
        struct {
            uint32_t sdio_in_force                 :    1;  /*force to contrl sdio_in sample edge using register*/
            uint32_t sdio_out_force                :    1;  /*0-negedge sample 1-posedge sample*/
            uint32_t sdio_in_ctrl                  :    1;  /*force to contrl sdio_out sample edge using register*/
            uint32_t sdio_out_ctrl                 :    1;  /*0-negedge sample 1-posedge sample*/
            uint32_t reserved4                     :    28;
        };
        uint32_t val;
    } sdio_slave_ctrl;
    union {
        struct {
            uint32_t pmu_rdn_eco_en                :    1;
            uint32_t pmu_rdn_eco_result            :    1;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } lp_pmu_rdn_eco_cs;
    uint32_t lp_pmu_rdn_eco_low;
    uint32_t lp_pmu_rdn_eco_high;
} lp_sys_dev_t;
extern lp_sys_dev_t LP_SYS;
#ifdef __cplusplus
}
#endif



#endif /*_SOC_LP_SYS_STRUCT_H_ */
