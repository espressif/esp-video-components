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
#ifndef _SOC_ICM_SYS_STRUCT_H_
#define _SOC_ICM_SYS_STRUCT_H_


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
    union {
        struct {
            uint32_t dlock_mst                     :    4;  /*Lowest numbered deadlocked master*/
            uint32_t dlock_slv                     :    3;  /*Slave with which dlock_mst is deadlocked*/
            uint32_t dlock_id                      :    4;  /*AXI ID of deadlocked transaction*/
            uint32_t dlock_wr                      :    1;  /*Asserted if deadlocked transaction is a write*/
            uint32_t reserved12                    :    20;
        };
        uint32_t val;
    } dlock_status;
    union {
        struct {
            uint32_t dlock                         :    1;
            uint32_t addrhole                      :    1;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } int_raw;
    union {
        struct {
            uint32_t dlock                         :    1;
            uint32_t addrhole                      :    1;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } int_st;
    union {
        struct {
            uint32_t dlock                         :    1;
            uint32_t addrhole                      :    1;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } int_ena;
    union {
        struct {
            uint32_t dlock                         :    1;
            uint32_t addrhole                      :    1;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } int_clr;
    union {
        struct {
            uint32_t cpu_priority                  :    4;  /*CPU arbitration priority for command channels between masters connected to sys_icm*/
            uint32_t cache_priority                :    4;  /*CACHE arbitration priority for command channels between masters connected to sys_icm*/
            uint32_t usb_priority                  :    4;  /*USB arbitration priority for command channels between masters connected to sys_icm*/
            uint32_t gmac_priority                 :    4;  /*GMAC arbitration priority for command channels between masters connected to sys_icm*/
            uint32_t sdmmc_priority                :    4;  /*SDMMC arbitration priority for command channels between masters connected to sys_icm*/
            uint32_t gdma_mst1_priority            :    4;  /*GDMA mst1 arbitration priority for command channels between masters connected to sys_icm*/
            uint32_t gdma_mst2_priority            :    4;  /*GDMA mst2 arbitration priority for command channels between masters connected to sys_icm*/
            uint32_t pdma_int_priority             :    4;  /*PDMA INT arbitration priority for command channels between masters connected to sys_icm*/
        };
        uint32_t val;
    } mst_arb_priority_reg0;
    union {
        struct {
            uint32_t pdma_ext_priority             :    4;  /*PDMA EXT arbitration priority for command channels between masters connected to sys_icm*/
            uint32_t jpeg_priority                 :    4;  /*JPEG arbitration priority for command channels between masters connected to sys_icm*/
            uint32_t dma2d_priority                :    4;  /*GFX arbitration priority for command channels between masters connected to sys_icm*/
            uint32_t usbotg11_priority             :    4;  /*USB OTG1.1 arbitration priority for command channels between masters connected to sys_icm*/
            uint32_t sdio_slave_priority           :    4;  /*SDIO SLAVE arbitration priority for command channels between masters connected to sys_icm*/
            uint32_t reserved20                    :    12;
        };
        uint32_t val;
    } mst_arb_priority_reg1;
    union {
        struct {
            uint32_t tcm_priority                  :    3;  /*TCM arbitration priority for response channels between slaves connected to sys_icm*/
            uint32_t l2mem_priority                :    3;  /*L2MEM arbitration priority for response channels between slaves connected to sys_icm*/
            uint32_t mspi_priority                 :    3;  /*MSPI arbitration priority for response channels between slaves connected to sys_icm*/
            uint32_t ddr0_priority                 :    3;  /*DDR0 arbitration priority for response channels between slaves connected to sys_icm*/
            uint32_t ddr1_priority                 :    3;  /*DDR1 arbitration priority for response channels between slaves connected to sys_icm*/
            uint32_t psram_priority                :    3;  /*PSRAM arbitration priority for response channels between slaves connected to sys_icm*/
            uint32_t lcd_priority                  :    3;  /*MIPI_LCD registers arbitration priority for response channels between slaves connected to sys_icm*/
            uint32_t cam_priority                  :    3;  /*MIPI_CAM registers arbitration priority for response channels between slaves connected to sys_icm*/
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } slv_arb_priority;
    union {
        struct {
            uint32_t cpu_arqos                     :    4;
            uint32_t cache_arqos                   :    4;
            uint32_t usb_arqos                     :    4;
            uint32_t gmac_arqos                    :    4;
            uint32_t sdmmc_arqos                   :    4;
            uint32_t gdma_mst1_arqos               :    4;
            uint32_t gdma_mst2_arqos               :    4;
            uint32_t pdma_int_arqos                :    4;
        };
        uint32_t val;
    } mst_arqos_reg0;
    union {
        struct {
            uint32_t pdma_ext_arqos                :    4;
            uint32_t jpeg_arqos                    :    4;
            uint32_t dma2d_arqos                   :    4;
            uint32_t usbotg11_arqos                :    4;
            uint32_t sdio_slave_arqos              :    4;
            uint32_t reserved20                    :    12;
        };
        uint32_t val;
    } mst_arqos_reg1;
    union {
        struct {
            uint32_t cpu_awqos                     :    4;
            uint32_t cache_awqos                   :    4;
            uint32_t usb_awqos                     :    4;
            uint32_t gmac_awqos                    :    4;
            uint32_t sdmmc_awqos                   :    4;
            uint32_t gdma_mst1_awqos               :    4;
            uint32_t gdma_mst2_awqos               :    4;
            uint32_t pdma_int_awqos                :    4;
        };
        uint32_t val;
    } mst_awqos_reg0;
    union {
        struct {
            uint32_t pdma_ext_awqos                :    4;
            uint32_t jpeg_awqos                    :    4;
            uint32_t dma2d_awqos                   :    4;
            uint32_t usbotg11_awqos                :    4;
            uint32_t sdio_slave_awqos              :    4;
            uint32_t reserved20                    :    12;
        };
        uint32_t val;
    } mst_awqos_reg1;
    uint32_t addrhole_addr;
    union {
        struct {
            uint32_t addrhole_id                   :    8;  /*see bud_id_assignment.csv for details*/
            uint32_t addrhole_wr                   :    1;
            uint32_t addrhole_secure               :    1;
            uint32_t reserved10                    :    22;
        };
        uint32_t val;
    } addrhole_info;
    union {
        struct {
            uint32_t dlock_timeout                 :    13;  /*if no response until reg_dlock_timeout bus clock cycle, deadlock will happen*/
            uint32_t reserved13                    :    19;
        };
        uint32_t val;
    } dlock_timeout;
    uint32_t reserved_44;
    uint32_t reserved_48;
    uint32_t reserved_4c;
    union {
        struct {
            uint32_t rdn_eco_en                    :    1;
            uint32_t rdn_eco_result                :    1;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } rdn_eco_cs;
    uint32_t rdn_eco_low;
    uint32_t rdn_eco_high;
} icm_sys_dev_t;
extern icm_sys_dev_t ICM_SYS;
#ifdef __cplusplus
}
#endif



#endif /*_SOC_ICM_SYS_STRUCT_H_ */
