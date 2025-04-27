.. _camera_sensor_test:

Camera sensor 调试说明
===========================

:link_to_translation:`en:[English]`

本文档主要介绍 Sensor 调试过程中的驱动开发流程及注意事项。Sensor 调试流程可按照下图所示流程实施：

.. figure:: ../../_static/Sensor_drivers/sensor_tuning_flow_en.jpg
   :alt: auth_pages
   :figclass: align-center

   Sensor tuning flow

准备材料
------------------

确认技术参数
^^^^^^^^^^^^^^^^^^^^

开发 sensor 的驱动程序前，先获取 sensor 的 datasheet 以及设计指南，了解 sensor 的帧率、输出尺寸、数据格式是否符合 SOC 的接口需求，并按照如下格式向 Sensor 原厂申请初始化配置列表：

.. code-block:: none

    Sensor name: OV2710
    SOC name: ESP32-P4
    Output size: 640x480
    FPS: 30
    Input Clock: 24MHz
    Output mode: Linear mode
    Interface: MIPI 2 data lanes
    Output format: RGB RAW8

获取 Sensor Initialize Settings，一般至少要准备最大规格和标准分辨率两种序列。

.. attention::

    对于使用 MIPI 接口的 sensor, 注意 MIPI 的一些参数需要符合接收端的需求。要求每个数据线的速率不大于 1.0 Gbps，无 line sync packet，MIPI Clock 工作在 non-continuous 模式。

硬件连接
^^^^^^^^^^^^^^^^^^^^

根据使用的相机接口，参考官方开发板的电路设计，申请 sensor 模组。重点需要关注的几个方面包括：

- Sensor 的电源需求，包括 AVDD、DVDD、DOVDD。
- Sensor 的输入时钟 XCLK 是使用外部晶振还是使用 SOC 的输出时钟。
- Sensor Power down(PWDN)、Reset(RST)管脚是否与 Sensor 进行连接。

不同接口的 sensor 模组与开发板的连接的方式不同。

- SPI 接口的相机传感器与开发板连接的示意图如下：

.. code-block:: none

    ------------------                      ------------------
    | Camera sensor  |                      | SoC            |
    |                |      Data Link       |                |
    |          Data1 |--------------------->| Data1          |
    |          Data0 |--------------------->| Data0          |
    |                |                      |                |
    | XCLK(Optional) |<---------------------| XCLK           |
    |          VSYNC |--------------------->| VSYNC          |
    |          PCLK  |--------------------->| PCLK           |

    ------------------                      ------------------
    | I2C Slave      |  SCCB Control Link   | I2C Master     |
    |            SCL |<---------------------| SCL            |
    |            SDA |<-------------------->| SDA            |
    ------------------                      ------------------

- DVP 接口的相机传感器与开发板连接的示意图如下：

.. code-block:: none

    ------------------                      ------------------
    | Camera sensor  |                      | SoC            |
    |                |      Data Link       |                |
    |          Data7 |--------------------->| Data7          |
    |          Data6 |--------------------->| Data6          |
    |          Data5 |--------------------->| Data5          |
    |          Data4 |--------------------->| Data4          |
    |          Data3 |--------------------->| Data3          |
    |          Data2 |--------------------->| Data2          |
    |          Data1 |--------------------->| Data1          |
    |          Data0 |--------------------->| Data0          |
    |                |                      |                |
    |          PCLK  |--------------------->| PCLK           |
    |          HREF  |--------------------->| HREF           |
    |          VSYNC |--------------------->| VSYNC          |
    |                |                      |                |

    ------------------                      ------------------
    | I2C Slave      |      Control Link    | I2C Master     |
    |            SCL |<---------------------| SCL            |
    |            SDA |<-------------------->| SDA            |
    ------------------                      ------------------

- MIPI 接口的相机传感器与开发板连接的示意图如下：

.. code-block:: none

    ------------------                      ------------------
    | CSI Transmitter|                      | CSI Receiver   |
    |                |      Data Link       |                |
    |          Data1+|--------------------->|Data1+          |
    |          Data1-|--------------------->|Data1-          |
    |          Data0+|--------------------->|Data0+          |
    |          Data0-|--------------------->|Data0-          |
    |                |                      |                |
    |          CLK  +|--------------------->|CLK  +          |
    |          CLK  -|--------------------->|CLK  -          |
    |                |                      |                |

    ------------------                      ------------------
    | CCI Slave      |      Control Link    | CCI Master     |
    |            SCL |<---------------------| SCL            |
    |            SDA |<-------------------->| SDA            |
    ------------------                      ------------------

获取 Sensor ID
----------------------------

.. _cam-sensor-driver-prepare:

准备 Sensor 驱动
^^^^^^^^^^^^^^^^^^^^

通过软件程序查询 sensor 的 ID，可以验证硬件连接是否正确、以及 I2C 通信是否正常。这需要先实现 Sensor 的驱动程序。

可以基于一款规格相近 Sensor 的驱动进行修改，尝试编写出 Sensor 驱动。更多介绍可以参考 `add new driver <https://github.com/espressif/esp-video-components/tree/master/esp_cam_sensor#steps-to-add-a-new-camera-sensor-driver>`_ 。

.. important::
  esp_cam_sensor 组件中默认使用 7bit 的 I2C 地址。

测试 I2C 通信
^^^^^^^^^^^^^^^^^^^^

在硬件连接正确，设备的管脚配置正确的情况下，运行 `capture stream <https://github.com/espressif/esp-video-components/tree/master/esp_video/examples/capture_stream>`_ 示例，若显示如下 log 则说明测试正常。

.. code-block:: none

    I (5310) example_init_video: MIPI-CSI camera sensor I2C port=0, scl_pin=8, sda_pin=7, freq=100000
    I (5320) ov5645: Detected Camera sensor PID=0x5645

.. attention::

    - 如果需要配置 sensor 的 PWDN 和 RST 管脚，推荐在应用层代码中控制这些接口的电平。
    - 如果使用 SOC 的管脚输出 XCLK 时钟信号，请参考 `xclk_generator <https://github.com/espressif/esp-video-components/tree/master/esp_cam_sensor/test_apps/xclk_generator>`_ 示例中的 API 来生成该信号。
    - 在管脚配置正确，sensor 供电正常，XCLK 正常的情况下，也可以尝试使用 `i2c_tools <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/i2c/i2c_tools>`_ 示例来探测设备的 I2C 地址信息。

完善 Format 信息
----------------------

参考 sensor 的技术手册以及 sensor 厂家提供的初始化列表，完善代表 sensor 格式信息的结构体 ``esp_cam_sensor_format_t`` 。以 OV2710 为例，该结构体包含的信息有：

.. code-block:: c

    static const esp_cam_sensor_format_t ov2710_format_info[] = {
        /* For MIPI */
        {
            .name = "MIPI_1lane_24Minput_RAW10_1920x1080_25fps",
            .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
            .port = ESP_CAM_SENSOR_MIPI_CSI,
            .xclk = OV2710_MCLK,
            .width = 1920,
            .height = 1080,
            .regs = init_reglist_MIPI_1lane_1920_1080_25fps,
            .regs_size = ARRAY_SIZE(init_reglist_MIPI_1lane_1920_1080_25fps),
            .fps = 25,
            .isp_info = &ov2710_isp_info[0],
            .mipi_info = {
                .mipi_clk = 800000000,
                .lane_num = 1,
                .line_sync_en = false,
            },
            .reserved = NULL,
        }
    };

.. attention::

    - 对于 MIPI 接口的 sensor，MIPI 接口的信息可以咨询 sensor 的 FAE 或者查看原厂提供的寄存器初始化列表信息。
    - 通常 sensor 的寄存器初始化列表会按照 soft reset -> standby enable -> sensor format init 的顺序进行配置，然后在调用 ``sensor_set_stream()`` 时切换到 standby disable 模式来输出数据。

在测试模式下采集图像
---------------------------

sensor 的测试模式可以输出有一定规律的数据，这有助于帮助检查接收端的配置是否与发送端匹配。开发人员参考 sensor 的技术手册实现该 sensor 的使能测试模式的接口，然后通过编译一些可以预览图像的 `examples <https://github.com/espressif/esp-video-components/tree/master/esp_video/examples>`_ ，查看该测试模式下的图像。以 OV2710 为例，需要以下步骤来完成：

- 实现使能测试模式的接口 ``ov2710_set_test_pattern()`` 。
- 在 ``ov2710_set_stream()`` 函数返回前，调用 ``ov2710_set_test_pattern()`` 启用测试模式。
- 编译示例，预览测试模式的图像。

.. attention::

    - 一些 YUV sensor 尽管可以正常被初始化并在接收端接收到数据，但是最终显示的图像的颜色异常，此时应检查 sensor 的信号极性、管脚驱动能力，数据大小端配置以及 YUYV 发送顺序。
    - 对于 RAW sensor，格式信息中还包括 ISP 信息，不正确的 ISP 信息将导致图像异常，但是不影响接收数据。若测试模式下编码后的图像异常，可以尝试更改 ``esp_cam_sensor_isp_info_t`` 中的 ``bayer_type`` 参数，直到测试图像与 sensor 的 datasheet 的一致。另外，sensor 的 vflip 和 hmirror 功能会影响 sensor 的 bayer_type 参数。
    - 推荐运行 web 示例来在网页上预览图像。也可以通过 LCD、USB 等接口导出图像进行预览。

完善 ISP 功能
------------------

对于 RAW sensor，还需提供正确的控制信息供 ISP 外设来使用。

其中关于 AE 控制信息，请参考 sensor 的技术手册按照如下顺序实现相关的接口：

- sensor_set_exp_val()。该接口用于设置 sensor 的曝光时间。
- sensor_set_total_gain_val()。该接口用于设置 sensor 的增益信息。sensor 的增益分为数字增益、模拟增益，该接口设置总增益，总增益 = 模拟增益 x 数字增益。
- sensor_query_para_desc()。该接口用于查询 sensor 的曝光、增益取值范围，默认值等信息。
- sensor_set_para_value()。该接口用于设置 sensor 的 ISP 控制参数。

.. attention::

    - 一些 RAW sensor 可以输出实时的黑电平、亮度的统计数据，它们需要特殊的 API，并完善 ISP 相关的其他参数，请联系乐鑫的技术人员完成技术支持。
    - sensor 默认的 ISP 控制信息，比如默认的曝光值、增益值，请确认与对应的寄存器列表信息一致。

测试 AE 配置
------------------

在完善 AE 配置信息后，可以启用一个定时器，在定时器中调用设置曝光、增益控制的接口，让配置参数递增或者递减变化，在可以预览图像的 `examples <https://github.com/espressif/esp-video-components/tree/master/esp_video/examples>`_ ，查看图像的亮度是否平滑地过渡。

协助 ISP tuning 人员进行图像质量调优
------------------------------------------------------

验证噪声和颜色
^^^^^^^^^^^^^^^^^^^^

sensor 驱动开发人员需要配合 ISP 调试人员验证不同增益下的噪声情况，完善增益的控制数组以及曝光时间的控制，平衡亮度变化的平滑性和噪声情况。此外，对于一些拥有部分 ISP 的 sensor，其 ISP 相关的功能，如 BLC、DPC 功能，也需要配合 ISP 调试人员进行验证。

图像质量调优
^^^^^^^^^^^^^^^^^^^^

协助图像质量调试人员优化图像质量。
