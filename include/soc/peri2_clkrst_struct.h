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
#ifndef _SOC_PERI2_CLKRST_STRUCT_H_
#define _SOC_PERI2_CLKRST_STRUCT_H_


#ifdef __cplusplus
extern "C" {
#endif

typedef volatile struct {
    uint32_t peri2_clk_ver_date;
    union {
        struct {
            uint32_t uart0_clk_en                  :    1;  /*pdma icm clock enable*/
            uint32_t uart1_clk_en                  :    1;  /*pdma icm clock enable*/
            uint32_t uart2_clk_en                  :    1;  /*pdma icm clock enable*/
            uint32_t uart3_clk_en                  :    1;  /*pdma icm clock enable*/
            uint32_t uart4_clk_en                  :    1;  /*pdma icm clock enable*/
            uint32_t uart_mem_clk_en               :    1;  /*pdma icm clock enable*/
            uint32_t i2s0_clk_en                   :    1;  /*pdma icm clock enable*/
            uint32_t i2s1_clk_en                   :    1;  /*pdma icm clock enable*/
            uint32_t i2s2_clk_en                   :    1;  /*pdma icm clock enable*/
            uint32_t spi2_clk_en                   :    1;  /*pdma icm clock enable*/
            uint32_t spi3_clk_en                   :    1;  /*pdma icm clock enable*/
            uint32_t uhci0_clk_en                  :    1;  /*pdma icm clock enable*/
            uint32_t pdma_clk_en                   :    1;  /*pdma icm clock enable*/
            uint32_t crypto_hmac_clk_en            :    1;  /*pdma icm clock enable*/
            uint32_t crypto_ds_clk_en              :    1;  /*pdma icm clock enable*/
            uint32_t crypto_rsa_clk_en             :    1;  /*pdma icm clock enable*/
            uint32_t crypto_sha_clk_en             :    1;  /*pdma icm clock enable*/
            uint32_t crypto_aes_clk_en             :    1;  /*pdma icm clock enable*/
            uint32_t lcdcam_clk_en                 :    1;  /*pdma icm clock enable*/
            uint32_t reserved19                    :    1;
            uint32_t adc_clk_en                    :    1;  /*pdma icm clock enable*/
            uint32_t reserved21                    :    1;
            uint32_t reserved22                    :    1;
            uint32_t crypto_clk_en                 :    1;  /*crypto engine clock enable*/
            uint32_t i3c_clk_en                    :    1;  /*i3c master clock enable*/
            uint32_t ecc_clk_en                    :    1;  /*ecc clock enable*/
            uint32_t clk_en                        :    1;
            uint32_t reserved27                    :    5;
        };
        uint32_t val;
    } peri2_clk_en_ctrl;
    union {
        struct {
            uint32_t uart0_apb_rstn                :    1;  /*software reset: low active*/
            uint32_t uart1_apb_rstn                :    1;  /*software reset: low active*/
            uint32_t uart2_apb_rstn                :    1;  /*software reset: low active*/
            uint32_t uart3_apb_rstn                :    1;  /*software reset: low active*/
            uint32_t uart4_apb_rstn                :    1;  /*software reset: low active*/
            uint32_t uart_mem_rstn                 :    1;  /*software reset: low active*/
            uint32_t uhci0_rstn                    :    1;  /*software reset: low active*/
            uint32_t i2s0_apb_rstn                 :    1;  /*software reset: low active*/
            uint32_t i2s1_apb_rstn                 :    1;  /*software reset: low active*/
            uint32_t i2s2_apb_rstn                 :    1;  /*software reset: low active*/
            uint32_t spi2_rstn                     :    1;  /*software reset: low active*/
            uint32_t spi3_rstn                     :    1;  /*software reset: low active*/
            uint32_t pdma_rstn                     :    1;  /*software reset: low active*/
            uint32_t crypto_hmac_rstn_en           :    1;  /*software reset: low active*/
            uint32_t crypto_ds_rstn_en             :    1;  /*software reset: low active*/
            uint32_t crypto_rsa_rstn_en            :    1;  /*software reset: low active*/
            uint32_t crypto_sha_rstn_en            :    1;  /*software reset: low active*/
            uint32_t crypto_aes_rstn_en            :    1;  /*software reset: low active*/
            uint32_t lcdcam_rstn                   :    1;  /*software reset: low active*/
            uint32_t reserved19                    :    1;
            uint32_t adc_rstn                      :    1;  /*software reset: low active*/
            uint32_t reserved21                    :    1;
            uint32_t reserved22                    :    1;
            uint32_t crypto_rstn                   :    1;  /*software reset: low active*/
            uint32_t i3c_apb_rstn                  :    1;  /*software reset: low active*/
            uint32_t ecc_rstn                      :    1;  /*software reset: low active*/
            uint32_t reserved26                    :    6;
        };
        uint32_t val;
    } peri2_rst_ctrl;
    union {
        struct {
            uint32_t uart0_force_norst             :    1;  /*software force no reset*/
            uint32_t uart1_force_norst             :    1;  /*software force no reset*/
            uint32_t uart2_force_norst             :    1;  /*software force no reset*/
            uint32_t uart3_force_norst             :    1;  /*software force no reset*/
            uint32_t uart4_force_norst             :    1;  /*software force no reset*/
            uint32_t uart_mem_force_norst          :    1;  /*software force no reset*/
            uint32_t uhci0_force_norst             :    1;  /*software force no reset*/
            uint32_t i2s0_force_norst              :    1;  /*software force no reset*/
            uint32_t i2s1_force_norst              :    1;  /*software force no reset*/
            uint32_t i2s2_force_norst              :    1;  /*software force no reset*/
            uint32_t spi2_force_norst              :    1;  /*software force no reset*/
            uint32_t spi3_force_norst              :    1;  /*software force no reset*/
            uint32_t pdma_force_norst              :    1;  /*software force no reset*/
            uint32_t lcdcam_force_norst            :    1;  /*software force no reset*/
            uint32_t reserved14                    :    1;
            uint32_t adc_force_norst               :    1;  /*software force no reset*/
            uint32_t reserved16                    :    1;
            uint32_t reserved17                    :    1;
            uint32_t crypto_force_norst            :    1;  /*software force no reset*/
            uint32_t i3c_force_norst               :    1;  /*software force no reset*/
            uint32_t ecc_force_norst               :    1;  /*software force no reset*/
            uint32_t reserved21                    :    11;
        };
        uint32_t val;
    } peri2_force_norst_ctrl;
    union {
        struct {
            uint32_t i3c_mst_clk_div_num           :    4;  /*i3c master core clock div num*/
            uint32_t i3c_mst_clk_sel               :    2;  /*i3c master core clock src sel*/
            uint32_t reserved6                     :    26;
        };
        uint32_t val;
    } i3c_mst_clk_div_ctrl;
    union {
        struct {
            uint32_t peri2_apb_clk_div_num         :    4;  /*peri2 apb clock div num*/
            uint32_t reserved4                     :    28;
        };
        uint32_t val;
    } peri2_apb_clk_div_ctrl;
    union {
        struct {
            uint32_t peri2_slow_apb_postw_en       :    1;  /*peri2 120MHz peri apb post write enable*/
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } peri2_slow_apb_postw_en;
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
    union {
        struct {
            uint32_t uart_mem_aux_ctrl             :    14;
            uint32_t reserved14                    :    18;
        };
        uint32_t val;
    } uart_mem_aux_ctrl;
    union {
        struct {
            uint32_t en_sec_rnd_clk                :    1;
            uint32_t sec_dpa_cfg_sel               :    1;
            uint32_t sec_dpa_level                 :    2;
            uint32_t reserved4                     :    28;
        };
        uint32_t val;
    } crypto_clk_random_ctrl;
    uint32_t reserved_2c;
    union {
        struct {
            uint32_t spi_mst_div_num               :    4;  /*spi master div num*/
            uint32_t reserved4                     :    28;
        };
        uint32_t val;
    } spi_mst_clk_div_ctrl;
    union {
        struct {
            uint32_t hp_peri2_rdn_eco_en           :    1;
            uint32_t hp_peri2_rdn_eco_result       :    1;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } hp_peri2_rdn_eco_cs;
    uint32_t hp_peri2_rdn_eco_low;
    uint32_t hp_peri2_rdn_eco_high;
} peri2_clkrst_dev_t;
extern peri2_clkrst_dev_t PERI2_CLKRST;
#ifdef __cplusplus
}
#endif



#endif /*_SOC_PERI2_CLKRST_STRUCT_H_ */
