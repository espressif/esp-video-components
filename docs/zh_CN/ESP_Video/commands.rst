****************
命令介绍
****************

本文档说明 ``esp_video`` 已实现的 V4L2 ioctl 请求代码、命令参数及用法示例，便于按功能查阅。设备节点、Capture / M2M 数据通路以及应用侧典型调用顺序见 :doc:`index`。初始化 API 见 :doc:`api_reference`。

.. important::

   - 示例侧重接口用法，未覆盖完整错误处理。产品代码须检查 ``ioctl`` 返回值。
   - 组件只实现当前芯片硬件能力范围内的 V4L2 功能。未列入下表的标准命令默认未实现；部分命令仅部分设备可用，或以编译宏为准。例如 ``VIDIOC_SUBSCRIBE_EVENT`` / ``VIDIOC_DQEVENT`` / ``VIDIOC_S_EVENT_CALLBACK`` 仅在 MIPI-CSI 驱动支持事件时可用（``ESP_VIDEO_CSI_DRIVER_HAS_EVENT``）。若当前 ESP-IDF 的 MIPI-CSI 驱动不含错误事件，应用程序不应调用这些命令。
   - Capture 设备使用 ``V4L2_BUF_TYPE_VIDEO_CAPTURE``。M2M 编解码设备（JPEG 编码 ``/dev/video10``、H.264 编码 ``/dev/video11``、JPEG 解码 ``/dev/video12``）须分别对 ``V4L2_BUF_TYPE_VIDEO_OUTPUT``（输入）和 ``V4L2_BUF_TYPE_VIDEO_CAPTURE``（输出）申请缓冲并 ``STREAMON`` / ``STREAMOFF``。
   - 像素格式按字节序区分：YUV422 使用 ``V4L2_PIX_FMT_YUYV`` / ``V4L2_PIX_FMT_UYVY``；RGB565 使用 ``V4L2_PIX_FMT_RGB565``（小端）/ ``V4L2_PIX_FMT_RGB565X``（大端）。已不再使用 ``V4L2_PIX_FMT_YUV422P``。JPEG 解码器还支持 ``V4L2_PIX_FMT_BGR565``。

ioctl 调用顺序
~~~~~~~~~~~~~~~~~~~~~~

应用侧典型顺序与 :doc:`index` 中的应用编程一节一致：打开设备 → 查询能力与格式 → 申请并排队缓冲区 → ``STREAMON`` → 循环 ``DQBUF`` / ``QBUF`` → ``STREAMOFF`` → 关闭设备。ioctl 会落到对应子设备；以查询、设置相机传感器格式为例：

.. blockdiag::
    :caption: ioctl 调用流程
    :align: center

    blockdiag video_ioctl_call_flow {

        # global attributes
        node_height = 60;
        node_width = 225;
        span_width = 50;
        span_height = 20;
        default_shape = roundedbox;

        # labels of diagram nodes
        IOCTL1 [label = "ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc)"];
        IOCTL2 [label = "ioctl(fd, VIDIOC_S_FMT, &format)"];
        SENSOR1 [label = "sensor_query_format()"];
        SENSOR2 [label = "sensor_set_format()"];

        # node connections + labels
        IOCTL1 -> SENSOR1;
        IOCTL2 -> SENSOR2;
    }

下文先给出已实现命令一览，再按功能展开参数与示例。超时、传感器格式、事件等厂商扩展 ioctl 见后文扩展命令。控制项 ID 见文末控制 ID 一览。

V4L2 命令
~~~~~~~~~~~~~~~~~~~~~~

当前已实现的请求代码如下。未列出的标准命令默认未实现。

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 命令
     - 适用范围
   * - ``VIDIOC_QUERYCAP``
     - 所有视频设备
   * - ``VIDIOC_ENUM_FMT`` / ``VIDIOC_G_FMT`` / ``VIDIOC_S_FMT``
     - Capture 与 M2M；须在停流时设置格式
   * - ``VIDIOC_ENUM_FRAMESIZES`` / ``VIDIOC_ENUM_FRAMEINTERVALS``
     - CSI / DVP / SPI 等采集设备
   * - ``VIDIOC_REQBUFS`` / ``VIDIOC_QUERYBUF`` / ``VIDIOC_QBUF`` / ``VIDIOC_DQBUF``
     - 所有流式设备；``count=0`` 释放缓冲
   * - ``VIDIOC_STREAMON`` / ``VIDIOC_STREAMOFF``
     - 所有流式设备；M2M 需分别启停 OUTPUT 与 CAPTURE
   * - ``VIDIOC_QUERY_EXT_CTRL`` / ``VIDIOC_S_EXT_CTRLS`` / ``VIDIOC_G_EXT_CTRLS`` / ``VIDIOC_QUERYMENU``
     - 传感器、ISP、JPEG、H.264 等控制项
   * - ``VIDIOC_S_SELECTION`` / ``VIDIOC_G_SELECTION``
     - MIPI-CSI 裁剪
   * - ``VIDIOC_S_PARM`` / ``VIDIOC_G_PARM``
     - 采集帧间隔；H.264 设备可用于配置/查询 FPS（开流后也可设置）
   * - ``VIDIOC_S_SENSOR_FMT`` / ``VIDIOC_G_SENSOR_FMT``
     - 相机传感器输出格式
   * - ``VIDIOC_S_MOTOR_FMT`` / ``VIDIOC_G_MOTOR_FMT``
     - 自动对焦电机（需使能电机控制器）
   * - ``VIDIOC_SET_OWNER``
     - 增减设备引用计数
   * - ``VIDIOC_S_DQBUF_TIMEOUT`` / ``VIDIOC_G_DQBUF_TIMEOUT``
     - ``DQBUF`` 超时
   * - ``VIDIOC_SUBSCRIBE_EVENT`` / ``VIDIOC_UNSUBSCRIBE_EVENT`` / ``VIDIOC_DQEVENT`` / ``VIDIOC_S_EVENT_CALLBACK``
     - 仅 MIPI-CSI，且驱动支持事件
   * - ``VIDIOC_RESTART``
     - 须在开流之后、停流之前调用

上表同时列出标准命令与厂商扩展命令。下面按查询、格式、缓冲、数据流、控制的顺序说明标准命令；每小节依次给出请求代码、参数结构、字段说明和示例。厂商扩展命令见后文扩展命令。

查询设备能力
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**命令简介**：

- 命令（请求代码）：``VIDIOC_QUERYCAP``
- 命令参数：``struct v4l2_capability``

参数主要字段：

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 参数
     - 类型
     - 用途
   * - driver
     - __u8[16]
     - 驱动名
   * - card
     - __u8[32]
     - 硬件卡名
   * - bus_info
     - __u8[32]
     - 总线信息
   * - version
     - __u32
     - 驱动版本
   * - capabilities
     - __u32
     - 能力位掩码
   * - device_caps
     - __u32
     - 设备能力位掩码

**用法示例**：

.. code-block:: c

   void example_querycap(void)
   {
       int fd = open("/dev/video0", O_RDWR);

       struct v4l2_capability cap = {0};

       ioctl(fd, VIDIOC_QUERYCAP, &cap);

       printf("driver: %s, card: %s, bus: %s\n", (char *)cap.driver, (char *)cap.card, (char *)cap.bus_info);
       printf("version: %d.%d.%d\n", (uint16_t)(cap.version >> 16),
              (uint8_t)(cap.version >> 8),
              (uint8_t)cap.version);
       printf("capabilities: 0x%x\n", cap.capabilities);
       printf("device_caps: 0x%x\n", cap.device_caps);

       close(fd);
   }

枚举像素格式
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**命令简介**：

- 命令（请求代码）：``VIDIOC_ENUM_FMT``
- 命令参数：``struct v4l2_fmtdesc``

参数主要字段：

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 参数
     - 类型
     - 用途
   * - index
     - __u32
     - 格式序号递增
   * - type
     - __u32
     - 缓冲区类型
   * - description
     - __u8[32]
     - 格式描述
   * - pixelformat
     - __u32
     - 四字符像素码

**用法示例**：

.. code-block:: c

   void example_enum_fmt(void)
   {
       int fd = open("/dev/video0", O_RDWR);

       struct v4l2_fmtdesc fmtdesc = {0};

       fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

       for (fmtdesc.index = 0; ; ++fmtdesc.index) {
           if (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) < 0) {
               break;
           }

           printf("Format: %s, pixelformat: 0x%x\n", (char *)fmtdesc.description, fmtdesc.pixelformat);
       }

       close(fd);
   }

设置/获取数据格式
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**命令简介**：

- 命令（请求代码）：``VIDIOC_S_FMT`` / ``VIDIOC_G_FMT``
- 命令参数：``struct v4l2_format``

参数主要字段（以 ``type == V4L2_BUF_TYPE_VIDEO_CAPTURE`` 为例）：

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 参数
     - 类型
     - 用途
   * - type
     - __u32
     - 缓冲区类型
   * -  fmt
     - union
     - 格式信息，仅支持 pix 字段

**pix（struct v4l2_pix_format） 字段说明**：

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 参数
     - 类型
     - 用途
   * - width
     - __u32
     - 图像宽度
   * - height
     - __u32
     - 图像高度
   * - pixelformat
     - __u32
     - 像素格式
   * - sizeimage
     - __u32
     - 图像大小
   * - ycbcr_enc
     - __u32
     - Y'CbCr 编码
   * - quantization
     - __u32
     - 量化

**用法示例（设置/获取/尝试格式）**：

.. code-block:: c

   void example_set_get_try_fmt(void)
   {
       int fd = open("/dev/video0", O_RDWR);

       struct v4l2_format fmt = {0};

       // 设置格式
       fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
       fmt.fmt.pix.width = 320;
       fmt.fmt.pix.height = 240;
       fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
       fmt.fmt.pix.field = V4L2_FIELD_NONE;

       ioctl(fd, VIDIOC_S_FMT, &fmt);

       /* 运行时改格式须在停流状态下进行 */

       // 获取当前格式
       memset(&fmt, 0, sizeof(fmt));
       fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

       ioctl(fd, VIDIOC_G_FMT, &fmt);

       printf("Current: %ux%u, fmt=0x%x\n", fmt.fmt.pix.width, fmt.fmt.pix.height, fmt.fmt.pix.pixelformat);

       close(fd);
   }

枚举支持的分辨率
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**命令简介**：

- 命令（请求代码）：``VIDIOC_ENUM_FRAMESIZES``
- 命令参数：``struct v4l2_frmsizeenum``

参数主要字段：

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 参数
     - 类型
     - 用途
   * - index
     - __u32
     - 分辨率序号递增
   * - pixel_format
     - __u32
     - 目标像素格式
   * - type
     - __u32
     - 固定/步进/自定义
   * - discrete/stepwise
     - union
     - 分辨率信息

**discrete（struct v4l2_frmsize_discrete） 字段说明**：

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 参数
     - 类型
     - 用途
   * - width
     - __u32
     - 宽度
   * - height
     - __u32
     - 高度

**stepwise（struct v4l2_frmsize_stepwise） 字段说明**：

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 参数
     - 类型
     - 用途
   * - min_width
     - __u32
     - 最小宽度
   * - max_width
     - __u32
     - 最大宽度
   * - step_width
     - __u32
     - 宽度步长
   * - min_height
     - __u32
     - 最小高度
   * - max_height
     - __u32
     - 最大高度
   * - step_height
     - __u32
     - 高度步长

**用法示例**：

.. code-block:: c

   void example_enum_framesizes(void)
   {
       int fd = open("/dev/video0", O_RDWR);

       struct v4l2_frmsizeenum fsize = {0};
       fsize.pixel_format = V4L2_PIX_FMT_RGB565;

       for (fsize.index = 0; ; fsize.index++) {
           if (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &fsize) < 0) {
               break;
           }

           if (fsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
               printf("Discrete: %ux%u\n", fsize.discrete.width, fsize.discrete.height);
           } else if (fsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
               printf("Stepwise: %ux%u~%ux%u, step: %ux%u\n", fsize.stepwise.min_width, fsize.stepwise.min_height, fsize.stepwise.max_width, fsize.stepwise.max_height, fsize.stepwise.step_width, fsize.stepwise.step_height);
           }
       }

       close(fd);
   }

枚举帧率
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**命令简介**：

- 命令（请求代码）：``VIDIOC_ENUM_FRAMEINTERVALS``
- 命令参数：``struct v4l2_frmivalenum``

参数主要字段：

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 参数
     - 类型
     - 用途
   * - index
     - __u32
     - 帧率序号递增
   * - pixel_format
     - __u32
     - 目标像素格式
   * - width
     - __u32
     - 宽度
   * - height
     - __u32
     - 高度
   * - type
     - __u32
     - 离散/步进/连续
   * - discrete/stepwise
     - union
     - 帧率信息

**discrete（struct v4l2_fract） 字段说明**：

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 参数
     - 类型
     - 用途
   * - numerator
     - __u32
     - 分子
   * - denominator
     - __u32
     - 分母

**stepwise（struct v4l2_frmival_stepwise） 字段说明**：

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 参数
     - 类型
     - 用途
   * - min
     - struct v4l2_fract
     - 最小帧率
   * - max
     - struct v4l2_fract
     - 最大帧率
   * - step
     - struct v4l2_fract
     - 帧率步长

**用法示例**：

.. code-block:: c

   void example_enum_frameintervals(void)
   {
       int fd = open("/dev/video0", O_RDWR);

       struct v4l2_frmivalenum ival = {0};
       ival.pixel_format = V4L2_PIX_FMT_RGB565;
       ival.width = 640;
       ival.height = 480;

       for (ival.index = 0; ; ival.index++) {
           if (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &ival) < 0) {
               break;
           }

           if (ival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
               printf("Discrete interval: %u/%u\n", ival.discrete.numerator, ival.discrete.denominator);
           } else if (ival.type == V4L2_FRMIVAL_TYPE_STEPWISE) {
               printf("Stepwise interval: %u/%u~%u/%u, step: %u/%u\n", ival.stepwise.min.numerator, ival.stepwise.min.denominator, ival.stepwise.max.numerator, ival.stepwise.max.denominator, ival.stepwise.step.numerator, ival.stepwise.step.denominator);
           }
       }

       close(fd);
   }

申请/释放缓冲区
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- 命令（请求代码）：``VIDIOC_REQBUFS``
- 命令参数：``struct v4l2_requestbuffers``

**字段说明：**

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 字段
     - 类型
     - 用途
   * - count
     - __u32
     - 申请/释放的缓冲区数量
   * - type
     - __u32
     - 缓冲区类型
   * - memory
     - __u32
     - 缓冲区内存类型

**用法示例**：

.. code-block:: c

   void example_reqbufs(void)
   {
       int fd = open("/dev/video0", O_RDWR);

       struct v4l2_requestbuffers req = {0};
       req.count = 3;
       req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
       req.memory = V4L2_MEMORY_MMAP;

       ioctl(fd, VIDIOC_REQBUFS, &req);

       /* count=0 释放已申请的视频缓冲 */
       req.count = 0;
       ioctl(fd, VIDIOC_REQBUFS, &req);

       close(fd);
   }

.. note::

   内存类型支持 ``V4L2_MEMORY_MMAP`` 与 ``V4L2_MEMORY_USERPTR``。M2M 设备需分别对 ``V4L2_BUF_TYPE_VIDEO_OUTPUT`` 与 ``V4L2_BUF_TYPE_VIDEO_CAPTURE`` 调用 ``VIDIOC_REQBUFS``。


查询缓冲区属性
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- 命令（请求代码）：``VIDIOC_QUERYBUF``
- 命令参数：``struct v4l2_buffer``

**字段说明：**

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 字段
     - 类型
     - 用途
   * - index
     - __u32
     - 缓冲区索引（0 ~ count-1）
   * - type
     - __u32
     - 缓冲区类型
   * - bytesused
     - __u32
     - 实际有效数据长度
   * - memory
     - __u32
     - 内存类型
   * - m.offset/m.userptr
     - union
     - 缓冲区地址（offset 或 userptr）
   * - length
     - __u32
     - 缓冲区总长度

**用法示例**：

.. code-block:: c

   void example_querybuf(void)
   {
       int fd = open("/dev/video0", O_RDWR);

       struct v4l2_buffer buf = {0};
       buf.index = 0;
       buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
       buf.memory = V4L2_MEMORY_MMAP;

       ioctl(fd, VIDIOC_QUERYBUF, &buf);

       printf("Buffer length: %u\n", buf.length);
       printf("Buffer offset: %u\n", buf.m.offset);

       close(fd);
   }

缓冲区入队列
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- 命令（请求代码）：``VIDIOC_QBUF``
- 命令参数：``struct v4l2_buffer``

结构体字段与 ``VIDIOC_QUERYBUF`` 相同。

**用法示例**：

.. code-block:: c

   void example_qbuf(void)
   {
       int fd = open("/dev/video0", O_RDWR);

       struct v4l2_buffer buf = {0};
       buf.index = 0;
       buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
       buf.memory = V4L2_MEMORY_MMAP;

       ioctl(fd, VIDIOC_QBUF, &buf);

       close(fd);
   }

缓冲区出队列
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- 命令（请求代码）：``VIDIOC_DQBUF``
- 命令参数：``struct v4l2_buffer``

结构体字段与 ``VIDIOC_QUERYBUF`` 相同。

**用法示例**：

.. code-block:: c

   void example_dqbuf(void)
   {
       int fd = open("/dev/video0", O_RDWR);

       struct v4l2_buffer buf = {0};
       buf.index = 0;
       buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
       buf.memory = V4L2_MEMORY_MMAP;
 
       ioctl(fd, VIDIOC_DQBUF, &buf);

       close(fd);
   }

启动/停止数据流
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- 命令（请求代码）：``VIDIOC_STREAMON`` / ``VIDIOC_STREAMOFF``
- 命令参数：``int *``

功能说明：

- ``STREAMON``：开流，开始处理缓冲队列
- ``STREAMOFF``：停流；设备会丢弃队列中尚未处理的缓冲

M2M 编解码设备须分别对 OUTPUT 与 CAPTURE 调用本命令。USB UVC 在停流后会回收队列中的缓冲。

**用法示例**：

.. code-block:: c

   void example_streamon_streamoff(void)
   {
       int fd = open("/dev/video0", O_RDWR);

       int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
       ioctl(fd, VIDIOC_STREAMON, &type);

       type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
       ioctl(fd, VIDIOC_STREAMOFF, &type);

       close(fd);
   }


查询扩展控制
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- 命令（请求代码）：``VIDIOC_QUERY_EXT_CTRL``
- 命令参数：``struct v4l2_query_ext_ctrl``

**字段说明**：

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 字段
     - 类型
     - 用途
   * - id
     - __u32
     - 控制ID
   * - type
     - __u32
     - 控制类型
   * - name
     - __u8[32]
     - 控制名称
   * - minimum
     - __s64
     - 最小值
   * - maximum
     - __s64
     - 最大值
   * - step
     - __u64
     - 步长
   * - default_value
     - __s64
     - 默认值
   * - elem_size
     - __u32
     - 元素大小
   * - elems
     - __u32
     - 元素数量
   * - nr_of_dims
     - __u32
     - 维度数量
   * - dims
     - __u32[4]
     - 维度

**用法示例**：

.. code-block:: c

   void example_query_ext_ctrl(void)
   {
       int fd = open("/dev/video0", O_RDWR);

       struct v4l2_query_ext_ctrl qctrl = {0};
       qctrl.id = V4L2_CID_BRIGHTNESS;
       ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &qctrl);

       printf("Control: %s, type: %u, minimum: %lld, maximum: %lld, step: %lld, default_value: %lld\n", qctrl.name, qctrl.type, qctrl.minimum, qctrl.maximum, qctrl.step, qctrl.default_value);

       close(fd);
   }

.. note::

   可用 ``qctrl.id = V4L2_CID_BRIGHTNESS | V4L2_CTRL_FLAG_NEXT_CTRL`` 从指定 ID 起迭代枚举后续控制项。

设置/获取扩展控制
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- 命令（请求代码）：``VIDIOC_S_EXT_CTRLS`` / ``VIDIOC_G_EXT_CTRLS``
- 命令参数：``struct v4l2_ext_controls``

**字段说明**：

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 字段
     - 类型
     - 用途
   * - ctrl_class/which
     - union
     - 控制类/控制类型
   * - count
     - __u32
     - 控制数量
   * - controls
     - struct v4l2_ext_control *
     - 控制数组

**controls（struct v4l2_ext_control） 字段说明**：

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 字段
     - 类型
     - 用途
   * - id
     - __u32
     - 控制ID
   * - size
     - __u32
     - 元素大小
   * - value/value64/...
     - union
     - 值/64 位值/... bit

**用法示例**：

.. code-block:: c

   void example_set_get_ext_ctrl(void)
   {
       int fd = open("/dev/video0", O_RDWR);

       struct v4l2_ext_controls ctrls = {0};
       struct v4l2_ext_control control = {0};

       ctrls.ctrl_class = V4L2_CTRL_CLASS_USER;
       ctrls.count = 1;
       ctrls.controls = &control;
       control.id = V4L2_CID_BRIGHTNESS;
       control.value = 128;
       control.size = sizeof(int);
       ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls);

       memset(&ctrls, 0, sizeof(struct v4l2_ext_controls));
       memset(&control, 0, sizeof(struct v4l2_ext_control));
       ctrls.ctrl_class = V4L2_CTRL_CLASS_USER;
       ctrls.count = 1;
       ctrls.controls = &control;
       control.id = V4L2_CID_BRIGHTNESS;
       control.size = sizeof(int);
       ioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls);
       printf("Brightness: %d\n", control.value);

       close(fd);
   }

``VIDIOC_S_EXT_CTRLS`` 可以一次设置单个或多个控制项。下面以相机传感器曝光时间为例：

.. code:: c

    static void config_exposure_time(int cam_fd, int32_t exposure)
    {
        struct v4l2_query_ext_ctrl qctrl;
        struct v4l2_ext_controls controls;
        struct v4l2_ext_control control[1];

        qctrl.id = V4L2_CID_EXPOSURE_ABSOLUTE;
        /* 查询该控制项的取值范围 */
        ioctl(cam_fd, VIDIOC_QUERY_EXT_CTRL, &qctrl);
        ESP_LOGW(TAG, "EXP min: %d, EXP max:  %d, step:%d", (int)qctrl.minimum, (int)qctrl.maximum, (int)qctrl.step);

        controls.ctrl_class = V4L2_CID_CAMERA_CLASS;
        controls.count      = 1;
        controls.controls   = control;
        control[0].id       = V4L2_CID_EXPOSURE_ABSOLUTE;
        control[0].value    = exposure / 100;
        if (ioctl(cam_fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
            ESP_LOGE(TAG, "failed to set exposure time");
        } else {
            ESP_LOGW(TAG, "set exposure");
        }
    }

.. note::

   通过 ``V4L2_CTRL_CLASS_ESP_CAM_IOCTL`` 可直接调用 ``esp_cam_sensor`` 的 ioctl（仅支持 ``v4l2_ext_control`` 的 ``p_u8`` 与 ``size`` 字段）。H.264 编码器支持在开流后设置 I 帧间隔、码率、QP 等控制项。


查询菜单
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- 命令（请求代码）：``VIDIOC_QUERYMENU``
- 命令参数：``struct v4l2_querymenu``

**字段说明**：

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 字段
     - 类型
     - 用途
   * - id
     - __u32
     - 菜单ID
   * - index
     - __u32
     - 菜单索引
   * - name/value
     - union
     - 菜单名称/菜单值

**用法示例**：

.. code-block:: c

   void example_query_menu(void)
   {
       int fd = open("/dev/video0", O_RDWR);

       struct v4l2_querymenu qmenu = {0};
       qmenu.id = V4L2_CID_GAIN;
       qmenu.index = 0;
       ioctl(fd, VIDIOC_QUERYMENU, &qmenu);

       printf("Gain: value: %lld\n", qmenu.value);

       close(fd);
   }


设置/获取选择区域
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- 命令（请求代码）：``VIDIOC_S_SELECTION`` / ``VIDIOC_G_SELECTION``
- 命令参数：``struct v4l2_selection``
- 适用范围：MIPI-CSI 视频设备的图像裁剪

**字段说明**：

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 字段
     - 类型
     - 用途
   * - type
     - __u32
     - 缓冲区类型
   * - target
     - __u32
     - 选择区域目标
   * - r
     - struct v4l2_rect
     - 选择区域

**r（struct v4l2_rect） 字段说明**：

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 字段
     - 类型
     - 用途
   * - left
     - __s32
     - 左边界
   * - top
     - __s32
     - 上边界
   * - width
     - __u32
     - 宽度
   * - height
     - __u32
     - 高度

**用法示例**：

.. code-block:: c

   void example_set_get_selection(void)
   {
       int fd = open("/dev/video0", O_RDWR);

       struct v4l2_selection selection = {0};
       selection.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
       selection.target = V4L2_SEL_TGT_CROP;
       selection.r.left = 0;
       selection.r.top = 0;
       selection.r.width = 640;
       selection.r.height = 480;
       ioctl(fd, VIDIOC_S_SELECTION, &selection);

       memset(&selection, 0, sizeof(struct v4l2_selection));
       selection.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
       selection.target = V4L2_SEL_TGT_CROP;
       ioctl(fd, VIDIOC_G_SELECTION, &selection);
       printf("Selection: left: %d, top: %d, width: %d, height: %d\n", selection.r.left, selection.r.top, selection.r.width, selection.r.height);

       close(fd);
   }


设置/获取流参数
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- 命令（请求代码）：``VIDIOC_S_PARM`` / ``VIDIOC_G_PARM``
- 命令参数：``struct v4l2_streamparm``
- 适用范围：采集设备的帧间隔；H.264 编码器可用 ``timeperframe`` 配置/查询 FPS，且支持在开流后修改编码参数

**字段说明**：

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 字段
     - 类型
     - 用途
   * - type
     - __u32
     - 流类型
   * - v4l2_captureparm/v4l2_outputparm
     - union
     - 采集参数

**capture（struct v4l2_captureparm） 字段说明**：

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 字段
     - 类型
     - 用途
   * - capability
     - __u32
     - 捕获能力
   * - timeperframe
     - struct v4l2_fract
     - 每帧时间间隔

**用法示例**：

.. code-block:: c

   void example_set_get_parm(void)
   {
       int fd = open("/dev/video0", O_RDWR);

       struct v4l2_streamparm sparm = {0};
       sparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
       sparm.parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
       sparm.parm.capture.timeperframe.numerator = 1;
       sparm.parm.capture.timeperframe.denominator = 30;
       ioctl(fd, VIDIOC_S_PARM, &sparm);

       memset(&sparm, 0, sizeof(struct v4l2_streamparm));
       sparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
       ioctl(fd, VIDIOC_G_PARM, &sparm);
       printf("Time per frame: %d/%d\n", sparm.parm.capture.timeperframe.numerator, sparm.parm.capture.timeperframe.denominator);

       close(fd);
   }

扩展命令
~~~~~~~~~~~~~~~~~~~~~~

以下为 ``esp_video`` 在标准 V4L2 之外提供的请求代码，用于超时、传感器/电机格式、引用计数、MIPI-CSI 事件和设备重启。

设置/获取缓冲区出队列超时时间
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- 命令（请求代码）：``VIDIOC_S_DQBUF_TIMEOUT`` / ``VIDIOC_G_DQBUF_TIMEOUT``
- 命令参数：``struct timeval``

**字段说明**：

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 字段
     - 类型
     - 用途
   * - tv_sec
     - __s32
     - 秒
   * - tv_usec
     - __s32
     - 微秒

**用法示例**：

.. code-block:: c

   void example_set_get_dqbuf_timeout(void)
   {
       int fd = open("/dev/video0", O_RDWR);

       struct timeval timeout = {0};
       timeout.tv_sec = 1;
       timeout.tv_usec = 0;

       ioctl(fd, VIDIOC_S_DQBUF_TIMEOUT, &timeout);

       memset(&timeout, 0, sizeof(struct timeval));
       ioctl(fd, VIDIOC_G_DQBUF_TIMEOUT, &timeout);
       printf("DQBUF timeout: %d/%d\n", timeout.tv_sec, timeout.tv_usec);

       close(fd);
   }


设置/获取相机传感器输出格式
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- 命令（请求代码）：``VIDIOC_S_SENSOR_FMT`` / ``VIDIOC_G_SENSOR_FMT``
- 命令参数：``esp_cam_sensor_format_t``

字段说明见 ``esp_cam_sensor_format_t``。

**用法示例**：

.. code-block:: c

   static const esp_cam_sensor_format_t s_sensor_format = {
       ......
   };

   void example_set_get_sensor_format(void)
   {
       int fd = open("/dev/video0", O_RDWR);

       ioctl(fd, VIDIOC_S_SENSOR_FMT, &s_sensor_format);

       esp_cam_sensor_format_t format = {0};
       ioctl(fd, VIDIOC_G_SENSOR_FMT, &format);
       printf("Sensor format: %s\n", format.name);

       close(fd);
   }

设置/获取自动对焦电机格式
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- 命令（请求代码）：``VIDIOC_S_MOTOR_FMT`` / ``VIDIOC_G_MOTOR_FMT``
- 命令参数：``esp_cam_motor_format_t``
- 适用范围：已使能相机电机控制器（``CONFIG_ESP_VIDEO_ENABLE_CAMERA_MOTOR_CONTROLLER``）

**主要字段**：

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 字段
     - 类型
     - 用途
   * - name
     - const char *
     - 格式名称
   * - mode
     - esp_cam_motor_mode_t
     - 控制模式（DIRECT / LSC / DLC）
   * - step_period
     - struct
     - 步进周期与每步 code 数
   * - init_position
     - int
     - 该格式对应的初始位置

**用法示例**：

.. code-block:: c

   void example_set_get_motor_format(void)
   {
       int fd = open("/dev/video0", O_RDWR);
       esp_cam_motor_format_t format = {0};

       ioctl(fd, VIDIOC_G_MOTOR_FMT, &format);
       printf("Motor format: %s\n", format.name);

       ioctl(fd, VIDIOC_S_MOTOR_FMT, &format);

       close(fd);
   }

设置设备引用计数
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- 命令（请求代码）：``VIDIOC_SET_OWNER``
- 命令参数：``int *``

功能说明：入参非 0 时增加设备引用计数，为 0 时减少引用计数。多打开/关闭同一视频设备时用于保持设备存活。

**用法示例**：

.. code-block:: c

   void example_set_owner(void)
   {
       int fd = open("/dev/video0", O_RDWR);
       int owner = 1;

       ioctl(fd, VIDIOC_SET_OWNER, &owner);

       owner = 0;
       ioctl(fd, VIDIOC_SET_OWNER, &owner);

       close(fd);
   }

订阅/取消订阅事件
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- 命令（请求代码）：``VIDIOC_SUBSCRIBE_EVENT`` / ``VIDIOC_UNSUBSCRIBE_EVENT``
- 命令参数：``struct v4l2_event_subscription``
- 适用范围：仅 MIPI-CSI；依赖 ``ESP_VIDEO_CSI_DRIVER_HAS_EVENT``。ESP-IDF 若未提供 MIPI-CSI 错误事件，则本命令不可用。

**字段说明**：

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 字段
     - 类型
     - 用途
   * - type
     - __u32
     - 事件类型。应用可订阅 ``V4L2_EVENT_ESP_MIPI_CSI_ERROR``；``V4L2_EVENT_ALL`` 用于取消全部订阅
   * - id
     - __u32
     - 事件 ID（当前未使用）
   * - flags
     - __u32
     - 订阅标志

当前支持的事件类型：

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 类型
     - 说明
   * - ``V4L2_EVENT_ESP_MIPI_CSI_ERROR``
     - MIPI-CSI 主机错误。载荷为 ``v4l2_event_esp_mipi_csi_error_t``，含 ``host_err_mask``
   * - ``V4L2_EVENT_ESP_VIDEO_EVENT_UNSUBSCRIBED``
     - 取消订阅后由驱动投递给 ``DQEVENT`` 等待任务，便于退出监听循环。应用无需主动订阅
   * - ``V4L2_EVENT_ESP_MIPI_CSI_INTERRUPT_DISABLE``
     - 内部使用。事件回调返回 true 时禁用硬件中断并投递本事件，应用不应订阅

``host_err_mask`` 位定义：

- ``ESP_MIPI_CSI_HOST_ERR_PHY``：PHY 错误
- ``ESP_MIPI_CSI_HOST_ERR_PACKET``：包错误
- ``ESP_MIPI_CSI_HOST_ERR_FRAME``：帧边界或序号错误
- ``ESP_MIPI_CSI_HOST_ERR_CRC``：CRC 错误
- ``ESP_MIPI_CSI_HOST_ERR_DATA_ID``：无法识别的 data type

**用法示例**：

.. code-block:: c

   void example_subscribe_event(int fd)
   {
       struct v4l2_event_subscription sub = {
           .type = V4L2_EVENT_ESP_MIPI_CSI_ERROR,
       };

       ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);

       /* 取消全部订阅 */
       sub.type = V4L2_EVENT_ALL;
       ioctl(fd, VIDIOC_UNSUBSCRIBE_EVENT, &sub);
   }

取出事件
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- 命令（请求代码）：``VIDIOC_DQEVENT``
- 命令参数：``struct v4l2_event``
- 适用范围：同 ``VIDIOC_SUBSCRIBE_EVENT``。通常在独立任务中循环调用。

**主要字段**：

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 字段
     - 类型
     - 用途
   * - type
     - __u32
     - 事件类型
   * - timestamp
     - struct timespec
     - 时间戳
   * - sequence
     - __u32
     - 序号
   * - u.data
     - __u8[]
     - 事件载荷。CSI 错误时按 ``v4l2_event_esp_mipi_csi_error_t`` 解析

**用法示例**：

.. code-block:: c

   void example_dqevent(int fd)
   {
       struct v4l2_event event;

       if (ioctl(fd, VIDIOC_DQEVENT, &event) < 0) {
           return;
       }

       if (event.type == V4L2_EVENT_ESP_MIPI_CSI_ERROR) {
           v4l2_event_esp_mipi_csi_error_t *err =
               (v4l2_event_esp_mipi_csi_error_t *)&event.u.data;
           printf("CSI error mask: 0x%x\n", (unsigned)err->host_err_mask);
       }
   }

完整事件跟踪示例见 ``esp_video/examples/common_components/example_video_common/example_video_event.c``。

设置事件回调
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- 命令（请求代码）：``VIDIOC_S_EVENT_CALLBACK``
- 命令参数：``struct v4l2_event_callback``
- 适用范围：同 MIPI-CSI 事件命令

**字段说明**：

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 字段
     - 类型
     - 用途
   * - callback_func
     - ``bool (*)(void *, uint32_t)``
     - 中断上下文回调。返回 true 时禁用硬件中断并投递 ``V4L2_EVENT_ESP_MIPI_CSI_INTERRUPT_DISABLE``
   * - user_data
     - void *
     - 回调用户数据

.. attention::

   回调必须放在 IRAM 中，且不得调用可能阻塞的函数（如 ``malloc`` / ``free`` / ``printf``），否则可能导致系统崩溃。

**用法示例**：

.. code-block:: c

   static bool IRAM_ATTR example_csi_isr(void *user_data, uint32_t err_mask)
   {
       (void)user_data;
       (void)err_mask;
       return false; /* 不禁用硬件中断 */
   }

   void example_set_event_callback(int fd)
   {
       struct v4l2_event_callback cb = {
           .callback_func = example_csi_isr,
           .user_data = NULL,
       };

       ioctl(fd, VIDIOC_S_EVENT_CALLBACK, &cb);
   }

重启视频设备
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- 命令（请求代码）：``VIDIOC_RESTART``
- 命令参数：``struct v4l2_restart_config``
- 适用范围：必须在开流之后、停流之前调用，用于硬件异常后恢复。

**字段说明**：

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - 字段
     - 类型
     - 用途
   * - type
     - int
     - ``enum v4l2_buf_type``，一般为 ``V4L2_BUF_TYPE_VIDEO_CAPTURE``
   * - restart_sensor
     - bool
     - 是否同时重启相机传感器

**用法示例**：

.. code-block:: c

   void example_restart(int fd)
   {
       struct v4l2_restart_config config = {
           .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
           .restart_sensor = true,
       };

       ioctl(fd, VIDIOC_RESTART, &config);
   }

控制 ID 一览
~~~~~~~~~~~~~~~~~~~~~~

以下为当前实现的主要控制 ID，配合前文 ``VIDIOC_QUERY_EXT_CTRL`` / ``VIDIOC_S_EXT_CTRLS`` 使用。具体设备是否支持某项，请用 ``VIDIOC_QUERY_EXT_CTRL`` 查询。

.. list-table::
   :header-rows: 1
   :width: 100%
   :widths: auto

   * - ID
     - Class
     - 说明
   * - ``V4L2_CID_VFLIP`` / ``V4L2_CID_HFLIP``
     - USER
     - 垂直/水平镜像
   * - ``V4L2_CID_GAIN``
     - USER
     - 像素增益
   * - ``V4L2_CID_EXPOSURE``
     - USER
     - 曝光，单位取决于传感器
   * - ``V4L2_CID_EXPOSURE_ABSOLUTE``
     - CAMERA
     - 曝光，单位 100 us
   * - ``V4L2_CID_TEST_PATTERN``
     - IMAGE_PROC
     - 测试图案
   * - ``V4L2_CID_JPEG_COMPRESSION_QUALITY``
     - JPEG
     - JPEG 压缩质量
   * - ``V4L2_CID_JPEG_CHROMA_SUBSAMPLING``
     - JPEG
     - JPEG 色度抽样
   * - ``V4L2_CID_MPEG_VIDEO_H264_I_PERIOD``
     - CODEC
     - H.264 I 帧间隔
   * - ``V4L2_CID_MPEG_VIDEO_BITRATE``
     - CODEC
     - H.264 码率（bit/s）
   * - ``V4L2_CID_MPEG_VIDEO_H264_MIN_QP`` / ``V4L2_CID_MPEG_VIDEO_H264_MAX_QP``
     - CODEC
     - H.264 量化参数范围
   * - ``V4L2_CID_RED_BALANCE`` / ``V4L2_CID_BLUE_BALANCE``
     - USER
     - 红/蓝色度增益
   * - ``V4L2_CID_BRIGHTNESS`` / ``V4L2_CID_CONTRAST`` / ``V4L2_CID_SATURATION`` / ``V4L2_CID_HUE``
     - USER
     - ISP 亮度/对比度/饱和度/色相
   * - ``V4L2_CID_USER_ESP_ISP_BF``
     - USER
     - ISP Bayer 滤波
   * - ``V4L2_CID_USER_ESP_ISP_CCM``
     - USER
     - ISP 色彩校正矩阵
   * - ``V4L2_CID_USER_ESP_ISP_SHARPEN``
     - USER
     - ISP 锐化
   * - ``V4L2_CID_USER_ESP_ISP_GAMMA`` / ``V4L2_CID_USER_ESP_ISP_GAMMA_EXT``
     - USER
     - ISP GAMMA；EXT 可分通道配置
   * - ``V4L2_CID_USER_ESP_ISP_DEMOSAIC``
     - USER
     - ISP 去马赛克
   * - ``V4L2_CID_USER_ESP_ISP_AWB`` / ``V4L2_CID_USER_ESP_ISP_AE`` / ``V4L2_CID_USER_ESP_ISP_AF`` / ``V4L2_CID_USER_ESP_ISP_HIST``
     - USER
     - ISP AWB / AE / AF / Histogram 窗口与统计
   * - ``V4L2_CID_USER_ESP_ISP_LSC``
     - USER
     - ISP 镜头阴影校正
   * - ``V4L2_CID_USER_ESP_ISP_BLC``
     - USER
     - ISP 黑电平校正
   * - ``V4L2_CID_USER_ESP_ISP_RAW_BYPASS``
     - USER
     - 输入输出均为 RAW8 时旁路 ISP
   * - ``V4L2_CID_CAMERA_STATS``
     - CAMERA
     - 传感器统计
   * - ``V4L2_CID_CAMERA_AE_LEVEL``
     - CAMERA
     - 传感器 AE 目标
   * - ``V4L2_CID_CAMERA_GROUP``
     - CAMERA
     - 曝光与增益组参数
   * - ``V4L2_CID_MOTOR_START_TIME``
     - CAMERA
     - 对焦电机启动时间

