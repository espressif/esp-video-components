# Espressif Camera Sensors Component

This component provides drivers for camera sensors that can be used on the ESP32 series chips.

[![Component Registry](https://components.espressif.com/components/espressif/esp_cam_sensor/badge.svg)](https://components.espressif.com/components/espressif/esp_cam_sensor)

## Supported Camera Sensors

| model   | max resolution | output interface | output format                                                | Len Size |
| ------- | -------------- | ---------- | ------------------------------------------------------------ | -------- |
| SC2336  | 1920 x 1080    | MIPI & DVP      | 8/10-bit Raw RGB data | 1/3"     |
| SC202CS(SC2356) | 1600 x 1200    | MIPI      | 8/10-bit Raw RGB data | 1/5.1"     |
| OV5645  | 2592 x 1944    | MIPI      | 8/10-bit Raw RGB data<br/>RGB565<br/>YUV/YCbCr422<br/>YUV420 | 1/4"     |
| OV5647  | 2592 x 1944    | MIPI & DVP      | 8/10-bit Raw RGB data | 1/4"     |
| OV2640  | 1600 x 1200    | DVP | 8/10-bit Raw RGB data<br/>JPEG compression<br/>YUV/YCbCr422<br/>RGB565 | 1/4"     |
| GC0308  | 640 x 480    | DVP | Grayscale<br/>YCbCr422<br/>RGB565 | 1/6.5"     |
| SC101IOT  | 1280 x 720    | DVP | YCbCr422<br/>8/10-bit Raw RGB data | 1/4.2"     |
| SC030IOT  | 640 x 480    | DVP | YCbCr422<br/>8bit Raw RGB data | 1/6.5"     |
