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
#ifndef _SOC_I3C_MST_MEM_STRUCT_H_
#define _SOC_I3C_MST_MEM_STRUCT_H_


#ifdef __cplusplus
extern "C" {
#endif

typedef volatile struct {
    uint32_t reserved_0;
    uint32_t reserved_4;
    uint32_t command_buf_port;
    uint32_t response_buf_port;
    uint32_t rx_data_port;
    uint32_t tx_data_port;
    union {
        struct {
            uint32_t data_length                   :    8;  /*This field represents the length of data received along with IBI, in bytes.*/
            uint32_t ibi_id                        :    8;  /*IBI Identifier. The byte received after START which includes the address the R/W bit: Device  address and R/W bit in case of Slave Interrupt or Master Request.*/
            uint32_t reserved16                    :    12;
            uint32_t ibi_sts                       :    1;  /*IBI received data/status. IBI Data register is mapped to the IBI Buffer. The IBI Data is always packed in4-byte aligned and put to the IBI Buffer. This register When read from, reads the data from the IBI buffer. IBI Status register when read from, returns the data from the IBI Buffer and indicates how the controller responded to incoming IBI(SIR, MR and HJ).*/
            uint32_t reserved29                    :    3;  /*In-Band Interrupt Buffer Status/Data Register. When receiving an IBI, IBI_PORT is used to both: Read the IBI Status Read the IBI Data(which is raw/opaque data)*/
        };
        uint32_t val;
    } ibi_status_buf;
    uint32_t reserved_1c;
    uint32_t reserved_20;
    uint32_t reserved_24;
    uint32_t reserved_28;
    uint32_t reserved_2c;
    uint32_t reserved_30;
    uint32_t reserved_34;
    uint32_t reserved_38;
    uint32_t reserved_3c;
    uint32_t ibi_data_buf;
    uint32_t reserved_44;
    uint32_t reserved_48;
    uint32_t reserved_4c;
    uint32_t reserved_50;
    uint32_t reserved_54;
    uint32_t reserved_58;
    uint32_t reserved_5c;
    uint32_t reserved_60;
    uint32_t reserved_64;
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
    union {
        struct {
            uint32_t static_addr                   :    7;
            uint32_t reserved7                     :    9;
            uint32_t dynamic_addr                  :    8;  /*Device Dynamic Address with parity, The MSB,bit[23], should be programmed with parity of dynamic address.*/
            uint32_t reserved24                    :    5;
            uint32_t nack_retry_cnt                :    2;  /*This field is used to set the Device NACK Retry count for the particular device. If the Device NACK's for the device address, the controller automatically retries the same device until this count expires. If the Slave does not ACK for the mentioned number of retries, then controller generates an error response and move to the Halt state.*/
            uint32_t i2c                           :    1;  /*Legacy I2C device or not. This bit should be set to 1 if the device is a legacy I2C device.*/
        };
        uint32_t val;
    } dev_addr_table[12];
    uint32_t reserved_f0;
    uint32_t reserved_f4;
    uint32_t reserved_f8;
    uint32_t reserved_fc;
    struct {
        uint32_t loc1;
        uint32_t loc2;
        uint32_t loc3;
        uint32_t loc4;
    } dev_char_table[12];
} i3c_mst_mem_dev_t;
extern i3c_mst_mem_dev_t I3C_MST_MEM;
#ifdef __cplusplus
}
#endif



#endif /*_SOC_I3C_MST_MEM_STRUCT_H_ */
