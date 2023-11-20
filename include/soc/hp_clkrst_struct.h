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
#ifndef _SOC_HP_CLKRST_STRUCT_H_
#define _SOC_HP_CLKRST_STRUCT_H_


#ifdef __cplusplus
extern "C" {
#endif

typedef volatile struct {
    uint32_t ver_date;
    union {
        struct {
            uint32_t hp_sys_root_clk_sel           :    2;  /*Hp system root clock source select
2'h0: 20M RC OSC
2'h1: 40M XTAL
2'h2: HP system PLL clock
2'h3: HP CPU PLL clock*/
            uint32_t hp_cpu_root_clk_sel           :    2;  /*Hp cpu root clock source select
2'h0: 20M RC OSC
2'h1: 40M XTAL
2'h2: HP CPU PLL clock
2'h3: HP system PLL clock*/
            uint32_t reserved4                     :    28;
        };
        uint32_t val;
    } hp_ctrl;
    union {
        struct {
            uint32_t cpu_clk_en                    :    1;  /*clock output enable*/
            uint32_t reserved1                     :    7;
            uint32_t cpu_clk_div_num               :    8;  /*clock divider number*/
            uint32_t reserved16                    :    8;
            uint32_t cpu_clk_cur_div_num           :    8;  /*current clock divider number*/
        };
        uint32_t val;
    } cpu_ctrl;
    union {
        struct {
            uint32_t sys_clk_en                    :    1;  /*clock output enable*/
            uint32_t sys_clk_sync_en               :    1;  /*clock sync enable : will adjust clock phase when receive clock sync signal*/
            uint32_t sys_clk_force_sync_en         :    1;  /*clock force sync enable : clock output only available when clock is synced*/
            uint32_t reserved3                     :    5;
            uint32_t sys_clk_div_num               :    8;  /*clock divider number*/
            uint32_t sys_clk_phase_offset          :    8;  /*phase offset compare to clock sync signal*/
            uint32_t sys_clk_cur_div_num           :    8;  /*current clock divider number*/
        };
        uint32_t val;
    } sys_ctrl;
    union {
        struct {
            uint32_t peri1_clk_en                  :    1;  /*clock output enable*/
            uint32_t peri1_clk_sync_en             :    1;  /*clock sync enable : will adjust clock phase when receive clock sync signal*/
            uint32_t peri1_clk_force_sync_en       :    1;  /*clock force sync enable : clock output only available when clock is synced*/
            uint32_t reserved3                     :    5;
            uint32_t peri1_clk_div_num             :    8;  /*clock divider number*/
            uint32_t peri1_clk_phase_offset        :    8;  /*phase offset compare to clock sync signal*/
            uint32_t peri1_clk_cur_div_num         :    8;  /*current clock divider number*/
        };
        uint32_t val;
    } peri1_ctrl;
    union {
        struct {
            uint32_t peri2_clk_en                  :    1;  /*clock output enable*/
            uint32_t peri2_clk_sync_en             :    1;  /*clock sync enable : will adjust clock phase when receive clock sync signal*/
            uint32_t peri2_clk_force_sync_en       :    1;  /*clock force sync enable : clock output only available when clock is synced*/
            uint32_t reserved3                     :    5;
            uint32_t peri2_clk_div_num             :    8;  /*clock divider number*/
            uint32_t peri2_clk_phase_offset        :    8;  /*phase offset compare to clock sync signal*/
            uint32_t peri2_clk_cur_div_num         :    8;  /*current clock divider number*/
        };
        uint32_t val;
    } peri2_ctrl;
    union {
        struct {
            uint32_t psram_phy_clk_en              :    1;
            uint32_t psram_phy_clk_sel             :    2;
            uint32_t reserved3                     :    5;
            uint32_t psram_phy_clk_div_num         :    8;
            uint32_t reserved16                    :    8;
            uint32_t psram_phy_clk_cur_div_num     :    8;  /*current clock divider number*/
        };
        uint32_t val;
    } psram_phy_ctrl;
    union {
        struct {
            uint32_t ddr_phy_clk_en                :    1;
            uint32_t ddr_phy_clk_sel               :    2;
            uint32_t reserved3                     :    5;
            uint32_t ddr_phy_clk_div_num           :    8;
            uint32_t reserved16                    :    8;
            uint32_t ddr_phy_clk_cur_div_num       :    8;  /*current clock divider number*/
        };
        uint32_t val;
    } ddr_phy_ctrl;
    union {
        struct {
            uint32_t mspi_clk_en                   :    1;  /*clock output enable*/
            uint32_t reserved1                     :    7;
            uint32_t mspi_clk_div_num              :    4;  /*clock divider number*/
            uint32_t reserved12                    :    4;
            uint32_t mspi_src_clk_sel              :    2;  /*2'b00:480MHz PLL
2'b01: MSPI DLL CLK
2'b1x: HP XTAL CLK*/
            uint32_t reserved18                    :    14;
        };
        uint32_t val;
    } mspi_ctrl;
    union {
        struct {
            uint32_t dual_mspi_clk_en              :    1;  /*clock output enable*/
            uint32_t reserved1                     :    7;
            uint32_t dual_mspi_clk_div_num         :    4;  /*clock divider number*/
            uint32_t reserved12                    :    4;
            uint32_t dual_mspi_src_clk_sel         :    2;  /*2'b00:480MHz PLL
2'b01: MSPI DLL CLK
2'b1x: HP XTAL CLK*/
            uint32_t reserved18                    :    14;
        };
        uint32_t val;
    } dual_mspi_ctrl;
    union {
        struct {
            uint32_t ref_clk_en                    :    1;  /*reference clock output enable*/
            uint32_t ref_clk_div_num               :    4;  /*reference clock divider number*/
            uint32_t reserved5                     :    3;
            uint32_t usb2_ref_clk_div_num          :    8;  /*usb2 phy reference clock divider number*/
            uint32_t ledc_ref_clk_div_num          :    4;  /*ledc reference clock divider number*/
            uint32_t usbphy_clk_div_num            :    4;  /*usbphy clock divider number*/
            uint32_t ref_clk2_div_num              :    4;  /*120MHz reference clock divider number, used by i3c master*/
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } ref_ctrl;
    union {
        struct {
            uint32_t tm_20m_clk_en                 :    1;  /*20M test mode clock enabl*/
            uint32_t tm_40m_clk_en                 :    1;  /*40M test mode clock enable*/
            uint32_t tm_48m_clk_en                 :    1;  /*48M test mode clock enable*/
            uint32_t tm_80m_clk_en                 :    1;  /*80M test mode clock enable*/
            uint32_t tm_120m_clk_en                :    1;  /*120M test mode clock enable*/
            uint32_t tm_160m_clk_en                :    1;  /*160M test mode clock enable*/
            uint32_t tm_200m_clk_en                :    1;  /*200M test mode clock enable*/
            uint32_t tm_240m_clk_en                :    1;  /*240M test mode clock enable*/
            uint32_t reserved8                     :    24;
        };
        uint32_t val;
    } tm_ctrl;
    union {
        struct {
            uint32_t core3_clk_en                  :    1;  /*hp core3 clock enable*/
            uint32_t core2_clk_en                  :    1;  /*hp core2 clock enable*/
            uint32_t core1_clk_en                  :    1;  /*hp core1 clock enable*/
            uint32_t core0_clk_en                  :    1;  /*hp core0 clock enable*/
            uint32_t core3_force_norst             :    1;  /*software force no reset*/
            uint32_t core2_force_norst             :    1;  /*software force no reset*/
            uint32_t core1_force_norst             :    1;  /*software force no reset*/
            uint32_t core0_force_norst             :    1;  /*software force no reset*/
            uint32_t core1_global_rstn             :    1;  /*core1 software global reset*/
            uint32_t core0_global_rstn             :    1;  /*core0 software global reset*/
            uint32_t reserved10                    :    22;
        };
        uint32_t val;
    } core_ctrl;
    union {
        struct {
            uint32_t cache_clk_en                  :    1;  /*cache clock enable*/
            uint32_t cache_apb_clk_en              :    1;  /*cache apb clock enable*/
            uint32_t hp_cache_rstn                 :    1;  /*cache software reset: low active*/
            uint32_t reserved3                     :    5;
            uint32_t cache_clk_div_num             :    8;  /*L2 cache clock divider number*/
            uint32_t reserved16                    :    16;
        };
        uint32_t val;
    } cache_ctrl;
    union {
        struct {
            uint32_t cpu_ctrl_clk_en               :    1;  /*cpu control logic clock enable*/
            uint32_t tcm_clk_en                    :    1;  /*tcm clock enable*/
            uint32_t tcm_rstn                      :    1;  /*tcm software reset: low active*/
            uint32_t l2_mem_clk_en                 :    1;  /*l2 memory clock enable*/
            uint32_t l2_mem_rstn                   :    1;  /*l2 memory software reset: low active*/
            uint32_t reserved5                     :    27;
        };
        uint32_t val;
    } cpu_peri_ctrl;
    union {
        struct {
            uint32_t hp_root_clk_sync_perid        :    16;  /*clock sync signal generation period*/
            uint32_t hp_root_clk_sync_en           :    1;  /*clock sync signal output enable*/
            uint32_t clk_en                        :    1;
            uint32_t reserved18                    :    14;
        };
        uint32_t val;
    } sync_ctrl;
    union {
        struct {
            uint32_t cpu_wfi_delay_num             :    4;  /*This register indicates delayed clock cycles before auto gating HP cache/trace clock once WFI asserted*/
            uint32_t reserved4                     :    12;
            uint32_t cpu_wfi_force_clkg1_on        :    1;  /*force group1(L1/L2 cache & trace & cpu_icm_ibus) clock on after WFI */
            uint32_t cpu_wfi_force_clkg2_on        :    1;  /*force group2(HP TCM) clock on after WFI*/
            uint32_t cpu_wfi_force_clkg3_on        :    1;  /*force group3(L2 Memory) clock on after WFI */
            uint32_t reserved19                    :    13;
        };
        uint32_t val;
    } wfi_gate_clk_ctrl;
    union {
        struct {
            uint32_t pvt_clk_sel                   :    2;  /*pvt clock sel*/
            uint32_t reserved2                     :    2;
            uint32_t pvt_clk_div_num               :    4;  /*pvt clock div number*/
            uint32_t pvt_top_clk_en                :    1;  /*pvt top clock en */
            uint32_t pvt_cpu_group1_clk_en         :    1;  /*pvt cpu group1 clk en*/
            uint32_t pvt_cpu_group2_clk_en         :    1;  /*pvt cpu group2 clk en */
            uint32_t pvt_peri_group1_clk_en        :    1;  /*pvt peri group1 clk en*/
            uint32_t pvt_peri_group2_clk_en        :    1;  /*pvt peri group2 clk en*/
            uint32_t pvt_apb_clk_en                :    1;  /*pvt apb clk en*/
            uint32_t reserved14                    :    2;
            uint32_t pvt_top_rstn                  :    1;  /*pvt top resetn */
            uint32_t pvt_cpu_group1_rstn           :    1;  /*pvt cpu group1 resetn */
            uint32_t pvt_cpu_group2_rstn           :    1;  /*pvt cpu group2 resetn */
            uint32_t pvt_peri_group1_rstn          :    1;  /*pvt peri group1 resetn */
            uint32_t pvt_peri_group2_rstn          :    1;  /*pvt peri group2 resetn */
            uint32_t pvt_apb_rstn                  :    1;  /*pvt apb resetn */
            uint32_t reserved22                    :    10;
        };
        uint32_t val;
    } pvt_ctrl;
    union {
        struct {
            uint32_t test_pll_sel                  :    3;  /*test pll source select
3'h0: RSVD
3'h1: system PLL
3'h2: CPU PLL
3'h3: MPSI DLL
3'h4: SDIO PLL CK0
3'h5: SDIO PLL CK1
3'h6: SDIO PLL CK2
3'h7: AUDIO APLL*/
            uint32_t reserved3                     :    13;
            uint32_t test_pll_div_num              :    12;  /*test pll divider number*/
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } test_pll_ctrl;
} hp_clkrst_dev_t;
extern hp_clkrst_dev_t HP_CLKRST;
#ifdef __cplusplus
}
#endif



#endif /*_SOC_HP_CLKRST_STRUCT_H_ */
