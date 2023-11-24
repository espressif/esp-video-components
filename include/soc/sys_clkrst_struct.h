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
#ifndef _SOC_SYS_CLKRST_STRUCT_H_
#define _SOC_SYS_CLKRST_STRUCT_H_


#ifdef __cplusplus
extern "C" {
#endif

typedef volatile struct {
    uint32_t sys_clk_ver_date;
    union {
        struct {
            uint32_t sys_icm_apb_clk_en            :    1;  /*system icm clock enable*/
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } sys_icm_ctrl;
    union {
        struct {
            uint32_t jpeg_clk_en                   :    1;  /*clock output enable*/
            uint32_t jpeg_clk_sync_en              :    1;  /*clock sync enable : will adjust clock phase when receive clock sync signal*/
            uint32_t jpeg_clk_force_sync_en        :    1;  /*clock force sync enable : clock output only available when clock is synced*/
            uint32_t jpeg_apb_clk_en               :    1;  /*apb clock enable*/
            uint32_t jpeg_rstn                     :    1;  /*software reset : low active*/
            uint32_t jpeg_force_norst              :    1;  /*software force no reset*/
            uint32_t reserved6                     :    2;
            uint32_t jpeg_clk_div_num              :    8;  /*clock divider number*/
            uint32_t jpeg_clk_phase_offset         :    8;  /*clock phase offset compare to hp clock sync signal*/
            uint32_t jpeg_clk_cur_div_num          :    8;  /*current clock divider number*/
        };
        uint32_t val;
    } jpeg_ctrl;
    union {
        struct {
            uint32_t gfx_clk_en                    :    1;  /*clock output enable*/
            uint32_t gfx_clk_sync_en               :    1;  /*clock sync enable : will adjust clock phase when receive clock sync signal*/
            uint32_t gfx_clk_force_sync_en         :    1;  /*clock force sync enable : clock output only available when clock is synced*/
            uint32_t gfx_apb_clk_en                :    1;  /*apb clock enable*/
            uint32_t gfx_rstn                      :    1;  /*software reset : low active*/
            uint32_t gfx_force_norst               :    1;  /*software force no reset*/
            uint32_t reserved6                     :    2;
            uint32_t gfx_clk_div_num               :    8;  /*clock divider number*/
            uint32_t gfx_clk_phase_offset          :    8;  /*clock phase offset compare to hp clock sync signal*/
            uint32_t gfx_clk_cur_div_num           :    8;  /*current clock divider number*/
        };
        uint32_t val;
    } gfx_ctrl;
    union {
        struct {
            uint32_t psram_clk_en                  :    1;  /*clock output enable*/
            uint32_t psram_clk_sync_en             :    1;  /*clock sync enable : will adjust clock phase when receive clock sync signal*/
            uint32_t psram_clk_force_sync_en       :    1;  /*clock force sync enable : clock output only available when clock is synced*/
            uint32_t psram_apb_clk_en              :    1;  /*apb clock enable*/
            uint32_t psram_rstn                    :    1;  /*software reset : low active*/
            uint32_t psram_force_norst             :    1;  /*software force no reset*/
            uint32_t reserved6                     :    2;
            uint32_t psram_clk_div_num             :    8;  /*clock divider number*/
            uint32_t psram_clk_phase_offset        :    8;  /*clock phase offset compare to hp clock sync signal*/
            uint32_t psram_clk_cur_div_num         :    8;  /*current clock divider number*/
        };
        uint32_t val;
    } psram_ctrl;
    union {
        struct {
            uint32_t mspi_apb_clk_en               :    1;
            uint32_t mspi_rstn                     :    1;
            uint32_t mspi_force_norst              :    1;
            uint32_t reserved3                     :    29;
        };
        uint32_t val;
    } mspi_ctrl;
    union {
        struct {
            uint32_t dsi_clk_en                    :    1;  /*clock output enable*/
            uint32_t dsi_clk_sync_en               :    1;  /*clock sync enable : will adjust clock phase when receive clock sync signal*/
            uint32_t dsi_clk_force_sync_en         :    1;  /*clock force sync enable : clock output only available when clock is synced*/
            uint32_t dsi_apb_clk_en                :    1;  /*apb clock enable*/
            uint32_t dsi_rstn                      :    1;  /*software reset : low active*/
            uint32_t dsi_force_norst               :    1;  /*software force no reset*/
            uint32_t reserved6                     :    2;
            uint32_t dsi_clk_div_num               :    8;  /*clock divider number*/
            uint32_t dsi_clk_phase_offset          :    8;  /*clock phase offset compare to hp clock sync signal*/
            uint32_t dsi_clk_cur_div_num           :    8;  /*current clock divider number*/
        };
        uint32_t val;
    } dsi_ctrl;
    union {
        struct {
            uint32_t csi_clk_en                    :    1;  /*clock output enable*/
            uint32_t csi_clk_sync_en               :    1;  /*clock sync enable : will adjust clock phase when receive clock sync signal*/
            uint32_t csi_clk_force_sync_en         :    1;  /*clock force sync enable : clock output only available when clock is synced*/
            uint32_t csi_apb_clk_en                :    1;  /*apb clock enable*/
            uint32_t csi_rstn                      :    1;  /*software reset : low active*/
            uint32_t csi_force_norst               :    1;  /*software force no reset*/
            uint32_t csi_brg_rstn                  :    1;  /*software reset : low active*/
            uint32_t reserved7                     :    1;
            uint32_t csi_clk_div_num               :    8;  /*clock divider number*/
            uint32_t csi_clk_phase_offset          :    8;  /*clock phase offset compare to hp clock sync signal*/
            uint32_t csi_clk_cur_div_num           :    8;  /*current clock divider number*/
        };
        uint32_t val;
    } csi_ctrl;
    union {
        struct {
            uint32_t usb_clk_en                    :    1;  /*clock output enable*/
            uint32_t usb_clk_sync_en               :    1;  /*clock sync enable : will adjust clock phase when receive clock sync signal*/
            uint32_t usb_clk_force_sync_en         :    1;  /*clock force sync enable : clock output only available when clock is synced*/
            uint32_t usb_apb_clk_en                :    1;  /*apb clock enable*/
            uint32_t usb_rstn                      :    1;  /*Usb software reset : low active*/
            uint32_t usb_phy_rstn                  :    1;  /*Usb phy software reset : low active*/
            uint32_t usb_force_norst               :    1;  /*Usb software force no reset*/
            uint32_t usb_phy_force_norst           :    1;  /*Usb phy software force no reset*/
            uint32_t usb_clk_div_num               :    8;  /*clock divider number*/
            uint32_t usb_clk_phase_offset          :    8;  /*clock phase offset compare to hp clock sync signal*/
            uint32_t usb_clk_cur_div_num           :    8;  /*current clock divider number*/
        };
        uint32_t val;
    } usb_ctrl;
    union {
        struct {
            uint32_t gmac_clk_en                   :    1;  /*clock output enable*/
            uint32_t gmac_clk_sync_en              :    1;  /*clock sync enable : will adjust clock phase when receive clock sync signal*/
            uint32_t gmac_clk_force_sync_en        :    1;  /*clock force sync enable : clock output only available when clock is synced*/
            uint32_t gmac_apb_clk_en               :    1;  /*apb clock enable*/
            uint32_t gmac_rstn                     :    1;  /*software reset : low active*/
            uint32_t gmac_force_norst              :    1;  /*software force no reset*/
            uint32_t reserved6                     :    2;
            uint32_t gmac_clk_div_num              :    8;  /*clock divider number*/
            uint32_t gmac_clk_phase_offset         :    8;  /*clock phase offset compare to hp clock sync signal*/
            uint32_t gmac_clk_cur_div_num          :    8;  /*current clock divider number*/
        };
        uint32_t val;
    } gmac_ctrl;
    union {
        struct {
            uint32_t sdmmc_clk_en                  :    1;  /*clock output enable*/
            uint32_t sdmmc_clk_sync_en             :    1;  /*clock sync enable : will adjust clock phase when receive clock sync signal*/
            uint32_t sdmmc_clk_force_sync_en       :    1;  /*clock force sync enable : clock output only available when clock is synced*/
            uint32_t sdmmc_apb_clk_en              :    1;  /*apb clock enable*/
            uint32_t sdmmc_rstn                    :    1;  /*software reset : low active*/
            uint32_t sdmmc_force_norst             :    1;  /*software force no reset */
            uint32_t reserved6                     :    2;
            uint32_t sdmmc_clk_div_num             :    8;  /*clock divider number*/
            uint32_t sdmmc_clk_phase_offset        :    8;  /*clock phase offset compare to hp clock sync signal*/
            uint32_t sdmmc_clk_cur_div_num         :    8;  /*current clock divider number*/
        };
        uint32_t val;
    } sdmmc_ctrl;
    union {
        struct {
            uint32_t ddrc_clk_en                   :    1;
            uint32_t ddrc_rstn                     :    1;
            uint32_t ddrc_force_norst              :    1;
            uint32_t reserved3                     :    29;
        };
        uint32_t val;
    } ddrc_ctrl;
    union {
        struct {
            uint32_t gdma_clk_en                   :    1;  /*clock output enable*/
            uint32_t gdma_clk_sync_en              :    1;  /*clock sync enable : will adjust clock phase when receive clock sync signal*/
            uint32_t gdma_clk_force_sync_en        :    1;  /*clock force sync enable : clock output only available when clock is synced*/
            uint32_t gdma_apb_clk_en               :    1;  /*apb clock enable*/
            uint32_t gdma_rstn                     :    1;  /*software reset : low active*/
            uint32_t gdma_force_norst              :    1;  /*software force no reset */
            uint32_t reserved6                     :    2;
            uint32_t gdma_clk_div_num              :    8;  /*clock divider number*/
            uint32_t gdma_clk_phase_offset         :    8;  /*clock phase offset compare to hp clock sync signal*/
            uint32_t gdma_clk_cur_div_num          :    8;  /*current clock divider number*/
        };
        uint32_t val;
    } gdma_ctrl;
    union {
        struct {
            uint32_t usbotg_clk_en                 :    1;
            uint32_t usbotg_clk_sync_en            :    1;
            uint32_t usbotg_clk_force_sync_en      :    1;
            uint32_t usbotg_apb_clk_en             :    1;
            uint32_t usbotg_rstn                   :    1;
            uint32_t usbotg_force_norst            :    1;
            uint32_t reserved6                     :    2;
            uint32_t usbotg_clk_div_num            :    8;
            uint32_t usbotg_clk_phase_offset       :    8;
            uint32_t usbotg_clk_cur_div_num        :    8;
        };
        uint32_t val;
    } usbotg_ctrl;
    union {
        struct {
            uint32_t sdio_slave_clk_en             :    1;
            uint32_t sdio_slave_clk_sync_en        :    1;
            uint32_t sdio_slave_clk_force_sync_en  :    1;
            uint32_t reserved3                     :    1;
            uint32_t sdio_slave_rstn               :    1;
            uint32_t sdio_slave_force_norst        :    1;
            uint32_t reserved6                     :    2;
            uint32_t sdio_slave_clk_div_num        :    8;
            uint32_t sdio_slave_clk_phase_offset   :    8;
            uint32_t sdio_slave_clk_cur_div_num    :    8;
        };
        uint32_t val;
    } sdio_slave_ctrl;
    uint32_t reserved_3c;
    union {
        struct {
            uint32_t isp_clk_en                    :    1;
            uint32_t isp_rstn                      :    1;
            uint32_t isp_force_norst               :    1;
            uint32_t isp_clk_div_num               :    4;
            uint32_t reserved7                     :    25;
        };
        uint32_t val;
    } isp_ctrl;
    union {
        struct {
            uint32_t dma2d_clk_en                  :    1;
            uint32_t dma2d_rstn                    :    1;
            uint32_t dma2d_force_norst             :    1;
            uint32_t reserved3                     :    29;
        };
        uint32_t val;
    } dma2d_ctrl;
    union {
        struct {
            uint32_t ppa_clk_en                    :    1;
            uint32_t ppa_rstn                      :    1;
            uint32_t ppa_force_norst               :    1;
            uint32_t reserved3                     :    29;
        };
        uint32_t val;
    } ppa_ctrl;
    uint32_t reserved_4c;
    union {
        struct {
            uint32_t debug_ch_num                  :    2;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } gdma_dbg_ctrl;
    uint32_t gmac_ptp_rd0;
    uint32_t gmac_ptp_rd1;
    union {
        struct {
            uint32_t ptp_pps                       :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } gmac_ptp_pps;
    union {
        struct {
            uint32_t reverse_gmac_rx               :    1;
            uint32_t reverse_gmac_tx               :    1;
            uint32_t ptp_ref_clk_sel               :    1;
            uint32_t phy_intf_sel                  :    3;
            uint32_t sbd_flowctrl                  :    1;
            uint32_t rmii_clk_port_sel             :    1;
            uint32_t revmii_pma_clk_en             :    1;
            uint32_t gmac_mem_clk_force_on         :    1;
            uint32_t gmac_func_tx_clk_en           :    1;
            uint32_t gmac_func_rx_clk_en           :    1;
            uint32_t gmac_func_tx_clk_force_on     :    1;
            uint32_t gmac_func_rx_clk_force_on     :    1;
            uint32_t reserved14                    :    18;
        };
        uint32_t val;
    } gmac_clk_ctrl;
    union {
        struct {
            uint32_t otg_phy_refclk_mode           :    1;
            uint32_t otg_suspendm                  :    1;
            uint32_t otg_phy_txbitstuff_en         :    1;
            uint32_t phy_suspendm                  :    1;
            uint32_t phy_suspend_force_en          :    1;
            uint32_t phy_pll_en                    :    1;
            uint32_t phy_pll_force_en              :    1;
            uint32_t phy_rstn                      :    1;
            uint32_t phy_reset_force_en            :    1;
            uint32_t phy_ref_clk_sel               :    1;
            uint32_t otg_ext_phy_sel               :    1;
            uint32_t reserved11                    :    21;
        };
        uint32_t val;
    } otg_phy_clk_ctrl;
    union {
        struct {
            uint32_t gmac_apb_postw_en             :    1;
            uint32_t jpeg_apb_postw_en             :    1;
            uint32_t dma2d_apb_postw_en            :    1;
            uint32_t usb11_apb_postw_en            :    1;
            uint32_t sdio_slave_apb_postw_en       :    1;
            uint32_t csi_host_apb_postw_en         :    1;
            uint32_t dsi_host_apb_postw_en         :    1;
            uint32_t reserved7                     :    25;
        };
        uint32_t val;
    } sys_peri_apb_postw_cntl;
    union {
        struct {
            uint32_t dma2d_lslp_mem_pd             :    1;
            uint32_t ppa_lslp_mem_pd               :    1;
            uint32_t jpeg_lslp_mem_pd              :    1;
            uint32_t jpeg_dslp_mem_pd              :    1;
            uint32_t jpeg_sdslp_mem_pd             :    1;
            uint32_t reserved5                     :    27;
        };
        uint32_t val;
    } sys_lslp_mem_pd;
    union {
        struct {
            uint32_t en                            :    1;
            uint32_t bit_out                       :    1;
            uint32_t bit_in                        :    1;
            uint32_t clk_en                        :    1;
            uint32_t reserved4                     :    28;
        };
        uint32_t val;
    } eco_cell_en_and_data;
    union {
        struct {
            uint32_t usb_mem_aux_ctrl              :    14;
            uint32_t otg_phy_test_done             :    1;
            uint32_t otg_phy_bisten                :    1;
            uint32_t reserved16                    :    16;
        };
        uint32_t val;
    } usb_mem_aux_ctrl;
    union {
        struct {
            uint32_t dual_mspi_apb_clk_en          :    1;
            uint32_t dual_mspi_rstn                :    1;
            uint32_t dual_mspi_force_norst         :    1;
            uint32_t reserved3                     :    29;
        };
        uint32_t val;
    } dual_mspi_ctrl;
    union {
        struct {
            uint32_t hp_peri_rdn_eco_en            :    1;
            uint32_t hp_peri_rdn_eco_result        :    1;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } hp_peri_rdn_eco_cs;
    uint32_t hp_peri_rdn_eco_low;
    uint32_t hp_peri_rdn_eco_high;
} sys_clkrst_dev_t;
extern sys_clkrst_dev_t SYS_CLKRST;
#ifdef __cplusplus
}
#endif



#endif /*_SOC_SYS_CLKRST_STRUCT_H_ */
