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
#ifndef _SOC_HP_SYS_STRUCT_H_
#define _SOC_HP_SYS_STRUCT_H_


#ifdef __cplusplus
extern "C" {
#endif

typedef volatile struct {
    uint32_t ver_date;
    union {
        struct {
            uint32_t clk_en                        :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } clk_en;
    uint32_t reserved_8;
    uint32_t reserved_c;
    union {
        struct {
            uint32_t cpu_int_from_cpu_0            :    1;  /*set 1 will triger a interrupt*/
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } cpu_int_from_cpu_0;
    union {
        struct {
            uint32_t cpu_int_from_cpu_1            :    1;  /*set 1 will triger a interrupt*/
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } cpu_int_from_cpu_1;
    union {
        struct {
            uint32_t cpu_int_from_cpu_2            :    1;  /*set 1 will triger a interrupt*/
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } cpu_int_from_cpu_2;
    union {
        struct {
            uint32_t cpu_int_from_cpu_3            :    1;  /*set 1 will triger a interrupt*/
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } cpu_int_from_cpu_3;
    union {
        struct {
            uint32_t l2_cache_clk_on               :    1;  /*    l2 cahce clk enable*/
            uint32_t l1_d_cache_clk_on             :    1;  /*  l1 dcahce clk enable*/
            uint32_t l1_i3_cache_clk_on            :    1;  /* l1 icahce3 clk enable*/
            uint32_t l1_i2_cache_clk_on            :    1;  /* l1 icahce2 clk enable*/
            uint32_t l1_i1_cache_clk_on            :    1;  /* l1 icahce1 clk enable*/
            uint32_t l1_i0_cache_clk_on            :    1;  /* l1 icahce0 clk enable*/
            uint32_t reserved6                     :    26;
        };
        uint32_t val;
    } cache_clk_config;
    union {
        struct {
            uint32_t l2_cache_reset                :    1;  /*   set 1 to reset l2 cahce */
            uint32_t l1_d_cache_reset              :    1;  /* set 1 to reset l1 dcahce */
            uint32_t l1_i3_cache_reset             :    1;  /*set 1 to reset l1 icahce3*/
            uint32_t l1_i2_cache_reset             :    1;  /*set 1 to reset l1 icahce2*/
            uint32_t l1_i1_cache_reset             :    1;  /*set 1 to reset l1 icahce1*/
            uint32_t l1_i0_cache_reset             :    1;  /*set 1 to reset l1 icahce0*/
            uint32_t reserved6                     :    26;
        };
        uint32_t val;
    } cache_reset_config;
    union {
        struct {
            uint32_t pll_freq_sel                  :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } cache_pll_sel_config;
    union {
        struct {
            uint32_t sys_dma_addr_sel              :    1;  /*0 means dma access extmem use 8xxx_xxxx else use 4xxx_xxxx*/
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } sys_dma_addr_ctrl;
    uint32_t reserved_30;
    union {
        struct {
            uint32_t tcm_ram_ibus0_wt              :    3;  /*weight value of ibus0*/
            uint32_t tcm_ram_ibus1_wt              :    3;  /*weight value of ibus1*/
            uint32_t tcm_ram_ibus2_wt              :    3;  /*weight value of ibus2*/
            uint32_t tcm_ram_ibus3_wt              :    3;  /*weight value of ibus3*/
            uint32_t tcm_ram_dbus0_wt              :    3;  /*weight value of dbus0*/
            uint32_t tcm_ram_dbus1_wt              :    3;  /*weight value of dbus1*/
            uint32_t tcm_ram_dbus2_wt              :    3;  /*weight value of dbus2*/
            uint32_t tcm_ram_dbus3_wt              :    3;  /*weight value of dbus3*/
            uint32_t tcm_ram_dma_wt                :    3;  /*weight value of dma*/
            uint32_t reserved27                    :    4;  /*higher weight value means higher priority*/
            uint32_t tcm_ram_wrr_high              :    1;  /*enable weighted round robin arbitration*/
        };
        uint32_t val;
    } tcm_ram_wrr_config;
    union {
        struct {
            uint32_t tcm_ram_ds                    :    16;  /*hp tcm ram deepsleep*/
            uint32_t tcm_ram_ls                    :    16;  /*hp tcm ram lightsleep*/
        };
        uint32_t val;
    } tcm_ram_pwr_ctrl0;
    union {
        struct {
            uint32_t tcm_ram_sd                    :    16;  /*hp tcm ram shutdown*/
            uint32_t hp_tcm_clk_force_on           :    1;  /*hp_tcm clk gatig force on*/
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } tcm_ram_pwr_ctrl1;
    union {
        struct {
            uint32_t l2_rom_idx0_sd                :    1;  /*hp l2 rom sub0 shutdown*/
            uint32_t l2_rom_idx1_sd                :    1;  /*hp l2 rom sub1 shutdown*/
            uint32_t l2_rom_idx2_sd                :    1;  /*hp l2 rom sub2 shutdown*/
            uint32_t l2_rom_idx3_sd                :    1;  /*hp l2 rom sub3 shutdown*/
            uint32_t l2_rom_idx0_ls                :    1;  /*hp l2 rom sub0 lightsleep*/
            uint32_t l2_rom_idx1_ls                :    1;  /*hp l2 rom sub1 lightsleep*/
            uint32_t l2_rom_idx2_ls                :    1;  /*hp l2 rom sub2 lightsleep*/
            uint32_t l2_rom_idx3_ls                :    1;  /*hp l2 rom sub3 lightsleep*/
            uint32_t l2_rom_clk_force_on           :    1;  /*l2_rom clk gating force on*/
            uint32_t reserved9                     :    23;
        };
        uint32_t val;
    } l2_rom_pwr_ctrl0;
    uint32_t reserved_44;
    uint32_t reserved_48;
    uint32_t reserved_4c;
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
    } hp_probea_ctrl;
    union {
        struct {
            uint32_t probe_b_mod_sel               :    16;
            uint32_t probe_b_top_sel               :    8;
            uint32_t probe_b_en                    :    1;
            uint32_t reserved25                    :    7;
        };
        uint32_t val;
    } hp_probeb_ctrl;
    union {
        struct {
            uint32_t pwm_probe_mux_sel             :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } hp_probe_ctrl;
    uint32_t hp_probe_out;
    union {
        struct {
            uint32_t l2_mem_idx0_ls                :    6;  /*l2mem sub0 ram lightsleep*/
            uint32_t l2_mem_idx0_ds                :    6;  /*l2mem sub0 ram deepsleep*/
            uint32_t l2_mem_idx0_sd                :    6;  /*l2mem sub0 ram shutdown*/
            uint32_t l2_mem_clk_force_on           :    1;  /*l2ram clk_gating force on*/
            uint32_t reserved19                    :    13;
        };
        uint32_t val;
    } l2_mem_ram_pwr_ctrl0;
    union {
        struct {
            uint32_t l2_mem_idx1_ls                :    6;  /*l2mem sub1 ram lightsleep*/
            uint32_t l2_mem_idx1_ds                :    6;  /*l2mem sub1 ram deepsleep*/
            uint32_t l2_mem_idx1_sd                :    6;  /*l2mem sub1 ram shutdown*/
            uint32_t reserved18                    :    14;
        };
        uint32_t val;
    } l2_mem_ram_pwr_ctrl1;
    union {
        struct {
            uint32_t l2_mem_idx2_ls                :    6;  /*l2mem sub2 ram lightsleep*/
            uint32_t l2_mem_idx2_ds                :    6;  /*l2mem sub2 ram deepsleep*/
            uint32_t l2_mem_idx2_sd                :    6;  /*l2mem sub2 ram shutdown*/
            uint32_t reserved18                    :    14;
        };
        uint32_t val;
    } l2_mem_ram_pwr_ctrl2;
    union {
        struct {
            uint32_t l2_mem_idx3_ls                :    6;  /*l2mem sub3 ram lightsleep*/
            uint32_t l2_mem_idx3_ds                :    6;  /*l2mem sub3 ram deepsleep*/
            uint32_t l2_mem_idx3_sd                :    6;  /*l2mem sub3 ram shutdown*/
            uint32_t reserved18                    :    14;
        };
        uint32_t val;
    } l2_mem_ram_pwr_ctrl3;
    union {
        struct {
            uint32_t enable_spi_manual_encrypt     :    1;
            uint32_t enable_download_db_encrypt    :    1;
            uint32_t enable_download_g0cb_decrypt  :    1;
            uint32_t enable_download_manual_encrypt:    1;
            uint32_t reserved4                     :    28;
        };
        uint32_t val;
    } crypto_ctrl;
    uint32_t gpio_o_hold_ctrl0;
    union {
        struct {
            uint32_t gpio_0_hold_high              :    8;  /*hold control for gpio63~56*/
            uint32_t reserved8                     :    24;
        };
        uint32_t val;
    } gpio_o_hold_ctrl1;
    union {
        struct {
            uint32_t hp_sys_rdn_eco_en             :    1;
            uint32_t hp_sys_rdn_eco_result         :    1;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } rdn_eco_cs;
    union {
        struct {
            uint32_t cache_apb_postw_en            :    1;  /*cache apb register interface post write enable, 1 will speed up write, but will take some time to update value to register*/
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } cache_apb_postw_en;
    union {
        struct {
            uint32_t l2_mem_sub_blksize            :    2;  /*l2mem sub block size 00=>32 01=>64 10=>128 11=>256*/
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } l2_mem_subsize;
    union {
        struct {
            uint32_t l2_mem_divide_addr0           :    15;  /*l2mem divide line address0,default 0xff1ffff*/
            uint32_t reserved15                    :    17;  /*l2mem divide address with attribute cacheable/non_cacheable in order to skip ecc check for axi/trace access, address block must be larger than 4 kb*/
        };
        uint32_t val;
    } l2_mem_divide_addr0;
    union {
        struct {
            uint32_t l2_mem_divide_addr1           :    15;  /*l2mem divide line address1,default 0xff3ffff*/
            uint32_t reserved15                    :    17;
        };
        uint32_t val;
    } l2_mem_divide_addr1;
    union {
        struct {
            uint32_t l2_mem_divide_addr2           :    15;  /*l2mem divide line address2,default 0xff5ffff*/
            uint32_t reserved15                    :    17;
        };
        uint32_t val;
    } l2_mem_divide_addr2;
    union {
        struct {
            uint32_t l2_mem_divide_addr3           :    15;  /*l2mem divide line address3,default 0xff7ffff*/
            uint32_t reserved15                    :    17;
        };
        uint32_t val;
    } l2_mem_divide_addr3;
    union {
        struct {
            uint32_t l2_mem_addr0_attribute        :    1;  /*l2 mem divided space0 attribute: 0=cacheable 1=non_cacheable*/
            uint32_t l2_mem_addr1_attribute        :    1;  /*l2 mem divided space1 attribute: 0=cacheable 1=non_cacheable*/
            uint32_t l2_mem_addr2_attribute        :    1;  /*l2 mem divided space2 attribute: 0=cacheable 1=non_cacheable*/
            uint32_t l2_mem_addr3_attribute        :    1;  /*l2 mem divided space3 attribute: 0=cacheable 1=non_cacheable*/
            uint32_t l2_mem_addr4_attribute        :    1;  /*l2 mem divided space4 attribute: 0=cacheable 1=non_cacheable*/
            uint32_t reserved5                     :    10;  /*reserved*/
            uint32_t l2_mem_addr0_noncacheable_bypass:    1;
            uint32_t l2_mem_addr1_noncacheable_bypass:    1;
            uint32_t l2_mem_addr2_noncacheable_bypass:    1;
            uint32_t l2_mem_addr3_noncacheable_bypass:    1;
            uint32_t l2_mem_addr4_noncacheable_bypass:    1;  /*set noncachebale bypass to allow noncacheable master(like axi) to access cacheable addr region*/
            uint32_t reserved20                    :    12;
        };
        uint32_t val;
    } l2_mem_addr_attribute;
    union {
        struct {
            uint32_t l2_mem_trace0_illegal_access_int_raw:    1;  /*intr triggered when trace0 access cacheable addr*/
            uint32_t l2_mem_trace1_illegal_access_int_raw:    1;  /*intr triggered when trace1 access cacheable addr*/
            uint32_t l2_mem_axi_illegal_access_int_raw:    1;  /*intr triggered when axi access cacheable addr*/
            uint32_t l2_mem_ecc_low_err_int_raw    :    1;  /*intr triggered when two bit error detected and corrected from ecc*/
            uint32_t l2_mem_ecc_high_err_int_raw   :    1;  /*intr triggered when one bit error detected and corrected from ecc*/
            uint32_t l2_mem_exceed_addr_int_raw    :    1;  /*intr triggered when access addr exceeds 0xff9ffff at bypass mode or exceeds 0xff80000 at l2cache 128kb mode or exceeds 0xff60000 at l2cache 256kb mode*/
            uint32_t reserved6                     :    26;
        };
        uint32_t val;
    } l2_mem_int_raw;
    union {
        struct {
            uint32_t l2_mem_trace0_illegal_access_int_st:    1;
            uint32_t l2_mem_trace1_illegal_access_int_st:    1;
            uint32_t l2_mem_axi_illegal_access_int_st:    1;
            uint32_t l2_mem_ecc_low_err_int_st     :    1;
            uint32_t l2_mem_ecc_high_err_int_st    :    1;
            uint32_t l2_mem_exceed_addr_int_st     :    1;
            uint32_t reserved6                     :    26;
        };
        uint32_t val;
    } l2_mem_int_st;
    union {
        struct {
            uint32_t l2_mem_trace0_illegal_access_int_ena:    1;
            uint32_t l2_mem_trace1_illegal_access_int_ena:    1;
            uint32_t l2_mem_axi_illegal_access_int_ena:    1;
            uint32_t l2_mem_ecc_low_err_int_ena    :    1;
            uint32_t l2_mem_ecc_high_err_int_ena   :    1;
            uint32_t l2_mem_exceed_addr_int_ena    :    1;
            uint32_t reserved6                     :    26;
        };
        uint32_t val;
    } l2_mem_int_ena;
    union {
        struct {
            uint32_t l2_mem_trace0_illegal_access_int_clr:    1;
            uint32_t l2_mem_trace1_illegal_access_int_clr:    1;
            uint32_t l2_mem_axi_illegal_access_int_clr:    1;
            uint32_t l2_mem_ecc_low_err_int_clr    :    1;
            uint32_t l2_mem_ecc_high_err_int_clr   :    1;
            uint32_t l2_mem_exceed_addr_int_clr    :    1;
            uint32_t reserved6                     :    26;
        };
        uint32_t val;
    } l2_mem_int_clr;
    union {
        struct {
            uint32_t l2_mem_exceed_addr_int_addr   :    21;
            uint32_t l2_mem_exceed_addr_int_we     :    1;
            uint32_t l2_mem_exceed_addr_int_master :    3;
            uint32_t reserved25                    :    7;
        };
        uint32_t val;
    } l2_mem_int_record0;
    union {
        struct {
            uint32_t l2_mem_ecc_high_err_int_addr  :    15;
            uint32_t l2_mem_ecc_high_err_bit       :    8;
            uint32_t reserved23                    :    9;
        };
        uint32_t val;
    } l2_mem_int_record1;
    union {
        struct {
            uint32_t l2_mem_ecc_low_err_int_addr   :    15;
            uint32_t l2_mem_ecc_low_err_bit        :    8;
            uint32_t reserved23                    :    9;
        };
        uint32_t val;
    } l2_mem_int_record2;
    union {
        struct {
            uint32_t l2_mem_axi_illegal_access_int_addr:    21;
            uint32_t l2_mem_axi_illegal_access_int_we:    1;
            uint32_t reserved22                    :    10;
        };
        uint32_t val;
    } l2_mem_int_record3;
    union {
        struct {
            uint32_t l2_mem_trace0_illegal_access_int_addr:    21;
            uint32_t l2_mem_trace0_illegal_access_int_we:    1;
            uint32_t reserved22                    :    10;
        };
        uint32_t val;
    } l2_mem_int_record4;
    union {
        struct {
            uint32_t l2_mem_trace1_illegal_access_int_addr:    21;
            uint32_t l2_mem_trace1_illegal_access_int_we:    1;
            uint32_t reserved22                    :    10;
        };
        uint32_t val;
    } l2_mem_int_record5;
    union {
        struct {
            uint32_t l2_cache_ecc_en               :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } l2_mem_l2_cache_ecc;
    union {
        struct {
            uint32_t l1_cache_bus0_id              :    4;
            uint32_t reserved4                     :    28;
        };
        uint32_t val;
    } l1cache_bus0_id;
    union {
        struct {
            uint32_t l1_cache_bus1_id              :    4;
            uint32_t reserved4                     :    28;
        };
        uint32_t val;
    } l1cache_bus1_id;
    union {
        struct {
            uint32_t tcm_ram_mem_aux_ctrl          :    14;  /*{ {TEST1} , {RME} , {RM[3:0]} , {RA[1:0]} , {WA[2:0]} , {WPULSE[2:0]} }*/
            uint32_t reserved14                    :    18;
        };
        uint32_t val;
    } tcm_ram_mem_aux_ctrl;
    union {
        struct {
            uint32_t l2_mem_aux_ctrl               :    14;  /*{ {TEST1} , {RME} , {RM[3:0]} , {RA[1:0]} , {WA[2:0]} , {WPULSE[2:0]} }*/
            uint32_t reserved14                    :    18;
        };
        uint32_t val;
    } l2_mem_aux_ctrl;
    union {
        struct {
            uint32_t l2_mem_rdn_eco_en             :    1;
            uint32_t l2_mem_rdn_eco_result         :    1;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } l2_mem_rdn_eco_cs;
    uint32_t l2_mem_rdn_eco_low;
    uint32_t l2_mem_rdn_eco_high;
    union {
        struct {
            uint32_t hp_tcm_rdn_eco_en             :    1;
            uint32_t hp_tcm_rdn_eco_result         :    1;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } hp_tcm_rdn_eco_cs;
    uint32_t hp_tcm_rdn_eco_low;
    uint32_t hp_tcm_rdn_eco_high;
    union {
        struct {
            uint32_t gpio_ded_hold                 :    26;  /*hold control for gpio63~56*/
            uint32_t reserved26                    :    6;
        };
        uint32_t val;
    } gpio_ded_hold_ctrl;
    union {
        struct {
            uint32_t l2_mem_sw_ecc_bwe_mask_ctrl   :    1;  /*Set 1 to mask bwe hamming code bit*/
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } l2_mem_sw_ecc_bwe_mask;
    union {
        struct {
            uint32_t usb20_mem_clk_force_on        :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } usb20otg_mem_ctrl;
} hp_sys_dev_t;
extern hp_sys_dev_t HP_SYS;
#ifdef __cplusplus
}
#endif



#endif /*_SOC_HP_SYS_STRUCT_H_ */
