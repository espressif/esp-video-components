技术选型
========

:link_to_translation:`en:[English]`

本文档主要介绍如何选择合适的乐鑫硬件产品、Camera sensor 型号以及项目初期的准备。

.. important::
  如果您在选择乐鑫硬件产品、 软件方案中有任何问题，请联系 `乐鑫商务 <https://www.espressif.com/zh-hans/contact-us/sales-questions>`_ 或者 `技术支持 <https://www.espressif.com/zh-hans/contact-us/technical-inquiries>`_。

硬件选型
------------

在开始使用 ESP-Video 之前，您需要选择一款合适的乐鑫芯片集成到您的产品中。硬件选型是一个复杂的过程，需要考虑多方面的因素，如功能、功耗、成本、尺寸等。请阅读下面内容帮助您选型硬件。

.. list::

  - `产品选型工具 <https://products.espressif.com/#/product-selector?language=zh&names=>`_ 可以帮助您了解不同乐鑫产品的硬件区别。
  - `技术规格书 <https://www.espressif.com/zh-hans/support/documents/technical-documents?keys=&field_download_document_type_tid%5B%5D=510>`_ 可以帮助您了解该芯片/模组所支持的硬件能力。
  - `硬件选型入门指导 <https://docs.espressif.com/projects/esp-techpedia/zh_CN/latest/esp-friends/get-started/board-selection.html>`_ 可以帮助您简要对比芯片差别，了解芯片、模组和开发板的差别以及选择指南。

硬件选型建议：

- 对于输出 RAW 格式的 sensor，必须选择带 ISP 的 ESP32 芯片，如 ESP32-P4。
- 对于输出 YUV 或者 RGB 的 sensor，可以选择 ESP32 所有拥有相机接口的芯片。

软件方案选型
--------------------

- 图传项目。图像的分辨率不大于 QVGA 的项目，可以选择拥有 SPI、DVP 的芯片；若图像的分辨率大于 720p 的可以选择拥有 USB、MIPI-CSI 接口的芯片。
- 视频流传输项目。若对帧率的要求比较高，推荐选择拥有 USB、MIPI-CSI 接口的芯片。
- AI 项目。推荐选择带 AI 加速指令集的芯片。
