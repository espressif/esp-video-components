.. _camera_motor_driver:

Camera Motor 驱动
===========================

:link_to_translation:`en:[English]`

概述
------------------

相机自动对焦传感器是用于实现自动对焦功能的专用设备，通常通过 I2C 接口进行控制。如下图所示，ISP 负责统计每帧数据的清晰度信息，并将统计数据上报给 IPA；然后，IPA 对统计数据进行分析，控制 camera motor 移动，完成对焦。

.. blockdiag::
    :caption: Camera motor Data Flow
    :align: center

    blockdiag Camera-motor-Data-Flow {

        # global attributes
        node_height = 60;
        node_width = 100;
        span_width = 50;
        span_height = 20;
        default_shape = roundedbox;

        # labels of diagram nodes
        SENSOR [label="Sensor\n frames", fontsize=12];
        ISP [label="ISP\npipelines", fontsize=12];
        IPA [label="IPA", fontsize=12];
        MOTOR [label="Motor", fontsize=12];

        # node connections + labels
        SENSOR -> ISP -> IPA;
        IPA -> MOTOR [label="Params", fontsize=8];
    }

驱动代码结构
----------------------------

camera motor 的驱动代码结构可以描述如下：

.. code-block:: none

    ├── include
    │   └── dw9714_types.h
    │   └── dw9714.h
    ├── private_include
    │   └── dw9714_regs.h
    │   └── dw9714_settings.h           Initialize settings of the sensor driver
    └── Kconfig.dw9714
    └── dw9714.c                        Implementation of the sensor driver

.. attention::

    - 如需新增 camera motor 的驱动程序，可以基于一款规格相近的 camera motor 驱动进行修改。
    - camera motor 与 camera sensor 模组的硬件连接方式有很多种，驱动人员根据实际的硬件需求编写驱动程序。建议尽量使用 camera sensor 上的管脚、并与 camera sensor 共用 sccb 总线，以节省 SOC 的管脚。

功能概览
------------------

camera motor 的驱动程序提供下述功能：

- `设备发现 <#device_find>`__ - 包括对传感器进行读写检查，以及如何在完成工作后回收资源。
- `参数控制 <#para_ctrl>`__ - 包括查询设备支持的参数信息，设置和查询参数值等信息。
- `IO 控制 <#io_ctrl>`__ - 包括控制设备的重启、休眠等功能。
- `格式控制 <#fmt-management>`__ - 描述了如何枚举该 camera motor 支持的格式，以及设置和查询格式。
- `查询名称 <#get-name>`__ - 描述了在使用中查询当前设备的名称。
- `Kconfig 选项 <#kconfig-options>`__ - 列出了支持的 Kconfig 选项，这些选项可以对驱动程序产生不同影响。

设备发现
^^^^^^^^^^^^^^^^

若传感器已正确连接到 SOC 上，并且驱动程序正常，驱动程序将自动检测对传感器进行 SCCB 读写检查，并返回 ``esp_cam_motor_device_t`` 来表示该设备。在调用任何其他 ``esp_cam_motor_`` API 之前必须先生成该设备的描述符。

通过传感器命名的接口探测指定设备
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

以 DW9714 为例，在应用代码中包含 ``dw9714.h`` 头文件，然后通过 ``dw9714_detect()`` 函数探测该总线上是否连接了该设备。

探测指定 sccb 总线上的所有设备
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

camera motor 驱动提供了一个数组，以供用户探测指定的 sccb 上连接的所有 camera motor 设备。

.. code-block:: c

    for (esp_cam_motor_detect_fn_t *p = &__esp_cam_motor_detect_fn_array_start; p < &__esp_cam_motor_detect_fn_array_end; p++) {
        esp_cam_motor_config_t cfg = {0};
        esp_cam_motor_device_t *motor_dev;
        const esp_video_init_cam_motor_config_t *cm = config->cam_motor;
        cfg.sccb_handle = create_sccb_device(sccb_mark, ESP_VIDEO_INIT_MOTOR_SCCB, &cm->sccb_config, p->sccb_addr);
        if (!cfg.sccb_handle) {
            return ESP_FAIL;
        }

        cfg.reset_pin = cm->reset_pin,
        cfg.pwdn_pin = cm->pwdn_pin,
        cfg.signal_pin = cm->signal_pin,
        motor_dev = (*(p->detect))((void *)&cfg);
        if (!motor_dev) {
            destroy_sccb_device(cfg.sccb_handle, sccb_mark, &config->csi->sccb_config);
            ESP_LOGE(TAG, "failed to detect sensor motor with address=%x", p->sccb_addr);
            continue;
        }
        break;
    }

删除设备
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

如果不再需要使用相机设备，可以调用 ``esp_cam_motor_del_dev()`` 来回收资源。

参数控制
^^^^^^^^^^^^^^^^

camera motor 设备的参数可以被查询和设置：

- esp_cam_motor_query_para_desc() 用于查询参数的类型、取值范围。
- esp_cam_motor_get_para_value() 用于查询参数的当前值。
- esp_cam_motor_set_para_value() 用于设置参数的值。

IO 控制
^^^^^^^^^^^^^^^^

相机设备的输入输出控制可以通过调用 ``esp_cam_motor_ioctl()`` 来实现。

格式控制
^^^^^^^^^^^^^^^^

camera motor 设备的格式信息包括：工作模式、步进信息、默认位置等信息。所有由初始化寄存器列表决定的信息均通过 ``esp_cam_motor_format_t`` 结构体来描述。通过设置和查询该结构体的信息，IPA 可以更加清楚地控制 camera motor 完成对焦功能。

- esp_cam_motor_query_format() 用于查询支持的格式信息。
- esp_cam_motor_get_format() 用于查询正在使用的格式信息。
- esp_cam_motor_set_format() 用于设置使用的格式信息。

.. attention::

    如需调整 camera motor 的工作模式，需要注意以下问题：
    
    - Ringing effect： 移动马达时由于有惯性，会存在惯性振动。通常 sensor 上带有抑制振动的控制模式，如 DW9714 传感器的 DLC(Dual Level Control) 模式。
    - Border control：移动马达时由于有惯性，接近边缘的时候要有边缘控制，否则可能撞到边缘。若默认设备上电后，将马达移动到距离感光矩阵最远处的地方，设备断电将可能导致马达迅速向初始化位置移动，从而触碰边缘，发出异响，此为正常现象。
    - Step size control：不要一次移动太大的步长，分多次移动，以平滑的小步长慢慢移动过去。开发人员编写完驱动后，可以启动一个定时器，并在定时器中可控制步长递增或递减，在可以观察图像的示例中，查看马达的移动是否平滑。
    - 考虑到马达的重力，其在水平、垂直朝上、垂直朝下等不同的方向上的重力不一样，需要的步进值也不一样。若有方向传感器，可以参考方向作优化。

查询名称
^^^^^^^^^^^^^^^^

通过调用 ``esp_cam_motor_get_name()`` 接口来查询当前设备的名称。

Kconfig 选项
^^^^^^^^^^^^^^^^

每一个 camera motor 都有一个配置文件，以 DW9714 为例，对应的是 ``sensors/dw9714/Kconfig.dw9714`` 文件。该配置文件可以配置该设备默认加载的格式、以及是否启用该设备的自动探测功能等信息。

API 参考
-------------------------

.. include-build-file:: inc/esp_cam_motor.inc
