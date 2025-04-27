关于 ESP-Video-Components 开发参考
=============================================

:link_to_translation:`en:[English]`

`ESP-Video-Components <https://github.com/espressif/esp-video-components>`_ 是乐鑫开发的相机应用框架。
本指南为 ESP32 相机应用开发框架 ESP-Video-Components 的配套文档。

ESP-Video-Components 提供的功能主要包括：

- 包含 MIPI、DVP、SPI、USB 四种相机接口的外设驱动程序
- 允许同时使用多个相机传感器
- 支持通过 POSIX API 和 `Linux V4L2 API <https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/v4l2.html>`_ 来获取来自相机传感器的数据流
- 支持对 ISP、Codec 设备的访问控制
- 支持自动调用图像处理算法库 esp-ipa
- 支持通过 JSON 文件控制 esp-ipa 中相关算法的参数

通过使用 ESP-Video-Components 组件，用户可以快捷地构建视觉应用。
