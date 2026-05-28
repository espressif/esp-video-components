快速入门
=======================================

:link_to_translation:`en:[English]`

.. important::
  在使用 esp-video-components 前，请仔细阅读 :doc:`../disclaimer`，并遵循其中的各项条款和注意事项。

概述
-------------------------------------------
esp-video-components 是乐鑫官方的相机系统开发与应用框架。它主要包括以下几个子组件：

- `esp_cam_sensor <https://github.com/espressif/esp-video-components/tree/master/esp_cam_sensor>`_：该组件提供包括 MIPI-CSI、DVP、SPI 三种相机传感器的底层驱动程序。
- `esp_ipa <https://github.com/espressif/esp-video-components/tree/master/esp_ipa>`_：该组件提供图像处理算法的库文件。通常，对于输出 ``RAW`` 数据的相机传感器，需要 AE、AWB 等算法的实时控制，才能得到较为清晰的图像。
- `esp_sccb_intf <https://github.com/espressif/esp-video-components/tree/master/esp_sccb_intf>`_：该组件提供相机控制总线的驱动程序。
- `esp_video <https://github.com/espressif/esp-video-components/tree/master/esp_video>`_：该组件依赖 esp_cam_sensor、esp_ipa、esp_h264 组件，实现兼容 Linux V4L2 标准的 API。用户可以在工程中添加该组件来快速添加需要的视觉功能。

根据相机传感器的数据接口，esp-video-components 支持的芯片有：

.. list-table::
  :header-rows: 1

  * - SoC
    - MIPI-CSI
    - DVP
    - USB
    - SPI
  * - ESP32-P4
    - supported
    - supported
    - supported
    - supported
  * - ESP32-S3
    -
    - supported
    - supported
    - supported
  * - ESP32-C3
    -
    - 
    - 
    - supported
  * - ESP32-C5
    -
    - 
    - 
    - supported
  * - ESP32-C6
    -
    - 
    - 
    - supported
  * - ESP32-C61
    -
    - 
    - 
    - supported

系统架构
----------------------------------------

.. figure:: ../../_static/get_started/esp-video-framework.png
   :align: center
   :alt: esp_video 系统框架
   :figclass: align-center

   esp_video 系统框架

从组织结构来看，分为五层：PC 端调试分析工具、应用层、应用框架层、设备层、内核层。

- PC 端调试分析工具，用于预览图像、分析图像质量、在线调试图像系统的参数、标定图像处理单元的参数。
- 应用层主要是调用 esp_video 提供的各种 API 进行应用开发。
- esp_video 是应用框架层，它不仅管理设备层的各个设备和库，并且向应用程序提供高效、方便的 API 接口。作为整个系统的中间件，它对上实现统一的接口，对下层组件也提供统一的标准以方便各种设备的兼容控制。
- 设备层，包括相机传感器设备、图像处理控制算法库的底层实现，并以统一的接口向上层提供调用接口。
- 内核层，是 ESP-IDF 提供的操作系统以及设备驱动代码。

开发板概述
-------------------------------------------

有一些拥有相机接口的开发板可以用来测试。开发者可以参考这些开发板的相机接口来设计硬件。

.. only:: esp32p4

    - `ESP32-P4-Function-EV-Board <https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4x-function-ev-board/index.html>`_
    - `ESP32-P4-EYE <https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4x-eye/index.html>`_

编译第一个工程
-------------------------------------------

安装 ESP-IDF
^^^^^^^^^^^^^^^^^^^^^^^^

请参考《ESP-IDF 编程指南》的 `快速入门 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32p4/get-started/index.html#id1>`__ 章节配置电脑。如果你是首次使用 ESP-IDF，建议先通过 `hello_world <https://github.com/espressif/esp-idf/tree/master/examples/get-started/hello_world>`_ 示例熟悉基本的开发流程。

运行示例
^^^^^^^^^^^^^^^^^^^^^^^^

esp-video-components 包含了常用的 `测试示例 <https://github.com/espressif/esp-video-components/tree/master/esp_video/examples>`_。通过运行 ``git clone --recursive https://github.com/espressif/esp-video-components.git`` 命令获取该仓库的源码，然后参考 **capture_stream** 示例中的 README 文件来编译并运行该示例。

.. note::

    如果无法通过 github 获取 esp-video-components 中最新的组件，用户可以通过 `jihulab <https://jihulab.com/esp-mirror/espressif/esp-video-components>`_ 来获取备份的组件。

添加组件到自定义工程
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

在工程目录的根文件夹下执行 ``idf.py add-dependency esp_video`` 命令可以添加 esp_video 组件。需要改变组件的源码时，可以参考 capture_stream 示例的 `main/idf_component.yml <https://github.com/espressif/esp-video-components/blob/master/esp_video/examples/capture_stream/main/idf_component.yml>`_ 文件，通过 ``override_path`` 指令指定本地组件的路径。

更多关于组件管理与使用的方法，请参考 `组件管理和使用 <https://docs.espressif.com/projects/esp-techpedia/zh_CN/latest/esp-friends/advanced-development/component-management.html>`_。