# Espressif Camera Sensors Component

This component provides drivers for camera sensors that can be used on the ESP32 series chips.

[![Component Registry](https://components.espressif.com/components/espressif/esp_cam_sensor/badge.svg)](https://components.espressif.com/components/espressif/esp_cam_sensor)

## Supported Camera Sensors

| model   | max resolution | output interface | output format                                                | Len Size |
| ------- | -------------- | ---------- | ------------------------------------------------------------ | -------- |
| SC2336  | 1920 x 1080    | MIPI & DVP      | 8/10-bit Raw RGB data | 1/3"     |
| OV5645  | 2592 x 1944    | MIPI      | 8/10-bit Raw RGB data<br/>RGB565<br/>YUV/YCbCr422<br/>YUV420 | 1/4"     |
| OV5647  | 2592 x 1944    | MIPI & DVP      | 8/10-bit Raw RGB data | 1/4"     |
