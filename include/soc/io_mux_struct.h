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
#ifndef _SOC_IO_MUX_STRUCT_H_
#define _SOC_IO_MUX_STRUCT_H_


#ifdef __cplusplus
extern "C" {
#endif

typedef volatile struct {
    uint32_t reserved_0;
    union {
        struct {
            uint32_t mcu_oe                        :    1;
            uint32_t slp_sel                       :    1;
            uint32_t mcu_wpd                       :    1;
            uint32_t mcu_wpu                       :    1;
            uint32_t mcu_ie                        :    1;
            uint32_t mcu_drv                       :    2;
            uint32_t fun_wpd                       :    1;
            uint32_t fun_wpu                       :    1;
            uint32_t fun_ie                        :    1;
            uint32_t fun_drv                       :    2;
            uint32_t mcu_sel                       :    3;
            uint32_t filter_en                     :    1;
            uint32_t reserved16                    :    16;
        };
        uint32_t val;
    } gpio[64];
    union {
        struct {
            uint32_t io_mux_date_reg               :    27;
            uint32_t fast_test_pad_out             :    1;  /*output to gpio matrix for fast output const0 or const1*/
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } date;
} io_mux_dev_t;
extern io_mux_dev_t IO_MUX;
#ifdef __cplusplus
}
#endif



#endif /*_SOC_IO_MUX_STRUCT_H_ */
