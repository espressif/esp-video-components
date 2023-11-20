# Sensor Lib

This directory contains the configuration of the sensors(camera sensors\VCM sensors\flash led sensors\IR) needed by Libcamera.

### Supported Sensor

| sensor | max resolution | color type | shutter Type | output format                                                | optical size | pixel size | Interface |
| ------ | -------------- | ---------- | ------------ | ------------------------------------------------------------ | ------------ | ---------- | ------------- |
| SC2336 | 1928*1088 | color | Rolling | 8/10-bit Raw RGB data | 1/3" | 2.7 µm x 2.7 µm | MIPI & DVP |
| OV5640 | 2592 x 1944 | color      | Rolling | YUV(422/420)/YCbCr422 RGB565/555 8-bit compressed data 8/10-bit Raw RGB data | 1/4"         | 1.4 µm x 1.4 µm | MIPI |
| OV5647 | 2592 x 1944 | color      | Rolling | 8/10-bit Raw RGB data | 1/4"        | 1.4 µm x 1.4 µm | MIPI |

Note that some sensors can use one of the two camera interfaces: DVP or MIPI-CSI, but the drivers they need may be different, so the list only lists the camera interface when it is used on ESP32.

refer:

[Raspberry Pi Documentation - Camera](https://www.raspberrypi.com/documentation/accessories/camera.html)