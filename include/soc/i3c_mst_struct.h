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
#ifndef _SOC_I3C_MST_STRUCT_H_
#define _SOC_I3C_MST_STRUCT_H_


#ifdef __cplusplus
extern "C" {
#endif

typedef volatile struct {
    union {
        struct {
            uint32_t reserved0                     :    1;
            uint32_t ba_include                    :    1;  /*This bit is used to include I3C broadcast address(0x7E) for private transfer.(If I3C broadcast address is not include for the private transfer, In-Band Interrupts driven from Slaves may not win address arbitration. Hence IBIs will get delayed)*/
            uint32_t trans_start                   :    1;  /*Transfer Start*/
            uint32_t clk_en                        :    1;
            uint32_t ibi_rstart_trans_en           :    1;
            uint32_t auto_dis_ibi_en               :    1;
            uint32_t dma_rx_en                     :    1;
            uint32_t dma_tx_en                     :    1;
            uint32_t multi_slv_single_ccc_en       :    1;  /*0: rx high bit first, 1: rx low bit first*/
            uint32_t rx_bit_order                  :    1;  /*0: rx low byte fist, 1: rx high byte first*/
            uint32_t rx_byte_order                 :    1;
            uint32_t scl_pullup_force_en           :    1;  /*This bit is used to force scl_pullup_en */
            uint32_t scl_oe_force_en               :    1;  /*This bit is used to force scl_oe*/
            uint32_t sda_pp_rd_pullup_en           :    1;
            uint32_t sda_rd_tbit_hlvl_pullup_en    :    1;
            uint32_t sda_pp_wr_pullup_en           :    1;
            uint32_t data_byte_cnt_unlatch         :    1;  /*1: read current real-time updated value  0: read latch data byte cnt value*/
            uint32_t mem_clk_force_on              :    1;  /*1: dev characteristic and address table memory clk  date force on . 0 :  clock gating by rd/wr.*/
            uint32_t reserved18                    :    14;  /*DEVICE_CTRL register controls the transfer properties and disposition of controllers capabilities.*/
        };
        uint32_t val;
    } device_ctrl;
    uint32_t reserved_4;
    uint32_t reserved_8;
    uint32_t reserved_c;
    uint32_t reserved_10;
    uint32_t reserved_14;
    uint32_t reserved_18;
    union {
        struct {
            uint32_t cmd_buf_empty_thld            :    4;  /*Command Buffer Empty Threshold Value is used to control the number of empty locations(or greater) in the Command Buffer that trigger CMD_BUFFER_READY_STAT interrupt. */
            uint32_t reserved4                     :    2;
            uint32_t resp_buf_thld                 :    3;  /*Response Buffer Threshold Value is used to control the number of entries in the Response Buffer that trigger the RESP_READY_STAT_INTR. */
            uint32_t reserved9                     :    3;
            uint32_t ibi_data_buf_thld             :    3;  /*In-Band Interrupt Data Threshold Value . Every In Band Interrupt received by I3C controller generates an IBI status. This field controls the number of IBI data entries in the IBI buffer that trigger the IBI_DATA_THLD_STAT interrupt. */
            uint32_t reserved15                    :    3;
            uint32_t ibi_status_buf_thld           :    3;
            uint32_t reserved21                    :    11;  /*In-Band Interrupt Status Threshold Value . Every In Band Interrupt received by I3C controller generates an IBI status. This field controls the number of IBI status entries in the IBI buffer that trigger the IBI_STATUS_THLD_STAT interrupt. */
        };
        uint32_t val;
    } buffer_thld_ctrl;
    union {
        struct {
            uint32_t tx_data_buf_thld              :    3;  /*Transmit Buffer Threshold Value. This field controls the number of empty locations in the Transmit FIFO that trigger the TX_THLD_STAT interrupt. Supports values: 000:2  001:4  010:8  011:16  100:31*/
            uint32_t rx_data_buf_thld              :    3;  /*Receive Buffer Threshold Value. This field controls the number of empty locations in the Receive FIFO that trigger the RX_THLD_STAT interrupt. Supports: 000:2  001:4  010:8  011:16  100:31*/
            uint32_t reserved6                     :    26;
        };
        uint32_t val;
    } data_buffer_thld_ctrl;
    union {
        struct {
            uint32_t reserved0                     :    1;  /*Reserved*/
            uint32_t reserved1                     :    1;
            uint32_t notify_sir_rejected           :    1;  /*Notify Rejected Slave Interrupt Request Control. This bit is used to suppress reporting to the application about Slave Interrupt Request. 0:Suppress passing the IBI Status to the IBI FIFO(hence not notifying the application) when a SIR request is NACKed and auto-disabled base on the IBI_SIR_REQ_REJECT register. 1: Writes IBI Status to the IBI FIFO(hence notifying the application) when SIR request is NACKed and auto-disabled based on the IBI_SIR_REQ_REJECT registerl.*/
            uint32_t reserved3                     :    29;
        };
        uint32_t val;
    } ibi_notify_ctrl;
    uint32_t ibi_sir_req_payload;
    uint32_t ibi_sir_req_reject;
    union {
        struct {
            uint32_t tx_data_buf_thld              :    1;
            uint32_t rx_data_buf_thld              :    1;
            uint32_t ibi_status_thld               :    1;
            uint32_t cmd_buf_empty_thld            :    1;
            uint32_t resp_ready                    :    1;
            uint32_t nxt_cmd_req_err               :    1;
            uint32_t transfer_err                  :    1;
            uint32_t transfer_complete             :    1;
            uint32_t command_done                  :    1;
            uint32_t detect_start                  :    1;
            uint32_t resp_buf_ovf                  :    1;
            uint32_t ibi_data_buf_ovf              :    1;
            uint32_t ibi_status_buf_ovf            :    1;
            uint32_t ibi_handle_done               :    1;
            uint32_t ibi_detect                    :    1;
            uint32_t cmd_ccc_mismatch              :    1;
            uint32_t reserved16                    :    16;
        };
        uint32_t val;
    } int_clr;
    union {
        struct {
            uint32_t tx_data_buf_thld              :    1;
            uint32_t rx_data_buf_thld              :    1;
            uint32_t ibi_status_thld               :    1;
            uint32_t cmd_buf_empty_thld            :    1;
            uint32_t resp_ready                    :    1;
            uint32_t nxt_cmd_req_err               :    1;
            uint32_t transfer_err                  :    1;
            uint32_t transfer_complete             :    1;
            uint32_t command_done                  :    1;
            uint32_t detect_start                  :    1;
            uint32_t resp_buf_ovf                  :    1;
            uint32_t ibi_data_buf_ovf              :    1;
            uint32_t ibi_status_buf_ovf            :    1;
            uint32_t ibi_handle_done               :    1;
            uint32_t ibi_detect                    :    1;
            uint32_t cmd_ccc_mismatch              :    1;
            uint32_t reserved16                    :    16;
        };
        uint32_t val;
    } int_raw;
    union {
        struct {
            uint32_t tx_data_buf_thld              :    1;  /*This interrupt is generated when number of empty locations in transmit buffer is greater than or equal to threshold value specified by TX_EMPTY_BUS_THLD field in DATA_BUFFER_THLD_CTRL register. This interrupt will be cleared automatically when number of empty locations in transmit buffer is less than threshold value.*/
            uint32_t rx_data_buf_thld              :    1;  /*This interrupt is generated when number of entries in receive buffer is greater than or equal to threshold value specified by RX_BUF_THLD field in DATA_BUFFER_THLD_CTRL register. This interrupt will be cleared automatically when number of entries in receive buffer is less than threshold value.*/
            uint32_t ibi_status_thld               :    1;  /*Only used in master mode. This interrupt is generated when number of entries in IBI buffer is greater than or equal to threshold value specified by IBI_BUF_THLD field in BUFFER_THLD_CTRL register. This interrupt will be cleared automatically when number of entries in IBI buffer is less than threshold value.*/
            uint32_t cmd_buf_empty_thld            :    1;  /*This interrupt is generated when number of empty locations in command buffer is greater than or equal to threshold value specified by CMD_EMPTY_BUF_THLD field in BUFFER_THLD_CTRL register. This interrupt will be cleared automatically when number of empty locations in command buffer is less than threshold value.*/
            uint32_t resp_ready                    :    1;  /*This interrupt is generated when number of entries in response buffer is greater than or equal to threshold value specified by RESP_BUF_THLD field in BUFFER_THLD_CTRL register. This interrupt will be cleared automatically when number of entries in response buffer is less than threshold value.*/
            uint32_t nxt_cmd_req_err               :    1;  /*This interrupt is generated if toc is 0(master will restart next command), but command buf is empty.*/
            uint32_t transfer_err                  :    1;  /*This interrupt is generated if any error occurs during transfer. The error type will be specified in the response packet associated with the command (in ERR_STATUS field of RESPONSE_BUFFER_PORT register). This bit can be cleared by writing 1'h1.*/
            uint32_t transfer_complete             :    1;
            uint32_t command_done                  :    1;
            uint32_t detect_start                  :    1;
            uint32_t resp_buf_ovf                  :    1;
            uint32_t ibi_data_buf_ovf              :    1;
            uint32_t ibi_status_buf_ovf            :    1;
            uint32_t ibi_handle_done               :    1;
            uint32_t ibi_detect                    :    1;
            uint32_t cmd_ccc_mismatch              :    1;
            uint32_t reserved16                    :    16;
        };
        uint32_t val;
    } int_st;
    union {
        struct {
            uint32_t tx_data_buf_thld              :    1;  /*Transmit Buffer threshold status enable.*/
            uint32_t rx_data_buf_thld              :    1;  /*Receive Buffer threshold status enable.*/
            uint32_t ibi_status_thld               :    1;  /*Only used in master mode. IBI Buffer threshold status enable.*/
            uint32_t cmd_buf_empty_thld            :    1;  /*Command buffer ready status enable.*/
            uint32_t resp_ready                    :    1;  /*Response buffer ready status enable.*/
            uint32_t nxt_cmd_req_err               :    1;  /*next command request error status enable*/
            uint32_t transfer_err                  :    1;  /*Transfer error status enable*/
            uint32_t transfer_complete             :    1;
            uint32_t command_done                  :    1;
            uint32_t detect_start                  :    1;
            uint32_t resp_buf_ovf                  :    1;
            uint32_t ibi_data_buf_ovf              :    1;
            uint32_t ibi_status_buf_ovf            :    1;
            uint32_t ibi_handle_done               :    1;
            uint32_t ibi_detect                    :    1;
            uint32_t cmd_ccc_mismatch              :    1;
            uint32_t reserved16                    :    16;  /*The Interrupt status will be updated in INTR_STATUS register if corresponding Status Enable bit set.*/
        };
        uint32_t val;
    } int_ena;
    uint32_t reserved_40;
    union {
        struct {
            uint32_t core_soft_rst                 :    1;
            uint32_t cmd_buf_rst                   :    1;
            uint32_t resp_buf_rst                  :    1;
            uint32_t tx_data_buf_buf_rst           :    1;
            uint32_t rx_data_buf_rst               :    1;
            uint32_t ibi_data_buf_rst              :    1;
            uint32_t ibi_status_buf_rst            :    1;
            uint32_t reserved7                     :    25;
        };
        uint32_t val;
    } reset_ctrl;
    union {
        struct {
            uint32_t cmd_buf_empty_cnt             :    5;  /*Command Buffer Empty Locations contains the number of empty locations in the command buffer.*/
            uint32_t reserved5                     :    3;
            uint32_t resp_buf_cnt                  :    4;  /*Response Buffer Level Value contains the number of valid data entries in the response buffer.*/
            uint32_t reserved12                    :    4;
            uint32_t ibi_data_buf_cnt              :    4;  /*IBI Buffer Level Value contains the number of valid entries in the IBI Buffer. This is field is used in master mode.*/
            uint32_t reserved20                    :    4;
            uint32_t ibi_status_buf_cnt            :    4;  /*IBI Buffer Status Count contains the number of IBI status entries in the IBI Buffer. This field is used in master mode.*/
            uint32_t reserved28                    :    4;  /*BUFFER_STATUS_LEVEL reflects the status level of Buffers in the controller.*/
        };
        uint32_t val;
    } buffer_status_level;
    union {
        struct {
            uint32_t tx_data_buf_empty_cnt         :    6;  /*Transmit Buffer Empty Level Value contains the number of empty locations in the transmit Buffer.*/
            uint32_t reserved6                     :    10;
            uint32_t rx_data_buf_cnt               :    6;  /*Receive Buffer Level value contains the number of valid data entries in the receive buffer.*/
            uint32_t reserved22                    :    10;  /*DATA_BUFFER_STATUS_LEVEL reflects the status level of the Buffers in the controller.*/
        };
        uint32_t val;
    } data_buffer_status_level;
    union {
        struct {
            uint32_t sda_lvl                       :    1;  /*This bit is used to check the SCL line level to recover from error and  for debugging. This bit reflects the value of synchronized scl_in_a. */
            uint32_t scl_lvl                       :    1;  /*This bit is used to check the SDA line level to recover from error and  for debugging. This bit reflects the value of synchronized sda_in_a. */
            uint32_t bus_busy                      :    1;
            uint32_t bus_free                      :    1;
            uint32_t reserved4                     :    5;
            uint32_t cmd_tid                       :    4;
            uint32_t scl_gen_fsm_state             :    3;
            uint32_t ibi_ev_handle_fsm_state       :    3;
            uint32_t i2c_mode_fsm_state            :    3;
            uint32_t sdr_mode_fsm_state            :    4;
            uint32_t daa_mode_fsm_state            :    3;  /*Reflects whether the Master Controller is in IDLE or not. This bit will be set when all the buffer(Command, Response, IBI, Transmit, Receive) are empty along with the Master State machine is in idle state. 0X0: not in idle  0x1: in idle*/
            uint32_t mst_main_fsm_state            :    3;
        };
        uint32_t val;
    } present_state0;
    union {
        struct {
            uint32_t data_byte_cnt                 :    16;  /*Present transfer data byte cnt: tx data byte cnt if write  rx data byte cnt if read  ibi data byte cnt if IBI handle.*/
            uint32_t reserved16                    :    16;
        };
        uint32_t val;
    } present_state1;
    union {
        struct {
            uint32_t dct_daa_init_index            :    4;  /*Reserved*/
            uint32_t dat_daa_init_index            :    4;
            uint32_t present_dct_index             :    4;
            uint32_t present_dat_index             :    4;
            uint32_t reserved16                    :    16;  /*Pointer for Device Address Table*/
        };
        uint32_t val;
    } device_table;
    union {
        struct {
            uint32_t resp_buf_to_value             :    5;
            uint32_t resp_buf_to_en                :    1;
            uint32_t ibi_data_buf_to_value         :    5;
            uint32_t ibi_data_buf_to_en            :    1;
            uint32_t ibi_status_buf_to_value       :    5;
            uint32_t ibi_status_buf_to_en          :    1;
            uint32_t rx_data_buf_to_value          :    5;
            uint32_t rx_data_buf_to_en             :    1;
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } time_out_value;
    union {
        struct {
            uint32_t i3c_od_low_period             :    16;  /*SCL Open-Drain low count for I3C transfers targeted to I3C devices.*/
            uint32_t i3c_od_high_period            :    16;  /*SCL Open-Drain High count for I3C transfers targeted to I3C devices.*/
        };
        uint32_t val;
    } scl_i3c_od_time;
    union {
        struct {
            uint32_t i3c_pp_low_period             :    8;
            uint32_t reserved8                     :    8;
            uint32_t i3c_pp_high_period            :    8;
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } scl_i3c_pp_time;
    union {
        struct {
            uint32_t i2c_fm_low_period             :    16;
            uint32_t i2c_fm_high_period            :    16;  /*The SCL open-drain low count timing for I2C Fast Mode transfers.*/
        };
        uint32_t val;
    } scl_i2c_fm_time;
    union {
        struct {
            uint32_t i2c_fmp_low_period            :    16;
            uint32_t i2c_fmp_high_period           :    8;
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } scl_i2c_fmp_time;
    union {
        struct {
            uint32_t i3c_ext_low_period1           :    8;
            uint32_t i3c_ext_low_period2           :    8;
            uint32_t i3c_ext_low_period3           :    8;
            uint32_t i3c_ext_low_period4           :    8;
        };
        uint32_t val;
    } scl_ext_low_time;
    union {
        struct {
            uint32_t sda_od_sample_time            :    9;  /*It is used to adjust sda sample point when scl high under open drain speed*/
            uint32_t sda_pp_sample_time            :    5;  /*It is used to adjust sda sample point when scl high under push pull speed*/
            uint32_t reserved14                    :    18;
        };
        uint32_t val;
    } sda_sample_time;
    union {
        struct {
            uint32_t sda_od_tx_hold_time           :    9;  /*It is used to adjust sda drive point after scl neg under open drain speed*/
            uint32_t sda_pp_tx_hold_time           :    5;  /*It is used to adjust sda dirve point after scl neg under push pull speed*/
            uint32_t reserved14                    :    18;
        };
        uint32_t val;
    } sda_hold_time;
    union {
        struct {
            uint32_t scl_start_hold_time           :    9;  /*I2C_SCL_START_HOLD_TIME*/
            uint32_t start_det_hold_time           :    2;
            uint32_t reserved11                    :    21;
        };
        uint32_t val;
    } scl_start_hold;
    union {
        struct {
            uint32_t scl_rstart_setup_time         :    9;  /*I2C_SCL_RSTART_SETUP_TIME*/
            uint32_t reserved9                     :    23;
        };
        uint32_t val;
    } scl_rstart_setup;
    union {
        struct {
            uint32_t scl_stop_hold_time            :    9;  /*I2C_SCL_STOP_HOLD_TIME*/
            uint32_t reserved9                     :    23;
        };
        uint32_t val;
    } scl_stop_hold;
    union {
        struct {
            uint32_t scl_stop_setup_time           :    9;  /*I2C_SCL_STOP_SETUP_TIME*/
            uint32_t reserved9                     :    23;
        };
        uint32_t val;
    } scl_stop_setup;
    uint32_t reserved_8c;
    union {
        struct {
            uint32_t bus_free_time                 :    16;  /*I3C Bus Free Count Value. This field is used only in Master mode. In pure Bus System, this field represents tCAS.  In Mixed Bus System, this field is expected to be programmed to tLOW of I2C Timing.*/
            uint32_t reserved16                    :    16;
        };
        uint32_t val;
    } bus_free_time;
    union {
        struct {
            uint32_t i3c_termn_t_ext_low_time      :    8;
            uint32_t reserved8                     :    24;
        };
        uint32_t val;
    } scl_termn_t_ext_low_time;
    uint32_t reserved_98;
    uint32_t reserved_9c;
    uint32_t i3c_ver_id;
    uint32_t i3c_ver_type;
    uint32_t reserved_a8;
    uint32_t fpga_debug_probe;
    union {
        struct {
            uint32_t rnd_eco_en                    :    1;
            uint32_t rnd_eco_result                :    1;
            uint32_t reserved2                     :    30;
        };
        uint32_t val;
    } rnd_eco_cs;
    uint32_t rnd_eco_low;
    uint32_t rnd_eco_high;
} i3c_mst_dev_t;
extern i3c_mst_dev_t I3C_MST;
#ifdef __cplusplus
}
#endif



#endif /*_SOC_I3C_MST_STRUCT_H_ */
