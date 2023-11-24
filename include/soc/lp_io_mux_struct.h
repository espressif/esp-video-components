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
#ifndef _SOC_LP_IO_MUX_STRUCT_H_
#define _SOC_LP_IO_MUX_STRUCT_H_


#ifdef __cplusplus
extern "C" {
#endif

typedef volatile struct {
    union {
        struct {
            uint32_t reserved0                     :    13;
            uint32_t fun_ie                        :    1;  /*input enable in work mode*/
            uint32_t soe                           :    1;  /*output enable in sleep mode*/
            uint32_t sie                           :    1;  /*input enable in sleep mode*/
            uint32_t ssel                          :    1;  /*1: enable sleep mode during sleep,0: no sleep mode*/
            uint32_t fun_sel                       :    2;  /*function sel*/
            uint32_t mux_sel                       :    1;  /*1: use RTC GPIO,0: use digital GPIO*/
            uint32_t reserved20                    :    7;
            uint32_t rue                           :    1;  /*RUE*/
            uint32_t rde                           :    1;  /*RDE*/
            uint32_t drv                           :    2;  /*DRV*/
            uint32_t reserved31                    :    1;
        };
        uint32_t val;
    } pad[24];
    union {
        struct {
            uint32_t reserved0                     :    27;
            uint32_t ext_wakeup0_sel               :    5;
        };
        uint32_t val;
    } ext_wakeup0;
    union {
        struct {
            uint32_t reserved0                     :    27;
            uint32_t xtl_ext_ctr_sel               :    5;  /*select RTC GPIO 0 ~ 23 to control XTAL*/
        };
        uint32_t val;
    } xtl_ext_ctr;
    union {
        struct {
            uint32_t pad0                          :    1;
            uint32_t pad1                          :    1;
            uint32_t pad2                          :    1;
            uint32_t pad3                          :    1;
            uint32_t pad4                          :    1;
            uint32_t pad5                          :    1;
            uint32_t pad6                          :    1;
            uint32_t pad7                          :    1;
            uint32_t pad8                          :    1;
            uint32_t pad9                          :    1;
            uint32_t pad10                         :    1;
            uint32_t pad11                         :    1;
            uint32_t pad12                         :    1;
            uint32_t pad13                         :    1;
            uint32_t pad14                         :    1;
            uint32_t pad15                         :    1;
            uint32_t pad16                         :    1;
            uint32_t pad17                         :    1;
            uint32_t pad18                         :    1;
            uint32_t pad19                         :    1;
            uint32_t pad20                         :    1;
            uint32_t pad21                         :    1;
            uint32_t pad22                         :    1;
            uint32_t pad23                         :    1;
            uint32_t reserved24                    :    8;
        };
        uint32_t val;
    } rtc_pad_hold;
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
    uint32_t reserved_100;
    uint32_t reserved_104;
    uint32_t reserved_108;
    uint32_t reserved_10c;
    uint32_t reserved_110;
    uint32_t reserved_114;
    uint32_t reserved_118;
    uint32_t reserved_11c;
    uint32_t reserved_120;
    uint32_t reserved_124;
    uint32_t reserved_128;
    uint32_t reserved_12c;
    uint32_t reserved_130;
    uint32_t reserved_134;
    uint32_t reserved_138;
    uint32_t reserved_13c;
    uint32_t reserved_140;
    uint32_t reserved_144;
    uint32_t reserved_148;
    uint32_t reserved_14c;
    uint32_t reserved_150;
    uint32_t reserved_154;
    uint32_t reserved_158;
    uint32_t reserved_15c;
    uint32_t reserved_160;
    uint32_t reserved_164;
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
    union {
        struct {
            uint32_t clk_en                        :    1;
            uint32_t reserved1                     :    31;
        };
        uint32_t val;
    } clk_en;
    union {
        struct {
            uint32_t date                          :    28;
            uint32_t reserved28                    :    4;
        };
        uint32_t val;
    } date;
} lp_io_mux_dev_t;
extern lp_io_mux_dev_t LP_IO_MUX;
#ifdef __cplusplus
}
#endif



#endif /*_SOC_LP_IO_MUX_STRUCT_H_ */
