// Copyright 2017-2018 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef _SOC_GPIO_SIG_MAP_H_
#define _SOC_GPIO_SIG_MAP_H_

#define MSPIQ_PAD_IN_IDX               0
#define MSPIQ_PAD_OUT_IDX              0
#define MSPID_PAD_IN_IDX               1
#define MSPID_PAD_OUT_IDX              1
#define MSPIHOLD_PAD_IN_IDX            2
#define MSPIHOLD_PAD_OUT_IDX           2
#define MSPIWP_PAD_IN_IDX              3
#define MSPIWP_PAD_OUT_IDX             3
#define MSPICK_PAD_OUT_IDX             4
#define MSPICS_PAD_OUT_IDX             5
#define MSPICS1_PAD_OUT_IDX            6
#define MSPID4_PAD_IN_IDX              7
#define MSPID4_PAD_OUT_IDX             7
#define MSPID5_PAD_IN_IDX              8
#define MSPID5_PAD_OUT_IDX             8
#define MSPID6_PAD_IN_IDX              9
#define MSPID6_PAD_OUT_IDX             9
#define MSPID7_PAD_IN_IDX              10
#define MSPID7_PAD_OUT_IDX             10
#define MSPIDQS_PAD_IN_IDX             11
#define MSPIDQS_PAD_OUT_IDX            11
#define UART0_RXD_PAD_IN_IDX           12
#define UART0_TXD_PAD_OUT_IDX          12
#define UART0_CTS_PAD_IN_IDX           13
#define UART0_RTS_PAD_OUT_IDX          13
#define UART0_DSR_PAD_IN_IDX           14
#define UART0_DTR_PAD_OUT_IDX          14
#define UART1_RXD_PAD_IN_IDX           15
#define UART1_TXD_PAD_OUT_IDX          15
#define UART1_CTS_PAD_IN_IDX           16
#define UART1_RTS_PAD_OUT_IDX          16
#define UART1_DSR_PAD_IN_IDX           17
#define UART1_DTR_PAD_OUT_IDX          17
#define UART2_RXD_PAD_IN_IDX           18
#define UART2_TXD_PAD_OUT_IDX          18
#define UART2_CTS_PAD_IN_IDX           19
#define UART2_RTS_PAD_OUT_IDX          19
#define UART2_DSR_PAD_IN_IDX           20
#define UART2_DTR_PAD_OUT_IDX          20
#define UART3_RXD_PAD_IN_IDX           21
#define UART3_TXD_PAD_OUT_IDX          21
#define UART3_CTS_PAD_IN_IDX           22
#define UART3_RTS_PAD_OUT_IDX          22
#define UART3_DSR_PAD_IN_IDX           23
#define UART3_DTR_PAD_OUT_IDX          23
#define UART4_RXD_PAD_IN_IDX           24
#define UART4_TXD_PAD_OUT_IDX          24
#define UART4_CTS_PAD_IN_IDX           25
#define UART4_RTS_PAD_OUT_IDX          25
#define UART4_DSR_PAD_IN_IDX           26
#define UART4_DTR_PAD_OUT_IDX          26
#define I2S0_O_BCK_PAD_IN_IDX          27
#define I2S0_O_BCK_PAD_OUT_IDX         27
#define I2S0_MCLK_PAD_IN_IDX           28
#define I2S0_MCLK_PAD_OUT_IDX          28
#define I2S0_O_WS_PAD_IN_IDX           29
#define I2S0_O_WS_PAD_OUT_IDX          29
#define I2S0_I_SD_PAD_IN_IDX           30
#define I2S0_O_SD_PAD_OUT_IDX          30
#define I2S0_I_BCK_PAD_IN_IDX          31
#define I2S0_I_BCK_PAD_OUT_IDX         31
#define I2S0_I_WS_PAD_IN_IDX           32
#define I2S0_I_WS_PAD_OUT_IDX          32
#define I2S1_O_BCK_PAD_IN_IDX          33
#define I2S1_O_BCK_PAD_OUT_IDX         33
#define I2S1_MCLK_PAD_IN_IDX           34
#define I2S1_MCLK_PAD_OUT_IDX          34
#define I2S1_O_WS_PAD_IN_IDX           35
#define I2S1_O_WS_PAD_OUT_IDX          35
#define I2S1_I_SD_PAD_IN_IDX           36
#define I2S1_O_SD_PAD_OUT_IDX          36
#define I2S1_I_BCK_PAD_IN_IDX          37
#define I2S1_I_BCK_PAD_OUT_IDX         37
#define I2S1_I_WS_PAD_IN_IDX           38
#define I2S1_I_WS_PAD_OUT_IDX          38
#define I2S2_O_BCK_PAD_IN_IDX          39
#define I2S2_O_BCK_PAD_OUT_IDX         39
#define I2S2_MCLK_PAD_IN_IDX           40
#define I2S2_MCLK_PAD_OUT_IDX          40
#define I2S2_O_WS_PAD_IN_IDX           41
#define I2S2_O_WS_PAD_OUT_IDX          41
#define I2S2_I_SD_PAD_IN_IDX           42
#define I2S2_O_SD_PAD_OUT_IDX          42
#define I2S2_I_BCK_PAD_IN_IDX          43
#define I2S2_I_BCK_PAD_OUT_IDX         43
#define I2S2_I_WS_PAD_IN_IDX           44
#define I2S2_I_WS_PAD_OUT_IDX          44
#define I2S0_I_SD1_PAD_IN_IDX          45
#define I2S0_O_SD1_PAD_OUT_IDX         45
#define I2S0_I_SD2_PAD_IN_IDX          46
#define USB_EXTPHY_OEN_PAD_OUT_IDX     46
#define I2S0_I_SD3_PAD_IN_IDX          47
#define SPI3_CS1_PAD_OUT_IDX           47
#define SPI3_CK_PAD_IN_IDX             48
#define SPI3_CK_PAD_OUT_IDX            48
#define SPI3_Q_PAD_IN_IDX              49
#define SPI3_QO_PAD_OUT_IDX            49
#define SPI3_D_PAD_IN_IDX              50
#define SPI3_D_PAD_OUT_IDX             50
#define SPI3_HOLD_PAD_IN_IDX           51
#define SPI3_HOLD_PAD_OUT_IDX          51
#define SPI3_WP_PAD_IN_IDX             52
#define SPI3_WP_PAD_OUT_IDX            52
#define SPI3_CS_PAD_IN_IDX             53
#define SPI3_CS_PAD_OUT_IDX            53
#define SPI2_CK_PAD_IN_IDX             54
#define SPI2_CK_PAD_OUT_IDX            54
#define SPI2_Q_PAD_IN_IDX              55
#define SPI2_Q_PAD_OUT_IDX             55
#define SPI2_D_PAD_IN_IDX              56
#define SPI2_D_PAD_OUT_IDX             56
#define SPI2_HOLD_PAD_IN_IDX           57
#define SPI2_HOLD_PAD_OUT_IDX          57
#define SPI2_WP_PAD_IN_IDX             58
#define SPI2_WP_PAD_OUT_IDX            58
#define SPI2_IO4_PAD_IN_IDX            59
#define SPI2_IO4_PAD_OUT_IDX           59
#define SPI2_IO5_PAD_IN_IDX            60
#define SPI2_IO5_PAD_OUT_IDX           60
#define SPI2_IO6_PAD_IN_IDX            61
#define SPI2_IO6_PAD_OUT_IDX           61
#define SPI2_IO7_PAD_IN_IDX            62
#define SPI2_IO7_PAD_OUT_IDX           62
#define SPI2_CS_PAD_IN_IDX             63
#define SPI2_CS_PAD_OUT_IDX            63
#define SPI2_CS1_PAD_OUT_IDX           64
#define SPI2_CS2_PAD_OUT_IDX           65
#define SPI2_CS3_PAD_OUT_IDX           66
#define SPI2_CS4_PAD_OUT_IDX           67
#define SPI2_CS5_PAD_OUT_IDX           68
#define I2C0_SCL_PAD_IN_IDX            69
#define I2C0_SCL_PAD_OUT_IDX           69
#define I2C0_SDA_PAD_IN_IDX            70
#define I2C0_SDA_PAD_OUT_IDX           70
#define I2C1_SCL_PAD_IN_IDX            71
#define I2C1_SCL_PAD_OUT_IDX           71
#define I2C1_SDA_PAD_IN_IDX            72
#define I2C1_SDA_PAD_OUT_IDX           72
#define GPIO_SD0_OUT_IDX               73
#define GPIO_SD1_OUT_IDX               74
#define GPIO_SD2_OUT_IDX               75
#define GPIO_SD3_OUT_IDX               76
#define GPIO_SD4_OUT_IDX               77
#define GPIO_SD5_OUT_IDX               78
#define GPIO_SD6_OUT_IDX               79
#define GPIO_SD7_OUT_IDX               80
#define CAN0_RX_PAD_IN_IDX             81
#define CAN0_TX_PAD_OUT_IDX            81
#define CAN0_BUS_OFF_ON_PAD_OUT_IDX    82
#define CAN0_CLKOUT_PAD_OUT_IDX        83
#define CAN1_RX_PAD_IN_IDX             84
#define CAN1_TX_PAD_OUT_IDX            84
#define CAN1_BUS_OFF_ON_PAD_OUT_IDX    85
#define CAN1_CLKOUT_PAD_OUT_IDX        86
#define CAN2_RX_PAD_IN_IDX             87
#define CAN2_TX_PAD_OUT_IDX            87
#define CAN2_BUS_OFF_ON_PAD_OUT_IDX    88
#define CAN2_CLKOUT_PAD_OUT_IDX        89
#define SUBMSPICK_PAD_OUT_IDX          90
#define SUBMSPIQ_PAD_IN_IDX            91
#define SUBMSPIQ_PAD_OUT_IDX           91
#define SUBMSPID_PAD_IN_IDX            92
#define SUBMSPID_PAD_OUT_IDX           92
#define SUBMSPIHOLD_PAD_IN_IDX         93
#define SUBMSPIHOLD_PAD_OUT_IDX        93
#define SUBMSPIWP_PAD_IN_IDX           94
#define SUBMSPIWP_PAD_OUT_IDX          94
#define SUBMSPICS0_PAD_OUT_IDX         95
#define SUBMSPICS1_PAD_OUT_IDX         96
#define SPI2_DQS_PAD_OUT_IDX           97
#define SPI3_CS2_PAD_OUT_IDX           98
#define SUBMSPID4_PAD_IN_IDX           99
#define SUBMSPID4_PAD_OUT_IDX          99
#define SUBMSPID5_PAD_IN_IDX           100
#define SUBMSPID5_PAD_OUT_IDX          100
#define SUBMSPID6_PAD_IN_IDX           101
#define SUBMSPID6_PAD_OUT_IDX          101
#define SUBMSPID7_PAD_IN_IDX           102
#define SUBMSPID7_PAD_OUT_IDX          102
#define SUBMSPIDQS_PAD_IN_IDX          103
#define SUBMSPIDQS_PAD_OUT_IDX         103
#define PWM0_SYNC0_PAD_IN_IDX          104
#define PWM0_CH0_A_PAD_OUT_IDX         104
#define PWM0_SYNC1_PAD_IN_IDX          105
#define PWM0_CH0_B_PAD_OUT_IDX         105
#define PWM0_SYNC2_PAD_IN_IDX          106
#define PWM0_CH1_A_PAD_OUT_IDX         106
#define PWM0_F0_PAD_IN_IDX             107
#define PWM0_CH1_B_PAD_OUT_IDX         107
#define PWM0_F1_PAD_IN_IDX             108
#define PWM0_CH2_A_PAD_OUT_IDX         108
#define PWM0_F2_PAD_IN_IDX             109
#define PWM0_CH2_B_PAD_OUT_IDX         109
#define PWM0_CAP0_PAD_IN_IDX           110
#define PWM1_CH0_A_PAD_OUT_IDX         110
#define PWM0_CAP1_PAD_IN_IDX           111
#define PWM1_CH0_B_PAD_OUT_IDX         111
#define PWM0_CAP2_PAD_IN_IDX           112
#define PWM1_CH1_A_PAD_OUT_IDX         112
#define PWM1_SYNC0_PAD_IN_IDX          113
#define PWM1_CH1_B_PAD_OUT_IDX         113
#define PWM1_SYNC1_PAD_IN_IDX          114
#define PWM1_CH2_A_PAD_OUT_IDX         114
#define PWM1_SYNC2_PAD_IN_IDX          115
#define PWM1_CH2_B_PAD_OUT_IDX         115
#define PWM1_F0_PAD_IN_IDX             116
#define SDIO_HOST_INT_PAD_OUT_IDX      116
#define PWM1_F1_PAD_IN_IDX             117
#define HP_PROBE_TOP_OUT0_IDX          117
#define PWM1_F2_PAD_IN_IDX             118
#define HP_PROBE_TOP_OUT1_IDX          118
#define PWM1_CAP0_PAD_IN_IDX           119
#define HP_PROBE_TOP_OUT2_IDX          119
#define PWM1_CAP1_PAD_IN_IDX           120
#define HP_PROBE_TOP_OUT3_IDX          120
#define PWM1_CAP2_PAD_IN_IDX           121
#define SDIO_SWITCH_VOLT_EN_PAD_OUT_IDX 121
#define GMII_MDI_PAD_IN_IDX            122
#define GMII_LOOPBK_PAD_OUT_IDX        122
#define GMAC_PHY_COL_PAD_IN_IDX        123
#define GMII_MDC_PAD_OUT_IDX           123
#define GMAC_PHY_CRS_PAD_IN_IDX        124
#define GMII_MDO_PAD_OUT_IDX           124
#define REVMII_MDC_PAD_IN_IDX          125
#define PCS_AQUIRED_SYNC_PAD_OUT_IDX   125
#define GMAC_TBI_SIGDET_PAD_IN_IDX     126
#define PCS_EN_CDET_PAD_OUT_IDX        126
#define PCS_EWRAP_PAD_OUT_IDX          127
#define PCS_LCK_REF_PAD_OUT_IDX        128
#define TWAI0_STANDBY_PAD_OUT_IDX      129
#define ADP_PRB_PAD_IN_IDX             130
#define TWAI1_STANDBY_PAD_OUT_IDX      130
#define ADP_SNS_PAD_IN_IDX             131
#define REVMII_COL_PAD_OUT_IDX         131
#define REVMII_CRS_PAD_OUT_IDX         132
#define ADP_CHRG_PAD_OUT_IDX           133
#define ADP_DISCHRG_PAD_OUT_IDX        134
#define ADP_PRB_EN_PAD_OUT_IDX         135
#define USB_OTG11_IDDIG_PAD_IN_IDX     136
#define ADP_SNS_EN_PAD_OUT_IDX         136
#define USB_OTG11_AVALID_PAD_IN_IDX    137
#define USB_OTG11_IDPULLUP_PAD_OUT_IDX 137
#define USB_SRP_BVALID_PAD_IN_IDX      138
#define USB_OTG11_DPPULLDOWN_PAD_OUT_IDX 138
#define USB_OTG11_VBUSVALID_PAD_IN_IDX 139
#define USB_OTG11_DMPULLDOWN_PAD_OUT_IDX 139
#define USB_SRP_SESSEND_PAD_IN_IDX     140
#define USB_OTG11_DRVVBUS_PAD_OUT_IDX  140
#define ULPI_DIR_PAD_IN_IDX            141
#define USB_SRP_CHRGVBUS_PAD_OUT_IDX   141
#define ULPI_NXT_PAD_IN_IDX            142
#define OTG_DRVVBUS_PAD_OUT_IDX        142
#define ULPI_CLK_PAD_IN_IDX            143
#define ULPI_STP_PAD_OUT_IDX           143
#define ULPI_DATA0_PAD_IN_IDX          144
#define ULPI_DATA0_PAD_OUT_IDX         144
#define ULPI_DATA1_PAD_IN_IDX          145
#define ULPI_DATA1_PAD_OUT_IDX         145
#define ULPI_DATA2_PAD_IN_IDX          146
#define ULPI_DATA2_PAD_OUT_IDX         146
#define ULPI_DATA3_PAD_IN_IDX          147
#define ULPI_DATA3_PAD_OUT_IDX         147
#define ULPI_DATA4_PAD_IN_IDX          148
#define ULPI_DATA4_PAD_OUT_IDX         148
#define ULPI_DATA5_PAD_IN_IDX          149
#define ULPI_DATA5_PAD_OUT_IDX         149
#define ULPI_DATA6_PAD_IN_IDX          150
#define ULPI_DATA6_PAD_OUT_IDX         150
#define ULPI_DATA7_PAD_IN_IDX          151
#define ULPI_DATA7_PAD_OUT_IDX         151
#define USB_EXTPHY_CLK_PAD_IN_IDX      152
#define USB_EXTPHY_SPEED_PAD_OUT_IDX   152
#define USB_EXTPHY_RCV_PAD_IN_IDX      153
#define USB_EXTPHY_SUSPND_PAD_OUT_IDX  153
#define USB_EXTPHY_VM_PAD_IN_IDX       154
#define USB_EXTPHY_VMO_PAD_OUT_IDX     154
#define USB_EXTPHY_VP_PAD_IN_IDX       155
#define USB_EXTPHY_VPO_PAD_OUT_IDX     155
#define LEDC_LS_SIG_OUT_PAD_OUT0_IDX   156
#define SD_CARD_DETECT_N_1_PAD_IN_IDX  157
#define LEDC_LS_SIG_OUT_PAD_OUT1_IDX   157
#define SD_CARD_DETECT_N_2_PAD_IN_IDX  158
#define LEDC_LS_SIG_OUT_PAD_OUT2_IDX   158
#define LEDC_LS_SIG_OUT_PAD_OUT3_IDX   159
#define LEDC_LS_SIG_OUT_PAD_OUT4_IDX   160
#define SD_CARD_WRITE_PRT_1_PAD_IN_IDX 161
#define LEDC_LS_SIG_OUT_PAD_OUT5_IDX   161
#define SD_CARD_WRITE_PRT_2_PAD_IN_IDX 162
#define LEDC_LS_SIG_OUT_PAD_OUT6_IDX   162
#define SD_DATA_STROBE_1_PAD_IN_IDX    163
#define LEDC_LS_SIG_OUT_PAD_OUT7_IDX   163
#define SD_DATA_STROBE_2_PAD_IN_IDX    164
#define REVMII_CLKMUX_SEL_PAD_OUT0_IDX 164
#define REVMII_CLKMUX_SEL_PAD_OUT1_IDX 165
#define REVMII_CLKMUX_SEL_PAD_OUT2_IDX 166
#define I3C_MST_SCL_PAD_IN_IDX         167
#define I3C_MST_SCL_PAD_OUT_IDX        167
#define I3C_MST_SDA_PAD_IN_IDX         168
#define I3C_MST_SDA_PAD_OUT_IDX        168
#define I3C_SLV_SCL_PAD_IN_IDX         169
#define I3C_SLV_SCL_PAD_OUT_IDX        169
#define I3C_SLV_SDA_PAD_IN_IDX         170
#define I3C_SLV_SDA_PAD_OUT_IDX        170
#define I3C_MST_SCL_PULLUP_EN_PAD_OUT_IDX 171
#define I3C_MST_SDA_PULLUP_EN_PAD_OUT_IDX 172
#define USB_JTAG_TDO_BRIDGE_PAD_IN_IDX 173
#define USB_JTAG_TDI_BRIDGE_PAD_OUT_IDX 173
#define USB_JTAG_TMS_BRIDGE_PAD_OUT_IDX 174
#define USB_JTAG_TCK_BRIDGE_PAD_OUT_IDX 175
#define USB_JTAG_TRST_BRIDGE_PAD_OUT_IDX 176
#define FAST_TEST_PAD_OUT_IDX          177
#define TWAI2_STANDBY_PAD_OUT_IDX      178
#define SD_RST_N_1_PAD_OUT_IDX         179
#define SD_RST_N_2_PAD_OUT_IDX         180
#define SD_CCMD_OD_PULLUP_EN_N_PAD_OUT_IDX 181
#define LCD_PCLK_PAD_OUT_IDX           182
#define CAM_CLK_PAD_OUT_IDX            183
#define LCD_H_ENABLE_PAD_OUT_IDX       184
#define LCD_H_SYNC_PAD_OUT_IDX         185
#define LCD_V_SYNC_PAD_OUT_IDX         186
#define LCD_DATA_OUT_PAD_OUT0_IDX      187
#define LCD_DATA_OUT_PAD_OUT1_IDX      188
#define LCD_DATA_OUT_PAD_OUT2_IDX      189
#define LCD_DATA_OUT_PAD_OUT3_IDX      190
#define CAM_PCLK_PAD_IN_IDX            191
#define LCD_DATA_OUT_PAD_OUT4_IDX      191
#define CAM_H_ENABLE_PAD_IN_IDX        192
#define LCD_DATA_OUT_PAD_OUT5_IDX      192
#define CAM_H_SYNC_PAD_IN_IDX          193
#define LCD_DATA_OUT_PAD_OUT6_IDX      193
#define CAM_V_SYNC_PAD_IN_IDX          194
#define LCD_DATA_OUT_PAD_OUT7_IDX      194
#define CAM_DATA_IN_PAD_IN0_IDX        195
#define LCD_DATA_OUT_PAD_OUT8_IDX      195
#define CAM_DATA_IN_PAD_IN1_IDX        196
#define LCD_DATA_OUT_PAD_OUT9_IDX      196
#define CAM_DATA_IN_PAD_IN2_IDX        197
#define LCD_DATA_OUT_PAD_OUT10_IDX     197
#define CAM_DATA_IN_PAD_IN3_IDX        198
#define LCD_DATA_OUT_PAD_OUT11_IDX     198
#define CAM_DATA_IN_PAD_IN4_IDX        199
#define LCD_DATA_OUT_PAD_OUT12_IDX     199
#define CAM_DATA_IN_PAD_IN5_IDX        200
#define LCD_DATA_OUT_PAD_OUT13_IDX     200
#define CAM_DATA_IN_PAD_IN6_IDX        201
#define LCD_DATA_OUT_PAD_OUT14_IDX     201
#define CAM_DATA_IN_PAD_IN7_IDX        202
#define LCD_DATA_OUT_PAD_OUT15_IDX     202
#define CAM_DATA_IN_PAD_IN8_IDX        203
#define LCD_DATA_OUT_PAD_OUT16_IDX     203
#define CAM_DATA_IN_PAD_IN9_IDX        204
#define LCD_DATA_OUT_PAD_OUT17_IDX     204
#define CAM_DATA_IN_PAD_IN10_IDX       205
#define LCD_DATA_OUT_PAD_OUT18_IDX     205
#define CAM_DATA_IN_PAD_IN11_IDX       206
#define LCD_DATA_OUT_PAD_OUT19_IDX     206
#define CAM_DATA_IN_PAD_IN12_IDX       207
#define LCD_DATA_OUT_PAD_OUT20_IDX     207
#define CAM_DATA_IN_PAD_IN13_IDX       208
#define LCD_DATA_OUT_PAD_OUT21_IDX     208
#define CAM_DATA_IN_PAD_IN14_IDX       209
#define LCD_DATA_OUT_PAD_OUT22_IDX     209
#define CAM_DATA_IN_PAD_IN15_IDX       210
#define LCD_DATA_OUT_PAD_OUT23_IDX     210
#define LCD_CS_PAD_OUT_IDX             211
#define LCD_DC_PAD_OUT_IDX             212
#define CORE_GPIO_IN_PAD_IN0_IDX       213
#define CORE_GPIO_OUT_PAD_OUT0_IDX     213
#define CORE_GPIO_IN_PAD_IN1_IDX       214
#define CORE_GPIO_OUT_PAD_OUT1_IDX     214
#define CORE_GPIO_IN_PAD_IN2_IDX       215
#define CORE_GPIO_OUT_PAD_OUT2_IDX     215
#define CORE_GPIO_IN_PAD_IN3_IDX       216
#define CORE_GPIO_OUT_PAD_OUT3_IDX     216
#define CORE_GPIO_IN_PAD_IN4_IDX       217
#define CORE_GPIO_OUT_PAD_OUT4_IDX     217
#define CORE_GPIO_IN_PAD_IN5_IDX       218
#define CORE_GPIO_OUT_PAD_OUT5_IDX     218
#define CORE_GPIO_IN_PAD_IN6_IDX       219
#define CORE_GPIO_OUT_PAD_OUT6_IDX     219
#define CORE_GPIO_IN_PAD_IN7_IDX       220
#define CORE_GPIO_OUT_PAD_OUT7_IDX     220
#define CORE_GPIO_IN_PAD_IN8_IDX       221
#define CORE_GPIO_OUT_PAD_OUT8_IDX     221
#define CORE_GPIO_IN_PAD_IN9_IDX       222
#define CORE_GPIO_OUT_PAD_OUT9_IDX     222
#define CORE_GPIO_IN_PAD_IN10_IDX      223
#define CORE_GPIO_OUT_PAD_OUT10_IDX    223
#define CORE_GPIO_IN_PAD_IN11_IDX      224
#define CORE_GPIO_OUT_PAD_OUT11_IDX    224
#define CORE_GPIO_IN_PAD_IN12_IDX      225
#define CORE_GPIO_OUT_PAD_OUT12_IDX    225
#define CORE_GPIO_IN_PAD_IN13_IDX      226
#define CORE_GPIO_OUT_PAD_OUT13_IDX    226
#define CORE_GPIO_IN_PAD_IN14_IDX      227
#define CORE_GPIO_OUT_PAD_OUT14_IDX    227
#define CORE_GPIO_IN_PAD_IN15_IDX      228
#define CORE_GPIO_OUT_PAD_OUT15_IDX    228
#define CORE_GPIO_IN_PAD_IN16_IDX      229
#define CORE_GPIO_OUT_PAD_OUT16_IDX    229
#define CORE_GPIO_IN_PAD_IN17_IDX      230
#define CORE_GPIO_OUT_PAD_OUT17_IDX    230
#define CORE_GPIO_IN_PAD_IN18_IDX      231
#define CORE_GPIO_OUT_PAD_OUT18_IDX    231
#define CORE_GPIO_IN_PAD_IN19_IDX      232
#define CORE_GPIO_OUT_PAD_OUT19_IDX    232
#define CORE_GPIO_IN_PAD_IN20_IDX      233
#define CORE_GPIO_OUT_PAD_OUT20_IDX    233
#define CORE_GPIO_IN_PAD_IN21_IDX      234
#define CORE_GPIO_OUT_PAD_OUT21_IDX    234
#define CORE_GPIO_IN_PAD_IN22_IDX      235
#define CORE_GPIO_OUT_PAD_OUT22_IDX    235
#define CORE_GPIO_IN_PAD_IN23_IDX      236
#define CORE_GPIO_OUT_PAD_OUT23_IDX    236
#define CORE_GPIO_IN_PAD_IN24_IDX      237
#define CORE_GPIO_OUT_PAD_OUT24_IDX    237
#define CORE_GPIO_IN_PAD_IN25_IDX      238
#define CORE_GPIO_OUT_PAD_OUT25_IDX    238
#define CORE_GPIO_IN_PAD_IN26_IDX      239
#define CORE_GPIO_OUT_PAD_OUT26_IDX    239
#define CORE_GPIO_IN_PAD_IN27_IDX      240
#define CORE_GPIO_OUT_PAD_OUT27_IDX    240
#define CORE_GPIO_IN_PAD_IN28_IDX      241
#define CORE_GPIO_OUT_PAD_OUT28_IDX    241
#define CORE_GPIO_IN_PAD_IN29_IDX      242
#define CORE_GPIO_OUT_PAD_OUT29_IDX    242
#define CORE_GPIO_IN_PAD_IN30_IDX      243
#define CORE_GPIO_OUT_PAD_OUT30_IDX    243
#define CORE_GPIO_IN_PAD_IN31_IDX      244
#define CORE_GPIO_OUT_PAD_OUT31_IDX    244
#define RMT_SIG_PAD_IN0_IDX            245
#define RMT_SIG_PAD_OUT0_IDX           245
#define RMT_SIG_PAD_IN1_IDX            246
#define RMT_SIG_PAD_OUT1_IDX           246
#define RMT_SIG_PAD_IN2_IDX            247
#define RMT_SIG_PAD_OUT2_IDX           247
#define RMT_SIG_PAD_IN3_IDX            248
#define RMT_SIG_PAD_OUT3_IDX           248
#define USB_SRP_DISCHRGVBUS_PAD_OUT_IDX 249
#define SIG_IN_FUNC_250_IDX            250
#define SIG_IN_FUNC250_IDX             250
#define SIG_IN_FUNC_251_IDX            251
#define SIG_IN_FUNC251_IDX             251
#define SIG_IN_FUNC_252_IDX            252
#define SIG_IN_FUNC252_IDX             252
#define SIG_IN_FUNC_253_IDX            253
#define SIG_IN_FUNC253_IDX             253
#define SIG_IN_FUNC_254_IDX            254
#define SIG_IN_FUNC254_IDX             254
#define SIG_IN_FUNC_255_IDX            255
#define SIG_IN_FUNC255_IDX             255
#endif  /* _SOC_GPIO_SIG_MAP_H_ */