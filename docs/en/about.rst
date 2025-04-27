About
==========

:link_to_translation:`zh_CN:[中文]`

`ESP-Video-Components <https://github.com/espressif/esp-video-components>`_ is a camera application framework developed by Espressif.
This guide is a companion document to the ESP-Video-Components, a camera application development framework for ESP32 chips.

The features provided by ESP-Video-Components mainly include:

- Peripheral drivers for four camera interfaces: MIPI, DVP, SPI, and USB
- Allows the use of multiple camera sensors at the same time
- Supports obtaining data streams from camera sensors through POSIX API and `Linux V4L2 API <https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/v4l2.html>`_
- Supports access control for ISP and Codec devices
- Supports automatic calling of the image processing algorithm library(esp-ipa)
- Supports controlling the parameters of related algorithms in esp-ipa through JSON files

By using ESP-Video-Components, users can quickly build visual applications.
