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
#ifndef _SOC_DMA2D_STRUCT_H_
#define _SOC_DMA2D_STRUCT_H_


#ifdef __cplusplus
extern "C" {
#endif

typedef volatile struct {
    union {
        struct {
            uint32_t auto_wrback                   :    1;  /*Set this bit to enable automatic outlink-writeback when all the data pointed by outlink descriptor has been received.*/
            uint32_t eof_mode                      :    1;  /*EOF flag generation mode when receiving data. 1: EOF flag for Tx channel 0 is generated when data need to read has been popped from FIFO in DMA*/
            uint32_t outdscr_burst_en              :    1;  /*Set this bit to 1 to enable INCR burst transfer for Tx channel 0 reading link descriptor when accessing internal SRAM. */
            uint32_t ecc_aes_en                    :    1;  /*When access address space is ecc/aes area, this bit should be set to 1. In this case, the start address of square should be 16-bit aligned. The width of square multiply byte number of one pixel should be 16-bit aligned. */
            uint32_t check_owner                   :    1;  /*Set this bit to enable checking the owner attribute of the link descriptor.*/
            uint32_t loop_test                     :    1;  /*reserved*/
            uint32_t mem_burst_length              :    3;  /*Block size of Tx channel 0. 0: single      1: 16 bytes      2: 32 bytes    3: 64 bytes    4: 128 bytes*/
            uint32_t macro_block_size              :    2;  /*Sel TX macro-block size 0: 8pixel*8pixel     1: 8pixel*16pixel     2: 16pixel*16pixel     3: no macro-block     , only useful in mode 1 of the link descriptor*/
            uint32_t dscr_port_en                  :    1;  /*Set this bit to 1 to obtain descriptor from IP port*/
            uint32_t page_bound_en                 :    1;  /*Set this bit to 1 to make sure AXI read data don't cross the address boundary which define by mem_burst_length*/
            uint32_t reserved13                    :    3;  /*reserved*/
            uint32_t reorder_en                    :    1;  /*Enable TX channel 0 macro block reorder when set to 1, only channel0 have this selection*/
            uint32_t reserved17                    :    7;  /*reserved*/
            uint32_t rst                           :    1;  /*Write 1 then write 0 to this bit to reset TX channel*/
            uint32_t cmd_disable                   :    1;  /*Write 1 before reset and write 0 after reset*/
            uint32_t arb_weight_opt_dis            :    1;  /*Set this bit to 1 to disable arbiter optimum weight function.*/
            uint32_t reserved27                    :    5;  /*reserved*/
        };
        uint32_t val;
    } out_conf0_ch0;
    union {
        struct {
            uint32_t done                          :    1;  /*The raw interrupt bit turns to high level when the last data pointed by one outlink descriptor has been transmitted to peripherals for Tx channel 0.*/
            uint32_t eof                           :    1;  /*The raw interrupt bit turns to high level when the last data pointed by one outlink descriptor has been read from memory for Tx channel 0. */
            uint32_t dscr_err                      :    1;  /*The raw interrupt bit turns to high level when detecting outlink descriptor error, including owner error, the second and third word error of outlink descriptor for Tx channel 0.*/
            uint32_t total_eof                     :    1;  /*The raw interrupt bit turns to high level when data corresponding a outlink (includes one link descriptor or few link descriptors) is transmitted out for Tx channel 0.*/
            uint32_t outfifo_ovf                   :    1;  /*The raw interrupt bit turns to high level when fifo is overflow. */
            uint32_t outfifo_udf                   :    1;  /*The raw interrupt bit turns to high level when fifo is underflow. */
            uint32_t outfifo_ro_ovf                :    1;  /*The raw interrupt bit turns to high level when reorder fifo is overflow. */
            uint32_t outfifo_ro_udf                :    1;  /*The raw interrupt bit turns to high level when reorder fifo is underflow. */
            uint32_t reserved8                     :    24;
        };
        uint32_t val;
    } out_int_raw_ch0;
    union {
        struct {
            uint32_t done                          :    1;  /*The interrupt enable bit for the OUT_DONE_CH_INT interrupt.*/
            uint32_t eof                           :    1;  /*The interrupt enable bit for the OUT_EOF_CH_INT interrupt.*/
            uint32_t dscr_err                      :    1;  /*The interrupt enable bit for the OUT_DSCR_ERR_CH_INT interrupt.*/
            uint32_t total_eof                     :    1;  /*The interrupt enable bit for the OUT_TOTAL_EOF_CH_INT interrupt.*/
            uint32_t outfifo_ovf                   :    1;  /*The interrupt enable bit for the OUTFIFO_OVF_CH_INT interrupt.*/
            uint32_t outfifo_udf                   :    1;  /*The interrupt enable bit for the OUTFIFO_UDF_CH_INT interrupt.*/
            uint32_t outfifo_ro_ovf                :    1;  /*The interrupt enable bit for the OUTFIFO_RO_OVF_CH_INT interrupt.*/
            uint32_t outfifo_ro_udf                :    1;  /*The interrupt enable bit for the OUTFIFO_RO_UDF_CH_INT interrupt.*/
            uint32_t reserved8                     :    24;
        };
        uint32_t val;
    } out_int_ena_ch0;
    union {
        struct {
            uint32_t done                          :    1;  /*The raw interrupt status bit for the OUT_DONE_CH_INT interrupt.*/
            uint32_t eof                           :    1;  /*The raw interrupt status bit for the OUT_EOF_CH_INT interrupt.*/
            uint32_t dscr_err                      :    1;  /*The raw interrupt status bit for the OUT_DSCR_ERR_CH_INT interrupt.*/
            uint32_t total_eof                     :    1;  /*The raw interrupt status bit for the OUT_TOTAL_EOF_CH_INT interrupt.*/
            uint32_t outfifo_ovf                   :    1;  /*The raw interrupt status bit for the OUTFIFO_OVF_CH_INT interrupt.*/
            uint32_t outfifo_udf                   :    1;  /*The raw interrupt status bit for the OUTFIFO_UDF_CH_INT interrupt.*/
            uint32_t outfifo_ro_ovf                :    1;  /*The raw interrupt status bit for the OUTFIFO_RO_OVF_CH_INT interrupt.*/
            uint32_t outfifo_ro_udf                :    1;  /*The raw interrupt status bit for the OUTFIFO_RO_UDF_CH_INT interrupt.*/
            uint32_t reserved8                     :    24;
        };
        uint32_t val;
    } out_int_st_ch0;
    union {
        struct {
            uint32_t done                          :    1;  /*Set this bit to clear the OUT_DONE_CH_INT interrupt.*/
            uint32_t eof                           :    1;  /*Set this bit to clear the OUT_EOF_CH_INT interrupt.*/
            uint32_t dscr_err                      :    1;  /*Set this bit to clear the OUT_DSCR_ERR_CH_INT interrupt.*/
            uint32_t total_eof                     :    1;  /*Set this bit to clear the OUT_TOTAL_EOF_CH_INT interrupt.*/
            uint32_t outfifo_ovf                   :    1;  /*Set this bit to clear the OUTFIFO_OVF_CH_INT interrupt.*/
            uint32_t outfifo_udf                   :    1;  /*Set this bit to clear the OUTFIFO_UDF_CH_INT interrupt.*/
            uint32_t outfifo_ro_ovf                :    1;  /*Set this bit to clear the OUTFIFO_RO_OVF_CH_INT interrupt.*/
            uint32_t outfifo_ro_udf                :    1;  /*Set this bit to clear the OUTFIFO_RO_UDF_CH_INT interrupt.*/
            uint32_t reserved8                     :    24;
        };
        uint32_t val;
    } out_int_clr_ch0;
    union {
        struct {
            uint32_t outfifo_full                  :    1;  /*Tx FIFO full signal for Tx channel 0.*/
            uint32_t outfifo_empty                 :    1;  /*Tx FIFO empty signal for Tx channel 0.*/
            uint32_t outfifo_cnt                   :    9;  /*The register stores the byte number of the data in Tx FIFO for Tx channel 0.*/
            uint32_t remaunder_1b                  :    1;  /*reserved*/
            uint32_t remaunder_2b                  :    1;  /*reserved*/
            uint32_t remaunder_3b                  :    1;  /*reserved*/
            uint32_t remaunder_4b                  :    1;  /*reserved*/
            uint32_t remaunder_5b                  :    1;  /*reserved*/
            uint32_t remaunder_6b                  :    1;  /*reserved*/
            uint32_t remaunder_7b                  :    1;  /*reserved*/
            uint32_t remaunder_8b                  :    1;  /*reserved*/
            uint32_t pixel_byte                    :    4;  /*the number of bytes contained in a pixel at TX channel     0: 1byte     1: 1.5bytes     2 : 2bytes     3: 2.5bytes     4: 3bytes     5: 4bytes*/
            uint32_t burst_block_num               :    4;  /*the number of macro blocks contained in a burst of data at TX channel*/
            uint32_t reserved27                    :    5;  /*reserved*/
        };
        uint32_t val;
    } outfifo_status_ch0;
    union {
        struct {
            uint32_t outfifo_wdata                 :    10;  /*This register stores the data that need to be pushed into DMA Tx FIFO.*/
            uint32_t outfifo_push                  :    1;  /*Set this bit to push data into DMA Tx FIFO.*/
            uint32_t reserved11                    :    21;  /*reserved*/
        };
        uint32_t val;
    } out_push_ch0;
    union {
        struct {
            uint32_t reserved0                     :    20;  /*reserved*/
            uint32_t outlink_stop                  :    1;  /*Set this bit to stop dealing with the outlink descriptors.*/
            uint32_t outlink_start                 :    1;  /*Set this bit to start dealing with the outlink descriptors.*/
            uint32_t outlink_restart               :    1;  /*Set this bit to restart a new outlink from the last address. */
            uint32_t outlink_park                  :    1;  /*1: the outlink descriptor's FSM is in idle state.  0: the outlink descriptor's FSM is working.*/
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } out_link_conf_ch0;
    uint32_t out_link_addr_ch0;
    union {
        struct {
            uint32_t outlink_dscr_addr             :    18;  /*This register stores the current outlink descriptor's address.*/
            uint32_t dscr_state                    :    2;  /*This register stores the current descriptor state machine state.*/
            uint32_t state                         :    4;  /*This register stores the current control module state machine state.*/
            uint32_t reset_avail                   :    1;  /*This register indicate that if the channel reset is safety.*/
            uint32_t reserved25                    :    7;  /*reserved*/
        };
        uint32_t val;
    } out_state_ch0;
    uint32_t out_eof_des_addr_ch0;
    uint32_t out_dscr_ch0;
    uint32_t out_dscr_bf0_ch0;
    uint32_t out_dscr_bf1_ch0;
    union {
        struct {
            uint32_t peri_sel                      :    3;  /*This register is used to select peripheral for Tx channel   0:  jpeg     1: display-1     2: display-2     3: display-3      7: no choose*/
            uint32_t reserved3                     :    29;
        };
        uint32_t val;
    } out_peri_sel_ch0;
    union {
        struct {
            uint32_t arb_token_num                 :    4;  /*Set the max number of token count of arbiter*/
            uint32_t arb_priority                  :    2;  /*Set the priority of channel*/
            uint32_t reserved6                     :    26;
        };
        uint32_t val;
    } out_arb_ch0;
    union {
        struct {
            uint32_t outfifo_ro_cnt                :    6;  /*The register stores the byte number of the data in color convert Tx FIFO for channel 0.*/
            uint32_t ro_wr_state                   :    2;  /*The register stores the state of read ram of reorder*/
            uint32_t ro_rd_state                   :    2;  /*The register stores the state of write ram of reorder*/
            uint32_t reserved10                    :    22;  /*reserved*/
        };
        uint32_t val;
    } out_ro_status_ch0;
    union {
        struct {
            uint32_t reserved0                     :    4;  /*reserved*/
            uint32_t ro_ram_force_pd               :    1;  /*dma reorder ram power down*/
            uint32_t ro_ram_force_pu               :    1;  /*dma reorder ram power up*/
            uint32_t ro_ram_clk_fo                 :    1;  /*1: Force to open the clock and bypass the gate-clock when accessing the RAM in DMA. 0: A gate-clock will be used when accessing the RAM in DMA.*/
            uint32_t reserved7                     :    25;
        };
        uint32_t val;
    } out_ro_pd_conf_ch0;
    union {
        struct {
            uint32_t color_output_sel              :    2;  /*Set final color convert process and output type    0: RGB888 to RGB565     1: YUV444 to YUV422     2: output directly*/
            uint32_t color_3b_proc_en              :    1;  /*Enable generic color convert modlue between color input & color output, need to configure parameter.*/
            uint32_t color_input_sel               :    3;  /*Set first color convert process and input color type    0: RGB565 to RGB888     1: YUV422 to YUV444      2: Other 2byte/pixel type    3: Other 3byte/pixel type    7: disable color space convert*/
            uint32_t reserved6                     :    26;  /*reserved*/
        };
        uint32_t val;
    } out_color_convert_ch0;
    union {
        struct {
            uint32_t scramble_sel_pre              :    3;  /*Scramble data before color convert :  0 : BYTE2-1-0   1 : BYTE2-0-1    2 : BYTE1-0-2    3 : BYTE1-2-0    4 : BYTE0-2-1    5 : BYTE0-1-2*/
            uint32_t reserved3                     :    29;  /*reserved*/
        };
        uint32_t val;
    } out_scramble_ch0;
    union {
        struct {
            uint32_t color_param_h0                :    21;  /*Set first 2 parameter of most significant byte of pending 3 bytes*/
            uint32_t reserved21                    :    11;  /*reserved*/
        };
        uint32_t val;
    } out_color_param0_ch0;
    union {
        struct {
            uint32_t color_param_h1                :    28;  /*Set last 2 parameter of most significant byte of pending 3 bytes*/
            uint32_t reserved28                    :    4;  /*reserved*/
        };
        uint32_t val;
    } out_color_param1_ch0;
    union {
        struct {
            uint32_t color_param_m0                :    21;  /*Set first 2 parameter of midium significant byte of pending 3 bytes*/
            uint32_t reserved21                    :    11;  /*reserved*/
        };
        uint32_t val;
    } out_color_param2_ch0;
    union {
        struct {
            uint32_t color_param_m1                :    28;  /*Set last 2 parameter of midium significant byte of pending 3 bytes*/
            uint32_t reserved28                    :    4;  /*reserved*/
        };
        uint32_t val;
    } out_color_param3_ch0;
    union {
        struct {
            uint32_t color_param_l0                :    21;  /*Set first 2 parameter of least significant byte of pending 3 bytes*/
            uint32_t reserved21                    :    11;  /*reserved*/
        };
        uint32_t val;
    } out_color_param4_ch0;
    union {
        struct {
            uint32_t color_param_l1                :    28;  /*Set last 2 parameter of least significant byte of pending 3 bytes*/
            uint32_t reserved28                    :    4;  /*reserved*/
        };
        uint32_t val;
    } out_color_param5_ch0;
    uint32_t reserved_68;
    uint32_t reserved_6c;
    uint32_t reserved_70;
    uint32_t reserved_74;
    uint32_t reserved_78;
    uint32_t reserved_7c;
    uint32_t reserved_80;
    uint32_t reserved_84;
    uint32_t reserved_88;
    uint32_t reserved_8c;
    uint32_t reserved_90;
    uint32_t reserved_94;
    uint32_t reserved_98;
    uint32_t reserved_9c;
    uint32_t reserved_a0;
    uint32_t reserved_a4;
    uint32_t reserved_a8;
    uint32_t reserved_ac;
    uint32_t reserved_b0;
    uint32_t reserved_b4;
    uint32_t reserved_b8;
    uint32_t reserved_bc;
    uint32_t reserved_c0;
    uint32_t reserved_c4;
    uint32_t reserved_c8;
    uint32_t reserved_cc;
    uint32_t reserved_d0;
    uint32_t reserved_d4;
    uint32_t reserved_d8;
    uint32_t reserved_dc;
    uint32_t reserved_e0;
    uint32_t reserved_e4;
    uint32_t reserved_e8;
    uint32_t reserved_ec;
    uint32_t reserved_f0;
    uint32_t reserved_f4;
    uint32_t reserved_f8;
    uint32_t reserved_fc;
    union {
        struct {
            uint32_t auto_wrback                   :    1;  /*Set this bit to enable automatic outlink-writeback when all the data pointed by outlink descriptor has been received.*/
            uint32_t eof_mode                      :    1;  /*EOF flag generation mode when receiving data. 1: EOF flag for Tx channel 0 is generated when data need to read has been popped from FIFO in DMA*/
            uint32_t outdscr_burst_en              :    1;  /*Set this bit to 1 to enable INCR burst transfer for Tx channel 0 reading link descriptor when accessing internal SRAM. */
            uint32_t ecc_aes_en                    :    1;  /*When access address space is ecc/aes area, this bit should be set to 1. In this case, the start address of square should be 16-bit aligned. The width of square multiply byte number of one pixel should be 16-bit aligned. */
            uint32_t check_owner                   :    1;  /*Set this bit to enable checking the owner attribute of the link descriptor.*/
            uint32_t loop_test                     :    1;  /*reserved*/
            uint32_t mem_burst_length              :    3;  /*Block size of Tx channel 1. 0: single      1: 16 bytes      2: 32 bytes    3: 64 bytes    4: 128 64 bytes*/
            uint32_t macro_block_size              :    2;  /*Sel TX macro-block size 0: 8pixel*8pixel     1: 8pixel*16pixel     2: 16pixel*16pixel     3: no macro-block     , only useful in mode 1 of the link descriptor*/
            uint32_t dscr_port_en                  :    1;  /*Set this bit to 1 to obtain descriptor from IP port*/
            uint32_t page_bound_en                 :    1;  /*Set this bit to 1 to make sure AXI read data don't cross the address boundary which define by mem_burst_length*/
            uint32_t reserved13                    :    11;  /*reserved*/
            uint32_t rst                           :    1;  /*Write 1 then write 0 to this bit to reset TX channel*/
            uint32_t cmd_disable                   :    1;  /*Write 1 before reset and write 0 after reset*/
            uint32_t arb_weight_opt_dis            :    1;  /*Set this bit to 1 to disable arbiter optimum weight function.*/
            uint32_t reserved27                    :    5;  /*reserved*/
        };
        uint32_t val;
    } out_conf0_ch1;
    union {
        struct {
            uint32_t done                          :    1;  /*The raw interrupt bit turns to high level when the last data pointed by one outlink descriptor has been transmitted to peripherals for Tx channel 0.*/
            uint32_t eof                           :    1;  /*The raw interrupt bit turns to high level when the last data pointed by one outlink descriptor has been read from memory for Tx channel 0. */
            uint32_t dscr_err                      :    1;  /*The raw interrupt bit turns to high level when detecting outlink descriptor error, including owner error, the second and third word error of outlink descriptor for Tx channel 0.*/
            uint32_t total_eof                     :    1;  /*The raw interrupt bit turns to high level when data corresponding a outlink (includes one link descriptor or few link descriptors) is transmitted out for Tx channel 0.*/
            uint32_t outfifo_ovf                   :    1;  /*The raw interrupt bit turns to high level when fifo is overflow. */
            uint32_t outfifo_udf                   :    1;  /*The raw interrupt bit turns to high level when fifo is underflow. */
            uint32_t outfifo_ro_ovf                :    1;  /*The raw interrupt bit turns to high level when reorder fifo is overflow. */
            uint32_t outfifo_ro_udf                :    1;  /*The raw interrupt bit turns to high level when reorder fifo is underflow. */
            uint32_t reserved8                     :    24;
        };
        uint32_t val;
    } out_int_raw_ch1;
    union {
        struct {
            uint32_t done                          :    1;  /*The interrupt enable bit for the OUT_DONE_CH_INT interrupt.*/
            uint32_t eof                           :    1;  /*The interrupt enable bit for the OUT_EOF_CH_INT interrupt.*/
            uint32_t dscr_err                      :    1;  /*The interrupt enable bit for the OUT_DSCR_ERR_CH_INT interrupt.*/
            uint32_t total_eof                     :    1;  /*The interrupt enable bit for the OUT_TOTAL_EOF_CH_INT interrupt.*/
            uint32_t outfifo_ovf                   :    1;  /*The interrupt enable bit for the OUTFIFO_OVF_CH_INT interrupt.*/
            uint32_t outfifo_udf                   :    1;  /*The interrupt enable bit for the OUTFIFO_UDF_CH_INT interrupt.*/
            uint32_t outfifo_ro_ovf                :    1;  /*The interrupt enable bit for the OUTFIFO_RO_OVF_CH_INT interrupt.*/
            uint32_t outfifo_ro_udf                :    1;  /*The interrupt enable bit for the OUTFIFO_RO_UDF_CH_INT interrupt.*/
            uint32_t reserved8                     :    24;
        };
        uint32_t val;
    } out_int_ena_ch1;
    union {
        struct {
            uint32_t done                          :    1;  /*The raw interrupt status bit for the OUT_DONE_CH_INT interrupt.*/
            uint32_t eof                           :    1;  /*The raw interrupt status bit for the OUT_EOF_CH_INT interrupt.*/
            uint32_t dscr_err                      :    1;  /*The raw interrupt status bit for the OUT_DSCR_ERR_CH_INT interrupt.*/
            uint32_t total_eof                     :    1;  /*The raw interrupt status bit for the OUT_TOTAL_EOF_CH_INT interrupt.*/
            uint32_t outfifo_ovf                   :    1;  /*The raw interrupt status bit for the OUTFIFO_OVF_CH_INT interrupt.*/
            uint32_t outfifo_udf                   :    1;  /*The raw interrupt status bit for the OUTFIFO_UDF_CH_INT interrupt.*/
            uint32_t outfifo_ro_ovf                :    1;  /*The raw interrupt status bit for the OUTFIFO_RO_OVF_CH_INT interrupt.*/
            uint32_t outfifo_ro_udf                :    1;  /*The raw interrupt status bit for the OUTFIFO_RO_UDF_CH_INT interrupt.*/
            uint32_t reserved8                     :    24;
        };
        uint32_t val;
    } out_int_st_ch1;
    union {
        struct {
            uint32_t done                          :    1;  /*Set this bit to clear the OUT_DONE_CH_INT interrupt.*/
            uint32_t eof                           :    1;  /*Set this bit to clear the OUT_EOF_CH_INT interrupt.*/
            uint32_t dscr_err                      :    1;  /*Set this bit to clear the OUT_DSCR_ERR_CH_INT interrupt.*/
            uint32_t total_eof                     :    1;  /*Set this bit to clear the OUT_TOTAL_EOF_CH_INT interrupt.*/
            uint32_t outfifo_ovf                   :    1;  /*Set this bit to clear the OUTFIFO_OVF_CH_INT interrupt.*/
            uint32_t outfifo_udf                   :    1;  /*Set this bit to clear the OUTFIFO_UDF_CH_INT interrupt.*/
            uint32_t outfifo_ro_ovf                :    1;  /*Set this bit to clear the OUTFIFO_RO_OVF_CH_INT interrupt.*/
            uint32_t outfifo_ro_udf                :    1;  /*Set this bit to clear the OUTFIFO_RO_UDF_CH_INT interrupt.*/
            uint32_t reserved8                     :    24;
        };
        uint32_t val;
    } out_int_clr_ch1;
    union {
        struct {
            uint32_t outfifo_full                  :    1;  /*Tx FIFO full signal for Tx channel 1.*/
            uint32_t outfifo_empty                 :    1;  /*Tx FIFO empty signal for Tx channel 1.*/
            uint32_t outfifo_cnt                   :    9;  /*The register stores the byte number of the data in Tx FIFO for Tx channel 1.*/
            uint32_t remaunder_1b                  :    1;  /*reserved*/
            uint32_t remaunder_2b                  :    1;  /*reserved*/
            uint32_t remaunder_3b                  :    1;  /*reserved*/
            uint32_t remaunder_4b                  :    1;  /*reserved*/
            uint32_t remaunder_5b                  :    1;  /*reserved*/
            uint32_t remaunder_6b                  :    1;  /*reserved*/
            uint32_t remaunder_7b                  :    1;  /*reserved*/
            uint32_t remaunder_8b                  :    1;  /*reserved*/
            uint32_t pixel_byte                    :    4;  /*the number of bytes contained in a pixel at TX channel     0: 1byte     1: 1.5bytes     2 : 2bytes     3: 2.5bytes     4: 3bytes     5: 4bytes*/
            uint32_t burst_block_num               :    4;  /*the number of macro blocks contained in a burst of data at TX channel*/
            uint32_t reserved27                    :    5;  /*reserved*/
        };
        uint32_t val;
    } outfifo_status_ch1;
    union {
        struct {
            uint32_t outfifo_wdata                 :    10;  /*This register stores the data that need to be pushed into DMA Tx FIFO.*/
            uint32_t outfifo_push                  :    1;  /*Set this bit to push data into DMA Tx FIFO.*/
            uint32_t reserved11                    :    21;  /*reserved*/
        };
        uint32_t val;
    } out_push_ch1;
    union {
        struct {
            uint32_t reserved0                     :    20;  /*reserved*/
            uint32_t outlink_stop                  :    1;  /*Set this bit to stop dealing with the outlink descriptors.*/
            uint32_t outlink_start                 :    1;  /*Set this bit to start dealing with the outlink descriptors.*/
            uint32_t outlink_restart               :    1;  /*Set this bit to restart a new outlink from the last address. */
            uint32_t outlink_park                  :    1;  /*1: the outlink descriptor's FSM is in idle state.  0: the outlink descriptor's FSM is working.*/
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } out_link_conf_ch1;
    uint32_t out_link_addr_ch1;
    union {
        struct {
            uint32_t outlink_dscr_addr             :    18;  /*This register stores the current outlink descriptor's address.*/
            uint32_t dscr_state                    :    2;  /*This register stores the current descriptor state machine state.*/
            uint32_t state                         :    4;  /*This register stores the current control module state machine state.*/
            uint32_t reset_avail                   :    1;  /*This register indicate that if the channel reset is safety.*/
            uint32_t reserved25                    :    7;  /*reserved*/
        };
        uint32_t val;
    } out_state_ch1;
    uint32_t out_eof_des_addr_ch1;
    uint32_t out_dscr_ch1;
    uint32_t out_dscr_bf0_ch1;
    uint32_t out_dscr_bf1_ch1;
    union {
        struct {
            uint32_t peri_sel                      :    3;  /*This register is used to select peripheral for Tx channel   0:  jpeg     1: display-1     2: display-2     3: display-3      7: no choose*/
            uint32_t reserved3                     :    29;
        };
        uint32_t val;
    } out_peri_sel_ch1;
    union {
        struct {
            uint32_t arb_token_num                 :    4;  /*Set the max number of token count of arbiter*/
            uint32_t arb_priority                  :    2;  /*Set the priority of channel*/
            uint32_t reserved6                     :    26;
        };
        uint32_t val;
    } out_arb_ch1;
    union {
        struct {
            uint32_t outfifo_ro_cnt                :    6;  /*The register stores the byte number of the data in color convert Tx FIFO for channel 1.*/
            uint32_t reserved6                     :    26;  /*reserved*/
        };
        uint32_t val;
    } out_ro_status_ch1;
    uint32_t reserved_144;
    union {
        struct {
            uint32_t color_output_sel              :    2;  /*Set final color convert process and output type    0: RGB888 to RGB565     1: YUV444 to YUV422     2: output directly*/
            uint32_t color_3b_proc_en              :    1;  /*Enable generic color convert modlue between color input & color output, need to configure parameter.*/
            uint32_t color_input_sel               :    3;  /*Set first color convert process and input color type    0: RGB565 to RGB888     1: YUV422 to YUV444      2: Other 2byte/pixel type    3: Other 3byte/pixel type    7: disable color space convert*/
            uint32_t reserved6                     :    26;  /*reserved*/
        };
        uint32_t val;
    } out_color_convert_ch1;
    union {
        struct {
            uint32_t scramble_sel_pre              :    3;  /*Scramble data before color convert :  0 : BYTE2-1-0   1 : BYTE2-0-1    2 : BYTE1-0-2    3 : BYTE1-2-0    4 : BYTE0-2-1    5 : BYTE0-1-2*/
            uint32_t reserved3                     :    29;  /*reserved*/
        };
        uint32_t val;
    } out_scramble_ch1;
    union {
        struct {
            uint32_t color_param_h0                :    21;  /*Set first 2 parameter of most significant byte of pending 3 bytes*/
            uint32_t reserved21                    :    11;  /*reserved*/
        };
        uint32_t val;
    } out_color_param0_ch1;
    union {
        struct {
            uint32_t color_param_h1                :    28;  /*Set last 2 parameter of most significant byte of pending 3 bytes*/
            uint32_t reserved28                    :    4;  /*reserved*/
        };
        uint32_t val;
    } out_color_param1_ch1;
    union {
        struct {
            uint32_t color_param_m0                :    21;  /*Set first 2 parameter of midium significant byte of pending 3 bytes*/
            uint32_t reserved21                    :    11;  /*reserved*/
        };
        uint32_t val;
    } out_color_param2_ch1;
    union {
        struct {
            uint32_t color_param_m1                :    28;  /*Set last 2 parameter of midium significant byte of pending 3 bytes*/
            uint32_t reserved28                    :    4;  /*reserved*/
        };
        uint32_t val;
    } out_color_param3_ch1;
    union {
        struct {
            uint32_t color_param_l0                :    21;  /*Set first 2 parameter of least significant byte of pending 3 bytes*/
            uint32_t reserved21                    :    11;  /*reserved*/
        };
        uint32_t val;
    } out_color_param4_ch1;
    union {
        struct {
            uint32_t color_param_l1                :    28;  /*Set last 2 parameter of least significant byte of pending 3 bytes*/
            uint32_t reserved28                    :    4;  /*reserved*/
        };
        uint32_t val;
    } out_color_param5_ch1;
    uint32_t reserved_168;
    uint32_t reserved_16c;
    uint32_t reserved_170;
    uint32_t reserved_174;
    uint32_t reserved_178;
    uint32_t reserved_17c;
    uint32_t reserved_180;
    uint32_t reserved_184;
    uint32_t reserved_188;
    uint32_t reserved_18c;
    uint32_t reserved_190;
    uint32_t reserved_194;
    uint32_t reserved_198;
    uint32_t reserved_19c;
    uint32_t reserved_1a0;
    uint32_t reserved_1a4;
    uint32_t reserved_1a8;
    uint32_t reserved_1ac;
    uint32_t reserved_1b0;
    uint32_t reserved_1b4;
    uint32_t reserved_1b8;
    uint32_t reserved_1bc;
    uint32_t reserved_1c0;
    uint32_t reserved_1c4;
    uint32_t reserved_1c8;
    uint32_t reserved_1cc;
    uint32_t reserved_1d0;
    uint32_t reserved_1d4;
    uint32_t reserved_1d8;
    uint32_t reserved_1dc;
    uint32_t reserved_1e0;
    uint32_t reserved_1e4;
    uint32_t reserved_1e8;
    uint32_t reserved_1ec;
    uint32_t reserved_1f0;
    uint32_t reserved_1f4;
    uint32_t reserved_1f8;
    uint32_t reserved_1fc;
    union {
        struct {
            uint32_t auto_wrback                   :    1;  /*Set this bit to enable automatic outlink-writeback when all the data pointed by outlink descriptor has been received.*/
            uint32_t eof_mode                      :    1;  /*EOF flag generation mode when receiving data. 1: EOF flag for Tx channel 0 is generated when data need to read has been popped from FIFO in DMA*/
            uint32_t outdscr_burst_en              :    1;  /*Set this bit to 1 to enable INCR burst transfer for Tx channel 0 reading link descriptor when accessing internal SRAM. */
            uint32_t ecc_aes_en                    :    1;  /*When access address space is ecc/aes area, this bit should be set to 1. In this case, the start address of square should be 16-bit aligned. The width of square multiply byte number of one pixel should be 16-bit aligned. */
            uint32_t check_owner                   :    1;  /*Set this bit to enable checking the owner attribute of the link descriptor.*/
            uint32_t loop_test                     :    1;  /*reserved*/
            uint32_t mem_burst_length              :    3;  /*Block size of Tx channel 2. 0: single      1: 16 bytes      2: 32 bytes    3: 64 bytes    4: 128 bytes*/
            uint32_t macro_block_size              :    2;  /*Sel TX macro-block size 0: 8pixel*8pixel     1: 8pixel*16pixel     2: 16pixel*16pixel     3: no macro-block     , only useful in mode 1 of the link descriptor*/
            uint32_t dscr_port_en                  :    1;  /*Set this bit to 1 to obtain descriptor from IP port*/
            uint32_t page_bound_en                 :    1;  /*Set this bit to 1 to make sure AXI read data don't cross the address boundary which define by mem_burst_length*/
            uint32_t reserved13                    :    11;  /*reserved*/
            uint32_t rst                           :    1;  /*Write 1 then write 0 to this bit to reset TX channel*/
            uint32_t cmd_disable                   :    1;  /*Write 1 before reset and write 0 after reset*/
            uint32_t arb_weight_opt_dis            :    1;  /*Set this bit to 1 to disable arbiter optimum weight function.*/
            uint32_t reserved27                    :    5;  /*reserved*/
        };
        uint32_t val;
    } out_conf0_ch2;
    union {
        struct {
            uint32_t done                          :    1;  /*The raw interrupt bit turns to high level when the last data pointed by one outlink descriptor has been transmitted to peripherals for Tx channel 0.*/
            uint32_t eof                           :    1;  /*The raw interrupt bit turns to high level when the last data pointed by one outlink descriptor has been read from memory for Tx channel 0. */
            uint32_t dscr_err                      :    1;  /*The raw interrupt bit turns to high level when detecting outlink descriptor error, including owner error, the second and third word error of outlink descriptor for Tx channel 0.*/
            uint32_t total_eof                     :    1;  /*The raw interrupt bit turns to high level when data corresponding a outlink (includes one link descriptor or few link descriptors) is transmitted out for Tx channel 0.*/
            uint32_t outfifo_ovf                   :    1;  /*The raw interrupt bit turns to high level when fifo is overflow. */
            uint32_t outfifo_udf                   :    1;  /*The raw interrupt bit turns to high level when fifo is underflow. */
            uint32_t outfifo_ro_ovf                :    1;  /*The raw interrupt bit turns to high level when reorder fifo is overflow. */
            uint32_t outfifo_ro_udf                :    1;  /*The raw interrupt bit turns to high level when reorder fifo is underflow. */
            uint32_t reserved8                     :    24;
        };
        uint32_t val;
    } out_int_raw_ch2;
    union {
        struct {
            uint32_t done                          :    1;  /*The interrupt enable bit for the OUT_DONE_CH_INT interrupt.*/
            uint32_t eof                           :    1;  /*The interrupt enable bit for the OUT_EOF_CH_INT interrupt.*/
            uint32_t dscr_err                      :    1;  /*The interrupt enable bit for the OUT_DSCR_ERR_CH_INT interrupt.*/
            uint32_t total_eof                     :    1;  /*The interrupt enable bit for the OUT_TOTAL_EOF_CH_INT interrupt.*/
            uint32_t outfifo_ovf                   :    1;  /*The interrupt enable bit for the OUTFIFO_OVF_CH_INT interrupt.*/
            uint32_t outfifo_udf                   :    1;  /*The interrupt enable bit for the OUTFIFO_UDF_CH_INT interrupt.*/
            uint32_t outfifo_ro_ovf                :    1;  /*The interrupt enable bit for the OUTFIFO_RO_OVF_CH_INT interrupt.*/
            uint32_t outfifo_ro_udf                :    1;  /*The interrupt enable bit for the OUTFIFO_RO_UDF_CH_INT interrupt.*/
            uint32_t reserved8                     :    24;
        };
        uint32_t val;
    } out_int_ena_ch2;
    union {
        struct {
            uint32_t done                          :    1;  /*The raw interrupt status bit for the OUT_DONE_CH_INT interrupt.*/
            uint32_t eof                           :    1;  /*The raw interrupt status bit for the OUT_EOF_CH_INT interrupt.*/
            uint32_t dscr_err                      :    1;  /*The raw interrupt status bit for the OUT_DSCR_ERR_CH_INT interrupt.*/
            uint32_t total_eof                     :    1;  /*The raw interrupt status bit for the OUT_TOTAL_EOF_CH_INT interrupt.*/
            uint32_t outfifo_ovf                   :    1;  /*The raw interrupt status bit for the OUTFIFO_OVF_CH_INT interrupt.*/
            uint32_t outfifo_udf                   :    1;  /*The raw interrupt status bit for the OUTFIFO_UDF_CH_INT interrupt.*/
            uint32_t outfifo_ro_ovf                :    1;  /*The raw interrupt status bit for the OUTFIFO_RO_OVF_CH_INT interrupt.*/
            uint32_t outfifo_ro_udf                :    1;  /*The raw interrupt status bit for the OUTFIFO_RO_UDF_CH_INT interrupt.*/
            uint32_t reserved8                     :    24;
        };
        uint32_t val;
    } out_int_st_ch2;
    union {
        struct {
            uint32_t done                          :    1;  /*Set this bit to clear the OUT_DONE_CH_INT interrupt.*/
            uint32_t eof                           :    1;  /*Set this bit to clear the OUT_EOF_CH_INT interrupt.*/
            uint32_t dscr_err                      :    1;  /*Set this bit to clear the OUT_DSCR_ERR_CH_INT interrupt.*/
            uint32_t total_eof                     :    1;  /*Set this bit to clear the OUT_TOTAL_EOF_CH_INT interrupt.*/
            uint32_t outfifo_ovf                   :    1;  /*Set this bit to clear the OUTFIFO_OVF_CH_INT interrupt.*/
            uint32_t outfifo_udf                   :    1;  /*Set this bit to clear the OUTFIFO_UDF_CH_INT interrupt.*/
            uint32_t outfifo_ro_ovf                :    1;  /*Set this bit to clear the OUTFIFO_RO_OVF_CH_INT interrupt.*/
            uint32_t outfifo_ro_udf                :    1;  /*Set this bit to clear the OUTFIFO_RO_UDF_CH_INT interrupt.*/
            uint32_t reserved8                     :    24;
        };
        uint32_t val;
    } out_int_clr_ch2;
    union {
        struct {
            uint32_t outfifo_full                  :    1;  /*Tx FIFO full signal for Tx channel 2.*/
            uint32_t outfifo_empty                 :    1;  /*Tx FIFO empty signal for Tx channel 2.*/
            uint32_t outfifo_cnt                   :    9;  /*The register stores the byte number of the data in Tx FIFO for Tx channel 2.*/
            uint32_t remaunder_1b                  :    1;  /*reserved*/
            uint32_t remaunder_2b                  :    1;  /*reserved*/
            uint32_t remaunder_3b                  :    1;  /*reserved*/
            uint32_t remaunder_4b                  :    1;  /*reserved*/
            uint32_t remaunder_5b                  :    1;  /*reserved*/
            uint32_t remaunder_6b                  :    1;  /*reserved*/
            uint32_t remaunder_7b                  :    1;  /*reserved*/
            uint32_t remaunder_8b                  :    1;  /*reserved*/
            uint32_t pixel_byte                    :    4;  /*the number of bytes contained in a pixel at TX channel     0: 1byte     1: 1.5bytes     2 : 2bytes     3: 2.5bytes     4: 3bytes     5: 4bytes*/
            uint32_t burst_block_num               :    4;  /*the number of macro blocks contained in a burst of data at TX channel*/
            uint32_t reserved27                    :    5;  /*reserved*/
        };
        uint32_t val;
    } outfifo_status_ch2;
    union {
        struct {
            uint32_t outfifo_wdata                 :    10;  /*This register stores the data that need to be pushed into DMA Tx FIFO.*/
            uint32_t outfifo_push                  :    1;  /*Set this bit to push data into DMA Tx FIFO.*/
            uint32_t reserved11                    :    21;  /*reserved*/
        };
        uint32_t val;
    } out_push_ch2;
    union {
        struct {
            uint32_t reserved0                     :    20;  /*reserved*/
            uint32_t outlink_stop                  :    1;  /*Set this bit to stop dealing with the outlink descriptors.*/
            uint32_t outlink_start                 :    1;  /*Set this bit to start dealing with the outlink descriptors.*/
            uint32_t outlink_restart               :    1;  /*Set this bit to restart a new outlink from the last address. */
            uint32_t outlink_park                  :    1;  /*1: the outlink descriptor's FSM is in idle state.  0: the outlink descriptor's FSM is working.*/
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } out_link_conf_ch2;
    uint32_t out_link_addr_ch2;
    union {
        struct {
            uint32_t outlink_dscr_addr             :    18;  /*This register stores the current outlink descriptor's address.*/
            uint32_t dscr_state                    :    2;  /*This register stores the current descriptor state machine state.*/
            uint32_t state                         :    4;  /*This register stores the current control module state machine state.*/
            uint32_t reset_avail                   :    1;  /*This register indicate that if the channel reset is safety.*/
            uint32_t reserved25                    :    7;  /*reserved*/
        };
        uint32_t val;
    } out_state_ch2;
    uint32_t out_eof_des_addr_ch2;
    uint32_t out_dscr_ch2;
    uint32_t out_dscr_bf0_ch2;
    uint32_t out_dscr_bf1_ch2;
    union {
        struct {
            uint32_t peri_sel                      :    3;  /*This register is used to select peripheral for Tx channel   0:  jpeg     1: display-1     2: display-2     3: display-3      7: no choose*/
            uint32_t reserved3                     :    29;
        };
        uint32_t val;
    } out_peri_sel_ch2;
    union {
        struct {
            uint32_t arb_token_num                 :    4;  /*Set the max number of token count of arbiter*/
            uint32_t arb_priority                  :    2;  /*Set the priority of channel*/
            uint32_t reserved6                     :    26;
        };
        uint32_t val;
    } out_arb_ch2;
    union {
        struct {
            uint32_t outfifo_ro_cnt                :    6;  /*The register stores the byte number of the data in color convert Tx FIFO for channel 2.*/
            uint32_t reserved6                     :    26;  /*reserved*/
        };
        uint32_t val;
    } out_ro_status_ch2;
    uint32_t reserved_244;
    union {
        struct {
            uint32_t color_output_sel              :    2;  /*Set final color convert process and output type    0: RGB888 to RGB565     1: YUV444 to YUV422     2: output directly*/
            uint32_t color_3b_proc_en              :    1;  /*Enable generic color convert modlue between color input & color output, need to configure parameter.*/
            uint32_t color_input_sel               :    3;  /*Set first color convert process and input color type    0: RGB565 to RGB888     1: YUV422 to YUV444      2: Other 2byte/pixel type    3: Other 3byte/pixel type    7: disable color space convert*/
            uint32_t reserved6                     :    26;  /*reserved*/
        };
        uint32_t val;
    } out_color_convert_ch2;
    union {
        struct {
            uint32_t scramble_sel_pre              :    3;  /*Scramble data before color convert :  0 : BYTE2-1-0   1 : BYTE2-0-1    2 : BYTE1-0-2    3 : BYTE1-2-0    4 : BYTE0-2-1    5 : BYTE0-1-2*/
            uint32_t reserved3                     :    29;  /*reserved*/
        };
        uint32_t val;
    } out_scramble_ch2;
    union {
        struct {
            uint32_t color_param_h0                :    21;  /*Set first 2 parameter of most significant byte of pending 3 bytes*/
            uint32_t reserved21                    :    11;  /*reserved*/
        };
        uint32_t val;
    } out_color_param0_ch2;
    union {
        struct {
            uint32_t color_param_h1                :    28;  /*Set last 2 parameter of most significant byte of pending 3 bytes*/
            uint32_t reserved28                    :    4;  /*reserved*/
        };
        uint32_t val;
    } out_color_param1_ch2;
    union {
        struct {
            uint32_t color_param_m0                :    21;  /*Set first 2 parameter of midium significant byte of pending 3 bytes*/
            uint32_t reserved21                    :    11;  /*reserved*/
        };
        uint32_t val;
    } out_color_param2_ch2;
    union {
        struct {
            uint32_t color_param_m1                :    28;  /*Set last 2 parameter of midium significant byte of pending 3 bytes*/
            uint32_t reserved28                    :    4;  /*reserved*/
        };
        uint32_t val;
    } out_color_param3_ch2;
    union {
        struct {
            uint32_t color_param_l0                :    21;  /*Set first 2 parameter of least significant byte of pending 3 bytes*/
            uint32_t reserved21                    :    11;  /*reserved*/
        };
        uint32_t val;
    } out_color_param4_ch2;
    union {
        struct {
            uint32_t color_param_l1                :    28;  /*Set last 2 parameter of least significant byte of pending 3 bytes*/
            uint32_t reserved28                    :    4;  /*reserved*/
        };
        uint32_t val;
    } out_color_param5_ch2;
    uint32_t reserved_268;
    uint32_t reserved_26c;
    uint32_t reserved_270;
    uint32_t reserved_274;
    uint32_t reserved_278;
    uint32_t reserved_27c;
    uint32_t reserved_280;
    uint32_t reserved_284;
    uint32_t reserved_288;
    uint32_t reserved_28c;
    uint32_t reserved_290;
    uint32_t reserved_294;
    uint32_t reserved_298;
    uint32_t reserved_29c;
    uint32_t reserved_2a0;
    uint32_t reserved_2a4;
    uint32_t reserved_2a8;
    uint32_t reserved_2ac;
    uint32_t reserved_2b0;
    uint32_t reserved_2b4;
    uint32_t reserved_2b8;
    uint32_t reserved_2bc;
    uint32_t reserved_2c0;
    uint32_t reserved_2c4;
    uint32_t reserved_2c8;
    uint32_t reserved_2cc;
    uint32_t reserved_2d0;
    uint32_t reserved_2d4;
    uint32_t reserved_2d8;
    uint32_t reserved_2dc;
    uint32_t reserved_2e0;
    uint32_t reserved_2e4;
    uint32_t reserved_2e8;
    uint32_t reserved_2ec;
    uint32_t reserved_2f0;
    uint32_t reserved_2f4;
    uint32_t reserved_2f8;
    uint32_t reserved_2fc;
    uint32_t reserved_300;
    uint32_t reserved_304;
    uint32_t reserved_308;
    uint32_t reserved_30c;
    uint32_t reserved_310;
    uint32_t reserved_314;
    uint32_t reserved_318;
    uint32_t reserved_31c;
    uint32_t reserved_320;
    uint32_t reserved_324;
    uint32_t reserved_328;
    uint32_t reserved_32c;
    uint32_t reserved_330;
    uint32_t reserved_334;
    uint32_t reserved_338;
    uint32_t reserved_33c;
    uint32_t reserved_340;
    uint32_t reserved_344;
    uint32_t reserved_348;
    uint32_t reserved_34c;
    uint32_t reserved_350;
    uint32_t reserved_354;
    uint32_t reserved_358;
    uint32_t reserved_35c;
    uint32_t reserved_360;
    uint32_t reserved_364;
    uint32_t reserved_368;
    uint32_t reserved_36c;
    uint32_t reserved_370;
    uint32_t reserved_374;
    uint32_t reserved_378;
    uint32_t reserved_37c;
    uint32_t reserved_380;
    uint32_t reserved_384;
    uint32_t reserved_388;
    uint32_t reserved_38c;
    uint32_t reserved_390;
    uint32_t reserved_394;
    uint32_t reserved_398;
    uint32_t reserved_39c;
    uint32_t reserved_3a0;
    uint32_t reserved_3a4;
    uint32_t reserved_3a8;
    uint32_t reserved_3ac;
    uint32_t reserved_3b0;
    uint32_t reserved_3b4;
    uint32_t reserved_3b8;
    uint32_t reserved_3bc;
    uint32_t reserved_3c0;
    uint32_t reserved_3c4;
    uint32_t reserved_3c8;
    uint32_t reserved_3cc;
    uint32_t reserved_3d0;
    uint32_t reserved_3d4;
    uint32_t reserved_3d8;
    uint32_t reserved_3dc;
    uint32_t reserved_3e0;
    uint32_t reserved_3e4;
    uint32_t reserved_3e8;
    uint32_t reserved_3ec;
    uint32_t reserved_3f0;
    uint32_t reserved_3f4;
    uint32_t reserved_3f8;
    uint32_t reserved_3fc;
    uint32_t reserved_400;
    uint32_t reserved_404;
    uint32_t reserved_408;
    uint32_t reserved_40c;
    uint32_t reserved_410;
    uint32_t reserved_414;
    uint32_t reserved_418;
    uint32_t reserved_41c;
    uint32_t reserved_420;
    uint32_t reserved_424;
    uint32_t reserved_428;
    uint32_t reserved_42c;
    uint32_t reserved_430;
    uint32_t reserved_434;
    uint32_t reserved_438;
    uint32_t reserved_43c;
    uint32_t reserved_440;
    uint32_t reserved_444;
    uint32_t reserved_448;
    uint32_t reserved_44c;
    uint32_t reserved_450;
    uint32_t reserved_454;
    uint32_t reserved_458;
    uint32_t reserved_45c;
    uint32_t reserved_460;
    uint32_t reserved_464;
    uint32_t reserved_468;
    uint32_t reserved_46c;
    uint32_t reserved_470;
    uint32_t reserved_474;
    uint32_t reserved_478;
    uint32_t reserved_47c;
    uint32_t reserved_480;
    uint32_t reserved_484;
    uint32_t reserved_488;
    uint32_t reserved_48c;
    uint32_t reserved_490;
    uint32_t reserved_494;
    uint32_t reserved_498;
    uint32_t reserved_49c;
    uint32_t reserved_4a0;
    uint32_t reserved_4a4;
    uint32_t reserved_4a8;
    uint32_t reserved_4ac;
    uint32_t reserved_4b0;
    uint32_t reserved_4b4;
    uint32_t reserved_4b8;
    uint32_t reserved_4bc;
    uint32_t reserved_4c0;
    uint32_t reserved_4c4;
    uint32_t reserved_4c8;
    uint32_t reserved_4cc;
    uint32_t reserved_4d0;
    uint32_t reserved_4d4;
    uint32_t reserved_4d8;
    uint32_t reserved_4dc;
    uint32_t reserved_4e0;
    uint32_t reserved_4e4;
    uint32_t reserved_4e8;
    uint32_t reserved_4ec;
    uint32_t reserved_4f0;
    uint32_t reserved_4f4;
    uint32_t reserved_4f8;
    uint32_t reserved_4fc;
    union {
        struct {
            uint32_t mem_trans_en                  :    1;  /*enable memory trans of the same channel*/
            uint32_t reserved1                     :    1;  /*reserved*/
            uint32_t indscr_burst_en               :    1;  /*Set this bit to 1 to enable INCR burst transfer for Rx transmitting link descriptor when accessing SRAM. */
            uint32_t ecc_aes_en                    :    1;  /*When access address space is ecc/aes area, this bit should be set to 1. In this case, the start address of square should be 16-bit aligned. The width of square multiply byte number of one pixel should be 16-bit aligned. */
            uint32_t check_owner                   :    1;  /*Set this bit to enable checking the owner attribute of the link descriptor.*/
            uint32_t loop_test                     :    1;  /*reserved*/
            uint32_t mem_burst_length              :    3;  /*Block size of Rx channel 0. 0: single      1: 16 bytes      2: 32 bytes    3: 64 bytes    4: 128 bytes*/
            uint32_t macro_block_size              :    2;  /*Sel RX macro-block size 0: 8pixel*8pixel     1: 8pixel*16pixel     2: 16pixel*16pixel     3: no macro-block     , only useful in mode 1 of the link descriptor*/
            uint32_t dscr_port_en                  :    1;  /*Set this bit to 1 to obtain descriptor from IP port*/
            uint32_t page_bound_en                 :    1;  /*Set this bit to 1 to make sure AXI write data don't cross the address boundary which define by mem_burst_length*/
            uint32_t reserved13                    :    3;  /*reserved*/
            uint32_t reorder_en                    :    1;  /*Enable RX channel 0 macro block reorder when set to 1, only channel0 have this selection*/
            uint32_t reserved17                    :    7;  /*reserved*/
            uint32_t rst                           :    1;  /*Write 1 then write 0 to this bit to reset Rx channel*/
            uint32_t cmd_disable                   :    1;  /*Write 1 before reset and write 0 after reset*/
            uint32_t arb_weight_opt_dis            :    1;  /*Set this bit to 1 to disable arbiter optimum weight function.*/
            uint32_t reserved27                    :    5;  /*reserved*/
        };
        uint32_t val;
    } in_conf0_ch0;
    union {
        struct {
            uint32_t done                          :    1;  /*The raw interrupt bit turns to high level when the last data pointed by one inlink descriptor has been transmitted to peripherals for Rx channel 0.*/
            uint32_t suc_eof                       :    1;  /*The raw interrupt bit turns to high level when the last data pointed by one inlink descriptor has been received and no data error is detected for Rx channel 0.*/
            uint32_t err_eof                       :    1;  /*The raw interrupt bit turns to high level when the last data pointed by one inlink descriptor has been received and data error is detected */
            uint32_t dscr_err                      :    1;  /*The raw interrupt bit turns to high level when detecting inlink descriptor error, including owner error, the second and third word error of inlink descriptor for Rx channel 0.*/
            uint32_t infifo_ovf                    :    1;  /*The raw interrupt bit turns to high level when fifo of Rx channel is overflow. */
            uint32_t infifo_udf                    :    1;  /*The raw interrupt bit turns to high level when fifo of Rx channel is underflow. */
            uint32_t dscr_empty                    :    1;  /*The raw interrupt bit turns to high level when the last descriptor is done but fifo also remain data.*/
            uint32_t infifo_ro_ovf                 :    1;  /*The raw interrupt bit turns to high level when reorder fifo is overflow. */
            uint32_t infifo_ro_udf                 :    1;  /*The raw interrupt bit turns to high level when reorder fifo is underflow. */
            uint32_t reserved9                     :    23;
        };
        uint32_t val;
    } in_int_raw_ch0;
    union {
        struct {
            uint32_t done                          :    1;  /*The interrupt enable bit for the IN_DONE_CH_INT interrupt.*/
            uint32_t suc_eof                       :    1;  /*The interrupt enable bit for the IN_SUC_EOF_CH_INT interrupt.*/
            uint32_t err_eof                       :    1;  /*The interrupt enable bit for the IN_ERR_EOF_CH_INT interrupt.*/
            uint32_t dscr_err                      :    1;  /*The interrupt enable bit for the IN_DSCR_ERR_CH_INT interrupt.*/
            uint32_t infifo_ovf                    :    1;  /*The interrupt enable bit for the INFIFO_OVF_CH_INT interrupt.*/
            uint32_t infifo_udf                    :    1;  /*The interrupt enable bit for the INFIFO_UDF_CH_INT interrupt.*/
            uint32_t dscr_empty                    :    1;  /*The interrupt enable bit for the IN_DSCR_EMPTY_CH_INT interrupt.*/
            uint32_t infifo_ro_ovf                 :    1;  /*The interrupt enable bit for the INFIFO_RO_OVF_CH_INT interrupt.*/
            uint32_t infifo_ro_udf                 :    1;  /*The interrupt enable bit for the INFIFO_RO_UDF_CH_INT interrupt.*/
            uint32_t reserved9                     :    23;
        };
        uint32_t val;
    } in_int_ena_ch0;
    union {
        struct {
            uint32_t done                          :    1;  /*The raw interrupt status bit for the IN_DONE_CH_INT interrupt.*/
            uint32_t suc_eof                       :    1;  /*The raw interrupt status bit for the IN_SUC_EOF_CH_INT interrupt.*/
            uint32_t err_eof                       :    1;  /*The raw interrupt status bit for the IN_ERR_EOF_CH_INT interrupt.*/
            uint32_t dscr_err                      :    1;  /*The raw interrupt status bit for the IN_DSCR_ERR_CH_INT interrupt.*/
            uint32_t infifo_ovf                    :    1;  /*The raw interrupt status bit for the INFIFO_OVF_CH_INT interrupt.*/
            uint32_t infifo_udf                    :    1;  /*The raw interrupt status bit for the INFIFO_UDF_CH_INT interrupt.*/
            uint32_t dscr_empty                    :    1;  /*The raw interrupt status bit for the IN_DSCR_EMPTY_CH_INT interrupt.*/
            uint32_t infifo_ro_ovf                 :    1;  /*The raw interrupt status bit for the INFIFO_RO_OVF_CH_INT interrupt.*/
            uint32_t infifo_ro_udf                 :    1;  /*The raw interrupt status bit for the INFIFO_RO_UDF_CH_INT interrupt.*/
            uint32_t reserved9                     :    23;
        };
        uint32_t val;
    } in_int_st_ch0;
    union {
        struct {
            uint32_t done                          :    1;  /*Set this bit to clear the IN_DONE_CH_INT interrupt.*/
            uint32_t suc_eof                       :    1;  /*Set this bit to clear the IN_SUC_EOF_CH_INT interrupt.*/
            uint32_t err_eof                       :    1;  /*Set this bit to clear the IN_ERR_EOF_CH_INT interrupt.*/
            uint32_t dscr_err                      :    1;  /*Set this bit to clear the INDSCR_ERR_CH_INT interrupt.*/
            uint32_t infifo_ovf                    :    1;  /*Set this bit to clear the INFIFO_OVF_CH_INT interrupt.*/
            uint32_t infifo_udf                    :    1;  /*Set this bit to clear the INFIFO_UDF_CH_INT interrupt.*/
            uint32_t dscr_empty                    :    1;  /*Set this bit to clear the IN_DSCR_EMPTY_CH_INT interrupt.*/
            uint32_t infifo_ro_ovf                 :    1;  /*Set this bit to clear the INFIFO_RO_OVF_CH_INT interrupt.*/
            uint32_t infifo_ro_udf                 :    1;  /*Set this bit to clear the INFIFO_RO_UDF_CH_INT interrupt.*/
            uint32_t reserved9                     :    23;
        };
        uint32_t val;
    } in_int_clr_ch0;
    union {
        struct {
            uint32_t infifo_full                   :    1;  /*Rx FIFO full signal for Rx channel.*/
            uint32_t infifo_empty                  :    1;  /*Rx FIFO empty signal for Rx channel.*/
            uint32_t infifo_cnt                    :    9;  /*The register stores the byte number of the data in Rx FIFO for Rx channel.*/
            uint32_t remaunder_1b                  :    1;  /*reserved*/
            uint32_t remaunder_2b                  :    1;  /*reserved*/
            uint32_t remaunder_3b                  :    1;  /*reserved*/
            uint32_t remaunder_4b                  :    1;  /*reserved*/
            uint32_t remaunder_5b                  :    1;  /*reserved*/
            uint32_t remaunder_6b                  :    1;  /*reserved*/
            uint32_t remaunder_7b                  :    1;  /*reserved*/
            uint32_t remaunder_8b                  :    1;  /*reserved*/
            uint32_t pixel_byte                    :    4;  /*the number of bytes contained in a pixel at RX channel*/
            uint32_t burst_block_num               :    4;  /*the number of macro blocks contained in a burst of data at RX channel*/
            uint32_t reserved27                    :    5;  /*reserved*/
        };
        uint32_t val;
    } infifo_status_ch0;
    union {
        struct {
            uint32_t infifo_rdata                  :    11;  /*This register stores the data popping from DMA Rx FIFO.*/
            uint32_t infifo_pop                    :    1;  /*Set this bit to pop data from DMA Rx FIFO.*/
            uint32_t reserved12                    :    20;  /*reserved*/
        };
        uint32_t val;
    } in_pop_ch0;
    union {
        struct {
            uint32_t reserved0                     :    20;
            uint32_t inlink_auto_ret               :    1;  /*Set this bit to return to current inlink descriptor's address, when there are some errors in current receiving data.*/
            uint32_t inlink_stop                   :    1;  /*Set this bit to stop dealing with the inlink descriptors.*/
            uint32_t inlink_start                  :    1;  /*Set this bit to start dealing with the inlink descriptors.*/
            uint32_t inlink_restart                :    1;  /*Set this bit to mount a new inlink descriptor.*/
            uint32_t inlink_park                   :    1;  /*1: the inlink descriptor's FSM is in idle state.  0: the inlink descriptor's FSM is working.*/
            uint32_t reserved25                    :    7;
        };
        uint32_t val;
    } in_link_conf_ch0;
    uint32_t in_link_addr_ch0;
    union {
        struct {
            uint32_t inlink_dscr_addr              :    18;  /*This register stores the current inlink descriptor's address.*/
            uint32_t dscr_state                    :    2;  /*reserved*/
            uint32_t state                         :    3;  /*reserved*/
            uint32_t reset_avail                   :    1;  /*This register indicate that if the channel reset is safety.*/
            uint32_t reserved24                    :    8;  /*reserved*/
        };
        uint32_t val;
    } in_state_ch0;
    uint32_t in_suc_eof_des_addr_ch0;
    uint32_t in_err_eof_des_addr_ch0;
    uint32_t in_dscr_ch0;
    uint32_t in_dscr_bf0_ch0;
    uint32_t in_dscr_bf1_ch0;
    union {
        struct {
            uint32_t peri_sel                      :    3;  /*This register is used to select peripheral for Rx channel   0:  jpeg     1: display-1     2: display-2        7: no choose*/
            uint32_t reserved3                     :    29;
        };
        uint32_t val;
    } in_peri_sel_ch0;
    union {
        struct {
            uint32_t arb_token_num                 :    4;  /*Set the max number of token count of arbiter*/
            uint32_t arb_priority                  :    1;  /*Set the priority of channel*/
            uint32_t reserved5                     :    27;
        };
        uint32_t val;
    } in_arb_ch0;
    union {
        struct {
            uint32_t infifo_ro_cnt                 :    5;  /*The register stores the byte number of the data in color convert Rx FIFO for channel 0.*/
            uint32_t ro_wr_state                   :    2;  /*The register stores the state of read ram of reorder*/
            uint32_t ro_rd_state                   :    2;  /*The register stores the state of write ram of reorder*/
            uint32_t reserved9                     :    23;  /*reserved*/
        };
        uint32_t val;
    } in_ro_status_ch0;
    union {
        struct {
            uint32_t reserved0                     :    4;  /*reserved*/
            uint32_t ro_ram_force_pd               :    1;  /*dma reorder ram power down*/
            uint32_t ro_ram_force_pu               :    1;  /*dma reorder ram power up*/
            uint32_t ro_ram_clk_fo                 :    1;  /*1: Force to open the clock and bypass the gate-clock when accessing the RAM in DMA. 0: A gate-clock will be used when accessing the RAM in DMA.*/
            uint32_t reserved7                     :    25;
        };
        uint32_t val;
    } in_ro_pd_conf_ch0;
    union {
        struct {
            uint32_t color_output_sel              :    2;  /*Set final color convert process and output type    0: RGB888 to RGB565     1: output directly*/
            uint32_t color_3b_proc_en              :    1;  /*Enable generic color convert modlue between color input & color output, need to configure parameter.*/
            uint32_t color_input_sel               :    3;  /*Set first color convert process and input color type    0: YUV422/420 to YUV444     1: YUV422      2: YUV444/420      7: disable color space convert*/
            uint32_t reserved6                     :    26;  /*reserved*/
        };
        uint32_t val;
    } in_color_convert_ch0;
    union {
        struct {
            uint32_t scramble_sel_pre              :    3;  /*Scramble data before color convert :  0 : BYTE2-1-0   1 : BYTE2-0-1    2 : BYTE1-0-2    3 : BYTE1-2-0    4 : BYTE0-2-1    5 : BYTE0-1-2*/
            uint32_t scramble_sel_post             :    3;  /*Scramble data after color convert :  0 : BYTE2-1-0   1 : BYTE2-0-1    2 : BYTE1-0-2    3 : BYTE1-2-0    4 : BYTE0-2-1    5 : BYTE0-1-2*/
            uint32_t reserved6                     :    26;  /*reserved*/
        };
        uint32_t val;
    } in_scramble_ch0;
    union {
        struct {
            uint32_t color_param_h0                :    21;  /*Set first 2 parameter of most significant byte of pending 3 bytes*/
            uint32_t reserved21                    :    11;  /*reserved*/
        };
        uint32_t val;
    } in_color_param0_ch0;
    union {
        struct {
            uint32_t color_param_h1                :    28;  /*Set last 2 parameter of most significant byte of pending 3 bytes*/
            uint32_t reserved28                    :    4;  /*reserved*/
        };
        uint32_t val;
    } in_color_param1_ch0;
    union {
        struct {
            uint32_t color_param_m0                :    21;  /*Set first 2 parameter of midium significant byte of pending 3 bytes*/
            uint32_t reserved21                    :    11;  /*reserved*/
        };
        uint32_t val;
    } in_color_param2_ch0;
    union {
        struct {
            uint32_t color_param_m1                :    28;  /*Set last 2 parameter of midium significant byte of pending 3 bytes*/
            uint32_t reserved28                    :    4;  /*reserved*/
        };
        uint32_t val;
    } in_color_param3_ch0;
    union {
        struct {
            uint32_t color_param_l0                :    21;  /*Set first 2 parameter of least significant byte of pending 3 bytes*/
            uint32_t reserved21                    :    11;  /*reserved*/
        };
        uint32_t val;
    } in_color_param4_ch0;
    union {
        struct {
            uint32_t color_param_l1                :    28;  /*Set last 2 parameter of least significant byte of pending 3 bytes*/
            uint32_t reserved28                    :    4;  /*reserved*/
        };
        uint32_t val;
    } in_color_param5_ch0;
    uint32_t reserved_56c;
    uint32_t reserved_570;
    uint32_t reserved_574;
    uint32_t reserved_578;
    uint32_t reserved_57c;
    uint32_t reserved_580;
    uint32_t reserved_584;
    uint32_t reserved_588;
    uint32_t reserved_58c;
    uint32_t reserved_590;
    uint32_t reserved_594;
    uint32_t reserved_598;
    uint32_t reserved_59c;
    uint32_t reserved_5a0;
    uint32_t reserved_5a4;
    uint32_t reserved_5a8;
    uint32_t reserved_5ac;
    uint32_t reserved_5b0;
    uint32_t reserved_5b4;
    uint32_t reserved_5b8;
    uint32_t reserved_5bc;
    uint32_t reserved_5c0;
    uint32_t reserved_5c4;
    uint32_t reserved_5c8;
    uint32_t reserved_5cc;
    uint32_t reserved_5d0;
    uint32_t reserved_5d4;
    uint32_t reserved_5d8;
    uint32_t reserved_5dc;
    uint32_t reserved_5e0;
    uint32_t reserved_5e4;
    uint32_t reserved_5e8;
    uint32_t reserved_5ec;
    uint32_t reserved_5f0;
    uint32_t reserved_5f4;
    uint32_t reserved_5f8;
    uint32_t reserved_5fc;
    union {
        struct {
            uint32_t mem_trans_en                  :    1;  /*enable memory trans of the same channel*/
            uint32_t reserved1                     :    1;  /*reserved*/
            uint32_t indscr_burst_en               :    1;  /*Set this bit to 1 to enable INCR burst transfer for Rx transmitting link descriptor when accessing SRAM. */
            uint32_t ecc_aes_en                    :    1;  /*When access address space is ecc/aes area, this bit should be set to 1. In this case, the start address of square should be 16-bit aligned. The width of square multiply byte number of one pixel should be 16-bit aligned. */
            uint32_t check_owner                   :    1;  /*Set this bit to enable checking the owner attribute of the link descriptor.*/
            uint32_t loop_test                     :    1;  /*reserved*/
            uint32_t mem_burst_length              :    3;  /*Block size of Rx channel 1. 0: single      1: 16 bytes      2: 32 bytes    3: 64 bytes    4: 128 bytes*/
            uint32_t macro_block_size              :    2;  /*Sel RX macro-block size 0: 8pixel*8pixel     1: 8pixel*16pixel     2: 16pixel*16pixel     3: no macro-block     , only useful in mode 1 of the link descriptor*/
            uint32_t dscr_port_en                  :    1;  /*Set this bit to 1 to obtain descriptor from IP port*/
            uint32_t page_bound_en                 :    1;  /*Set this bit to 1 to make sure AXI write data don't cross the address boundary which define by mem_burst_length*/
            uint32_t reserved13                    :    11;  /*reserved*/
            uint32_t rst                           :    1;  /*Write 1 then write 0 to this bit to reset Rx channel*/
            uint32_t cmd_disable                   :    1;  /*Write 1 before reset and write 0 after reset*/
            uint32_t arb_weight_opt_dis            :    1;  /*Set this bit to 1 to disable arbiter optimum weight function.*/
            uint32_t reserved27                    :    5;  /*reserved*/
        };
        uint32_t val;
    } in_conf0_ch1;
    union {
        struct {
            uint32_t done                          :    1;  /*The raw interrupt bit turns to high level when the last data pointed by one inlink descriptor has been transmitted to peripherals for Rx channel 1.*/
            uint32_t suc_eof                       :    1;  /*The raw interrupt bit turns to high level when the last data pointed by one inlink descriptor has been received and no data error is detected for Rx channel 1.*/
            uint32_t err_eof                       :    1;  /*The raw interrupt bit turns to high level when the last data pointed by one inlink descriptor has been received and data error is detected */
            uint32_t dscr_err                      :    1;  /*The raw interrupt bit turns to high level when detecting inlink descriptor error, including owner error, the second and third word error of inlink descriptor for Rx channel 1.*/
            uint32_t infifo_ovf                    :    1;  /*This raw interrupt bit turns to high level when fifo of Rx channel is overflow. */
            uint32_t infifo_udf                    :    1;  /*This raw interrupt bit turns to high level when fifo of Rx channel is underflow. */
            uint32_t dscr_empty                    :    1;  /*The raw interrupt bit turns to high level when the last descriptor is done but fifo also remain data.*/
            uint32_t reserved7                     :    25;
        };
        uint32_t val;
    } in_int_raw_ch1;
    union {
        struct {
            uint32_t done                          :    1;  /*The interrupt enable bit for the IN_DONE_CH_INT interrupt.*/
            uint32_t suc_eof                       :    1;  /*The interrupt enable bit for the IN_SUC_EOF_CH_INT interrupt.*/
            uint32_t err_eof                       :    1;  /*The interrupt enable bit for the IN_ERR_EOF_CH_INT interrupt.*/
            uint32_t dscr_err                      :    1;  /*The interrupt enable bit for the IN_DSCR_ERR_CH_INT interrupt.*/
            uint32_t infifo_ovf                    :    1;  /*The interrupt enable bit for the INFIFO_OVF_CH_INT interrupt.*/
            uint32_t infifo_udf                    :    1;  /*The interrupt enable bit for the INFIFO_UDF_CH_INT interrupt.*/
            uint32_t dscr_empty                    :    1;  /*The interrupt enable bit for the IN_DSCR_EMPTY_CH_INT interrupt.*/
            uint32_t reserved7                     :    25;
        };
        uint32_t val;
    } in_int_ena_ch1;
    union {
        struct {
            uint32_t done                          :    1;  /*The raw interrupt status bit for the IN_DONE_CH_INT interrupt.*/
            uint32_t suc_eof                       :    1;  /*The raw interrupt status bit for the IN_SUC_EOF_CH_INT interrupt.*/
            uint32_t err_eof                       :    1;  /*The raw interrupt status bit for the IN_ERR_EOF_CH_INT interrupt.*/
            uint32_t dscr_err                      :    1;  /*The raw interrupt status bit for the IN_DSCR_ERR_CH_INT interrupt.*/
            uint32_t infifo_ovf                    :    1;  /*The raw interrupt status bit for the INFIFO_OVF_CH_INT interrupt.*/
            uint32_t infifo_udf                    :    1;  /*The raw interrupt status bit for the INFIFO_UDF_CH_INT interrupt.*/
            uint32_t dscr_empty                    :    1;  /*The raw interrupt status bit for the IN_DSCR_EMPTY_CH_INT interrupt.*/
            uint32_t reserved7                     :    25;
        };
        uint32_t val;
    } in_int_st_ch1;
    union {
        struct {
            uint32_t done                          :    1;  /*Set this bit to clear the IN_DONE_CH_INT interrupt.*/
            uint32_t suc_eof                       :    1;  /*Set this bit to clear the IN_SUC_EOF_CH_INT interrupt.*/
            uint32_t err_eof                       :    1;  /*Set this bit to clear the IN_ERR_EOF_CH_INT interrupt.*/
            uint32_t dscr_err                      :    1;  /*Set this bit to clear the INDSCR_ERR_CH_INT interrupt.*/
            uint32_t infifo_ovf                    :    1;  /*Set this bit to clear the INFIFO_OVF_CH_INT interrupt.*/
            uint32_t infifo_udf                    :    1;  /*Set this bit to clear the INFIFO_UDF_CH_INT interrupt.*/
            uint32_t dscr_empty                    :    1;  /*Set this bit to clear the IN_DSCR_EMPTY_CH_INT interrupt.*/
            uint32_t reserved7                     :    25;
        };
        uint32_t val;
    } in_int_clr_ch1;
    union {
        struct {
            uint32_t infifo_full                   :    1;  /*Rx FIFO full signal for Rx channel.*/
            uint32_t infifo_empty                  :    1;  /*Rx FIFO empty signal for Rx channel.*/
            uint32_t infifo_cnt                    :    9;  /*The register stores the byte number of the data in Rx FIFO for Rx channel.*/
            uint32_t remaunder_1b                  :    1;  /*reserved*/
            uint32_t remaunder_2b                  :    1;  /*reserved*/
            uint32_t remaunder_3b                  :    1;  /*reserved*/
            uint32_t remaunder_4b                  :    1;  /*reserved*/
            uint32_t remaunder_5b                  :    1;  /*reserved*/
            uint32_t remaunder_6b                  :    1;  /*reserved*/
            uint32_t remaunder_7b                  :    1;  /*reserved*/
            uint32_t remaunder_8b                  :    1;  /*reserved*/
            uint32_t pixel_byte                    :    4;  /*the number of bytes contained in a pixel at RX channel*/
            uint32_t burst_block_num               :    4;  /*the number of macro blocks contained in a burst of data at RX channel*/
            uint32_t reserved27                    :    5;  /*reserved*/
        };
        uint32_t val;
    } infifo_status_ch1;
    union {
        struct {
            uint32_t infifo_rdata                  :    11;  /*This register stores the data popping from DMA Rx FIFO.*/
            uint32_t infifo_pop                    :    1;  /*Set this bit to pop data from DMA Rx FIFO.*/
            uint32_t reserved12                    :    20;  /*reserved*/
        };
        uint32_t val;
    } in_pop_ch1;
    union {
        struct {
            uint32_t reserved0                     :    20;
            uint32_t inlink_auto_ret               :    1;  /*Set this bit to return to current inlink descriptor's address, when there are some errors in current receiving data.*/
            uint32_t inlink_stop                   :    1;  /*Set this bit to stop dealing with the inlink descriptors.*/
            uint32_t inlink_start                  :    1;  /*Set this bit to start dealing with the inlink descriptors.*/
            uint32_t inlink_restart                :    1;  /*Set this bit to mount a new inlink descriptor.*/
            uint32_t inlink_park                   :    1;  /*1: the inlink descriptor's FSM is in idle state.  0: the inlink descriptor's FSM is working.*/
            uint32_t reserved25                    :    7;
        };
        uint32_t val;
    } in_link_conf_ch1;
    uint32_t in_link_addr_ch1;
    union {
        struct {
            uint32_t inlink_dscr_addr              :    18;  /*This register stores the current inlink descriptor's address.*/
            uint32_t dscr_state                    :    2;  /*reserved*/
            uint32_t state                         :    3;  /*reserved*/
            uint32_t reset_avail                   :    1;  /*This register indicate that if the channel reset is safety.*/
            uint32_t reserved24                    :    8;  /*reserved*/
        };
        uint32_t val;
    } in_state_ch1;
    uint32_t in_suc_eof_des_addr_ch1;
    uint32_t in_err_eof_des_addr_ch1;
    uint32_t in_dscr_ch1;
    uint32_t in_dscr_bf0_ch1;
    uint32_t in_dscr_bf1_ch1;
    union {
        struct {
            uint32_t peri_sel                      :    3;  /*This register is used to select peripheral for Rx channel   0:  jpeg     1: display-1     2: display-2        7: no choose*/
            uint32_t reserved3                     :    29;
        };
        uint32_t val;
    } in_peri_sel_ch1;
    union {
        struct {
            uint32_t arb_token_num                 :    4;  /*Set the max number of token count of arbiter*/
            uint32_t arb_priority                  :    1;  /*Set the priority of channel*/
            uint32_t reserved5                     :    27;
        };
        uint32_t val;
    } in_arb_ch1;
    uint32_t reserved_644;
    uint32_t reserved_648;
    uint32_t reserved_64c;
    uint32_t reserved_650;
    uint32_t reserved_654;
    uint32_t reserved_658;
    uint32_t reserved_65c;
    uint32_t reserved_660;
    uint32_t reserved_664;
    uint32_t reserved_668;
    uint32_t reserved_66c;
    uint32_t reserved_670;
    uint32_t reserved_674;
    uint32_t reserved_678;
    uint32_t reserved_67c;
    uint32_t reserved_680;
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
    uint32_t reserved_6f8;
    uint32_t reserved_6fc;
    uint32_t reserved_700;
    uint32_t reserved_704;
    uint32_t reserved_708;
    uint32_t reserved_70c;
    uint32_t reserved_710;
    uint32_t reserved_714;
    uint32_t reserved_718;
    uint32_t reserved_71c;
    uint32_t reserved_720;
    uint32_t reserved_724;
    uint32_t reserved_728;
    uint32_t reserved_72c;
    uint32_t reserved_730;
    uint32_t reserved_734;
    uint32_t reserved_738;
    uint32_t reserved_73c;
    uint32_t reserved_740;
    uint32_t reserved_744;
    uint32_t reserved_748;
    uint32_t reserved_74c;
    uint32_t reserved_750;
    uint32_t reserved_754;
    uint32_t reserved_758;
    uint32_t reserved_75c;
    uint32_t reserved_760;
    uint32_t reserved_764;
    uint32_t reserved_768;
    uint32_t reserved_76c;
    uint32_t reserved_770;
    uint32_t reserved_774;
    uint32_t reserved_778;
    uint32_t reserved_77c;
    uint32_t reserved_780;
    uint32_t reserved_784;
    uint32_t reserved_788;
    uint32_t reserved_78c;
    uint32_t reserved_790;
    uint32_t reserved_794;
    uint32_t reserved_798;
    uint32_t reserved_79c;
    uint32_t reserved_7a0;
    uint32_t reserved_7a4;
    uint32_t reserved_7a8;
    uint32_t reserved_7ac;
    uint32_t reserved_7b0;
    uint32_t reserved_7b4;
    uint32_t reserved_7b8;
    uint32_t reserved_7bc;
    uint32_t reserved_7c0;
    uint32_t reserved_7c4;
    uint32_t reserved_7c8;
    uint32_t reserved_7cc;
    uint32_t reserved_7d0;
    uint32_t reserved_7d4;
    uint32_t reserved_7d8;
    uint32_t reserved_7dc;
    uint32_t reserved_7e0;
    uint32_t reserved_7e4;
    uint32_t reserved_7e8;
    uint32_t reserved_7ec;
    uint32_t reserved_7f0;
    uint32_t reserved_7f4;
    uint32_t reserved_7f8;
    uint32_t reserved_7fc;
    uint32_t reserved_800;
    uint32_t reserved_804;
    uint32_t reserved_808;
    uint32_t reserved_80c;
    uint32_t reserved_810;
    uint32_t reserved_814;
    uint32_t reserved_818;
    uint32_t reserved_81c;
    uint32_t reserved_820;
    uint32_t reserved_824;
    uint32_t reserved_828;
    uint32_t reserved_82c;
    uint32_t reserved_830;
    uint32_t reserved_834;
    uint32_t reserved_838;
    uint32_t reserved_83c;
    uint32_t reserved_840;
    uint32_t reserved_844;
    uint32_t reserved_848;
    uint32_t reserved_84c;
    uint32_t reserved_850;
    uint32_t reserved_854;
    uint32_t reserved_858;
    uint32_t reserved_85c;
    uint32_t reserved_860;
    uint32_t reserved_864;
    uint32_t reserved_868;
    uint32_t reserved_86c;
    uint32_t reserved_870;
    uint32_t reserved_874;
    uint32_t reserved_878;
    uint32_t reserved_87c;
    uint32_t reserved_880;
    uint32_t reserved_884;
    uint32_t reserved_888;
    uint32_t reserved_88c;
    uint32_t reserved_890;
    uint32_t reserved_894;
    uint32_t reserved_898;
    uint32_t reserved_89c;
    uint32_t reserved_8a0;
    uint32_t reserved_8a4;
    uint32_t reserved_8a8;
    uint32_t reserved_8ac;
    uint32_t reserved_8b0;
    uint32_t reserved_8b4;
    uint32_t reserved_8b8;
    uint32_t reserved_8bc;
    uint32_t reserved_8c0;
    uint32_t reserved_8c4;
    uint32_t reserved_8c8;
    uint32_t reserved_8cc;
    uint32_t reserved_8d0;
    uint32_t reserved_8d4;
    uint32_t reserved_8d8;
    uint32_t reserved_8dc;
    uint32_t reserved_8e0;
    uint32_t reserved_8e4;
    uint32_t reserved_8e8;
    uint32_t reserved_8ec;
    uint32_t reserved_8f0;
    uint32_t reserved_8f4;
    uint32_t reserved_8f8;
    uint32_t reserved_8fc;
    uint32_t reserved_900;
    uint32_t reserved_904;
    uint32_t reserved_908;
    uint32_t reserved_90c;
    uint32_t reserved_910;
    uint32_t reserved_914;
    uint32_t reserved_918;
    uint32_t reserved_91c;
    uint32_t reserved_920;
    uint32_t reserved_924;
    uint32_t reserved_928;
    uint32_t reserved_92c;
    uint32_t reserved_930;
    uint32_t reserved_934;
    uint32_t reserved_938;
    uint32_t reserved_93c;
    uint32_t reserved_940;
    uint32_t reserved_944;
    uint32_t reserved_948;
    uint32_t reserved_94c;
    uint32_t reserved_950;
    uint32_t reserved_954;
    uint32_t reserved_958;
    uint32_t reserved_95c;
    uint32_t reserved_960;
    uint32_t reserved_964;
    uint32_t reserved_968;
    uint32_t reserved_96c;
    uint32_t reserved_970;
    uint32_t reserved_974;
    uint32_t reserved_978;
    uint32_t reserved_97c;
    uint32_t reserved_980;
    uint32_t reserved_984;
    uint32_t reserved_988;
    uint32_t reserved_98c;
    uint32_t reserved_990;
    uint32_t reserved_994;
    uint32_t reserved_998;
    uint32_t reserved_99c;
    uint32_t reserved_9a0;
    uint32_t reserved_9a4;
    uint32_t reserved_9a8;
    uint32_t reserved_9ac;
    uint32_t reserved_9b0;
    uint32_t reserved_9b4;
    uint32_t reserved_9b8;
    uint32_t reserved_9bc;
    uint32_t reserved_9c0;
    uint32_t reserved_9c4;
    uint32_t reserved_9c8;
    uint32_t reserved_9cc;
    uint32_t reserved_9d0;
    uint32_t reserved_9d4;
    uint32_t reserved_9d8;
    uint32_t reserved_9dc;
    uint32_t reserved_9e0;
    uint32_t reserved_9e4;
    uint32_t reserved_9e8;
    uint32_t reserved_9ec;
    uint32_t reserved_9f0;
    uint32_t reserved_9f4;
    uint32_t reserved_9f8;
    uint32_t reserved_9fc;
    union {
        struct {
            uint32_t rid_err_cnt                   :    4;  /*AXI read id err cnt*/
            uint32_t rresp_err_cnt                 :    4;  /*AXI read resp err cnt*/
            uint32_t wresp_err_cnt                 :    4;  /*AXI write resp err cnt*/
            uint32_t rd_fifo_cnt                   :    3;  /*AXI read cmd fifo remain cmd count*/
            uint32_t rd_bak_fifo_cnt               :    3;  /*AXI read backup cmd fifo remain cmd count*/
            uint32_t wr_fifo_cnt                   :    3;  /*AXI write cmd fifo remain cmd count*/
            uint32_t wr_bak_fifo_cnt               :    5;  /*AXI write backup cmd fifo remain cmd count*/
            uint32_t reserved26                    :    6;  /*reserved*/
        };
        uint32_t val;
    } axi_err;
    union {
        struct {
            uint32_t axim_rd_rst                   :    1;  /*Write 1 then write 0  to this bit to reset axi master read data FIFO.*/
            uint32_t axim_wr_rst                   :    1;  /*Write 1 then write 0  to this bit to reset axi master write data FIFO.*/
            uint32_t clk_en                        :    1;  /*1'h1: Force clock on for register. 1'h0: Support clock only when application writes registers.*/
            uint32_t reserved3                     :    29;
        };
        uint32_t val;
    } rst_conf;
    uint32_t intr_mem_start_addr;
    uint32_t intr_mem_end_addr;
    uint32_t extr_mem_start_addr;
    uint32_t extr_mem_end_addr;
    union {
        struct {
            uint32_t arb_timenum                   :    16;  /*Set the max number of timeout count of arbiter*/
            uint32_t weight_en                     :    1;
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } out_arb_config;
    union {
        struct {
            uint32_t arb_timenum                   :    16;  /*Set the max number of timeout count of arbiter*/
            uint32_t weight_en                     :    1;
            uint32_t reserved17                    :    15;
        };
        uint32_t val;
    } in_arb_config;
    union {
        struct {
            uint32_t rdn_ena                       :    1;  /*reserved*/
            uint32_t rdn_result                    :    1;  /*reserved*/
            uint32_t reserved2                     :    30;  /*reserved*/
        };
        uint32_t val;
    } rdn_result;
    uint32_t rdn_eco_high;
    uint32_t rdn_eco_low;
    uint32_t date;
} dma2d_dev_t;
extern dma2d_dev_t DMA2D;
#ifdef __cplusplus
}
#endif



#endif /*_SOC_DMA2D_STRUCT_H_ */
