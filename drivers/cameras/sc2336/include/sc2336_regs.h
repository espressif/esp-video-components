/*
 * SC2336 register definitions.
 */
#ifndef __SC2336_REG_REGS_H__
#define __SC2336_REG_REGS_H__

#define SC2336_REG_END		0xffff
#define SC2336_REG_DELAY	0xfffe

/* sc2336 registers */
#define SC2336_REG_SENSOR_ID_H             0x3107
#define SC2336_REG_SENSOR_ID_L             0x3108

#define SC2336_REG_GROUP_HOLD              0x3812

#define SC2336_REG_DIG_GAIN                0x3e06
#define SC2336_REG_DIG_FINE_GAIN           0x3e07
#define SC2336_REG_ANG_GAIN                0x3e09

#define SC2336_REG_SHUTTER_TIME_H          0x3e00
#define SC2336_REG_SHUTTER_TIME_M          0x3e01
#define SC2336_REG_SHUTTER_TIME_L          0x3e02

#define SC2336_REG_TOTAL_WIDTH_H           0x320c // HTS,line width
#define SC2336_REG_TOTAL_WIDTH_L           0x320d
#define SC2336_REG_TOTAL_HEIGHT_H          0x320e // VTS,frame hight
#define SC2336_REG_TOTAL_HEIGHT_L          0x320f

#define SC2336_REG_OUT_WIDTH_H             0x3208 // width
#define SC2336_REG_OUT_WIDTH_L             0x3209
#define SC2336_REG_OUT_HEIHT_H             0x320a // hight
#define SC2336_REG_OUT_HEIHT_L             0x320b

#define SC2336_REG_OUT_START_PIXEL_H       0x3210 // start X
#define SC2336_REG_OUT_START_PIXEL_L       0x3211
#define SC2336_REG_OUT_START_LINE_H        0x3212 // start Y
#define SC2336_REG_OUT_START_LINE_L        0x3213

#define SC2336_REG_FLIP_MIRROR             0x3221
#define SC2336_REG_SLEEP_MODE              0x0100
#endif