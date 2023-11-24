#pragma once
#include "mipi_csi.h"

int isp_init(uint32_t frame_width, uint32_t frame_height, pixformat_t in_type, pixformat_t out_type, bool isp_enable, void *sensor_info);
