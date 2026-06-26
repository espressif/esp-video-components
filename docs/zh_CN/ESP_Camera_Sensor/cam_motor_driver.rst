.. _camera_motor_driver:

相机自动对焦电机驱动开发
========================

:link_to_translation:`en:[English]`

概述
----

相机自动对焦电机设备是用于实现自动对焦功能的专用设备，通常通过 I2C 接口进行控制。如下图所示，图像信号处理器 (Image Signal Processor, ISP) 负责统计每帧数据的清晰度信息，并将统计数据上报给图像处理算法 (Image Processing Algorithms, IPA) 处理；然后，IPA 对统计数据进行分析，控制自动对焦电机移动，完成对焦。

.. blockdiag::
    :caption: 相机自动对焦电机数据流
    :align: center

    blockdiag Camera-motor-Data-Flow {

        # global attributes
        node_height = 60;
        node_width = 100;
        span_width = 50;
        span_height = 20;
        default_shape = roundedbox;

        # labels of diagram nodes
        SENSOR [label="传感器\n帧数据", fontsize=12];
        ISP [label="ISP\n流水线", fontsize=12];
        IPA [label="IPA", fontsize=12];
        MOTOR [label="电机", fontsize=12];

        # node connections + labels
        SENSOR -> ISP -> IPA;
        IPA -> MOTOR [label="参数", fontsize=8];
    }

驱动架构
--------

相机自动对焦电机的驱动程序主要代码结构包括：

.. code-block:: none

    ├── include
    │   └── dw9714_types.h
    │   └── dw9714.h
    ├── private_include
    │   └── dw9714_settings.h    # 电机驱动程序的初始化配置
    └── Kconfig.dw9714
    └── dw9714.c                 # 电机驱动程序的实现

.. attention::

    - 可以找一款规格相近的相机自动对焦电机，参考其驱动程序进行修改适配。
    - 自动对焦电机与相机模组的硬件连接方式有很多种，可以根据实际的硬件需求编写驱动程序。建议尽量使用相机传感器上的管脚，并与相机传感器共用 SCCB 总线，以节省芯片管脚。

功能概览
--------

相机自动对焦电机的驱动程序提供下述功能：

- :ref:`设备发现 <device_find>` — 包括对电机设备进行读写检查，以及如何在完成工作后回收资源。
- :ref:`参数控制 <para_ctrl>` — 包括查询设备支持的参数信息，以及设置和查询参数值。
- :ref:`IO 控制 <io_ctrl>` — 包括控制设备的重启、休眠等功能。
- :ref:`格式控制 <fmt-management>` — 描述了如何枚举该对焦电机支持的格式，以及设置和查询格式。
- :ref:`查询名称 <motor-get-name>` — 描述了在使用中查询当前设备的名称。
- :ref:`Kconfig 选项 <motor-kconfig-options>` — 列出了支持的 Kconfig 选项，这些选项可以对驱动程序产生不同影响。

.. _device_find:

设备发现
^^^^^^^^

若电机设备已正确连接到芯片上，并且驱动程序正常，驱动程序将自动对电机设备进行 SCCB 读写检查，并返回 ``esp_cam_motor_device_t`` 来表示该设备。在调用任何其他 ``esp_cam_motor_`` API 之前必须先获取该设备的描述符。

通过电机专用接口探测指定设备
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

以 DW9714 为例，在应用代码中包含 ``dw9714.h`` 头文件，然后通过 ``dw9714_detect()`` 函数探测该总线上是否连接了 DW9714 电机。

探测指定 SCCB 总线上的所有设备
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

相机自动对焦电机驱动提供了一个数组，可以用来探测指定的 SCCB 总线上连接的所有自动对焦电机设备。请通过 ``esp_cam_motor_detect_get_array()`` 获取该数组的起止指针，以兼容不同的设备探测方式。

.. code-block:: c

    esp_cam_motor_detect_fn_t *array_start = NULL;
    esp_cam_motor_detect_fn_t *array_end = NULL;
    esp_cam_motor_detect_get_array(&array_start, &array_end);
    for (esp_cam_motor_detect_fn_t *p = array_start; p < array_end; p++) {
        esp_cam_motor_config_t cfg = {0};
        esp_cam_motor_device_t *motor_dev;
        const esp_video_init_cam_motor_config_t *cm = config->cam_motor;
        cfg.sccb_handle = create_sccb_device(sccb_mark, ESP_VIDEO_INIT_MOTOR_SCCB, &cm->sccb_config, p->sccb_addr);
        if (!cfg.sccb_handle) {
            return ESP_FAIL;
        }

        cfg.reset_pin = cm->reset_pin;
        cfg.pwdn_pin = cm->pwdn_pin;
        cfg.signal_pin = cm->signal_pin;
        motor_dev = (*(p->detect))((void *)&cfg);
        if (!motor_dev) {
            ESP_LOGE(TAG, "failed to detect motor with address=%x", p->sccb_addr);
            continue;
        }
        break;
    }

删除设备
~~~~~~~~

如果不再需要使用电机，可以调用 ``esp_cam_motor_del_dev()`` 来回收资源。

.. _para_ctrl:

参数控制
^^^^^^^^

使用以下函数，可以查询和设置相机自动对焦电机设备的参数：

- ``esp_cam_motor_query_para_desc()`` 用于查询参数的类型、取值范围。
- ``esp_cam_motor_get_para_value()`` 用于查询参数的当前值。
- ``esp_cam_motor_set_para_value()`` 用于设置参数的值。

.. _io_ctrl:

IO 控制
^^^^^^^

电机的输入输出控制可以通过调用 ``esp_cam_motor_ioctl()`` 来实现。

.. _fmt-management:

格式控制
^^^^^^^^

相机自动对焦电机设备的格式信息包括：工作模式、步进信息、默认位置等信息。所有由初始化寄存器列表决定的信息均通过 ``esp_cam_motor_format_t`` 结构体来描述。通过设置和查询该结构体的信息，IPA 可以更加清楚地控制对焦电机完成对焦功能。

- ``esp_cam_motor_query_formats()`` 用于查询支持的格式信息。
- ``esp_cam_motor_get_format()`` 用于查询正在使用的格式信息。
- ``esp_cam_motor_set_format()`` 用于设置使用的格式信息。

.. attention::

    如需调整自动对焦电机的工作模式，需要注意以下问题：
    
    - 振铃效应：移动电机时由于有惯性，会存在惯性振动。通常自动对焦电机驱动 IC 上带有抑制振动的控制模式，如 DW9714 的双级控制 (Dual Level Control, DLC) 模式。
    - 边界控制：移动电机时由于有惯性，接近行程边缘时需要进行边界控制，否则可能撞到边缘。若电机上电后，默认将其移动到距离感光矩阵最远处的地方，设备断电将可能导致电机迅速向初始化位置移动，从而触碰边缘，发出异响，此为正常现象。
    - 步长控制：不要一次移动太大的步长，而是分多次以平滑的小步长逐步移动到目标位置。编写完驱动后，可以启动一个定时器，在定时器回调中控制步长递增或递减，然后通过可预览图像的示例程序，观察电机的移动是否平滑。
    - 考虑到电机的重力，其在水平、垂直朝上、垂直朝下等不同的方向上的重力不一样，需要的步进值也不一样。若有方向传感器，可结合其方向信息进行优化。

.. _motor-get-name:

查询名称
^^^^^^^^

通过调用 ``esp_cam_motor_get_name()`` 接口来查询当前电机设备的名称。

.. _motor-kconfig-options:

Kconfig 选项
^^^^^^^^^^^^

每一个自动对焦电机都有一个配置文件，以 DW9714 为例，对应的是 ``esp_cam_sensor/motors/dw9714/Kconfig.dw9714`` 文件。通过该文件可以配置电机默认加载的格式、是否启用自动探测功能等。

此外，还有一些通用的配置选项：

- ``CONFIG_CAMERA_SENSOR_MOTOR_DETECT_METHOD`` 用于选择相机传感器与对焦电机的设备探测方式，支持以下两种模式：

  - ``CONFIG_CAMERA_SENSOR_MOTOR_DETECT_METHOD_DYNAMIC_LINK`` （默认）：通过 ``esp_cam_motor_detect_fn`` 链接器段，将当前固件中所有可用的自动对焦电机探测函数及其配置数据自动纳入固件。即使应用代码未显式引用这些函数，链接器也会保留它们，从而支持自动探测所有已启用电机，但固件体积相对较大。
  - ``CONFIG_CAMERA_SENSOR_MOTOR_DETECT_METHOD_STATIC_STORE``：仅将应用实际引用的自动对焦电机探测函数及其配置数据以 C 语言数组形式静态存储在 flash 中。未被引用的探测函数和数据可由链接器从最终固件中排除，从而减小二进制体积。

API 参考
--------

.. include-build-file:: inc/esp_cam_motor.inc
