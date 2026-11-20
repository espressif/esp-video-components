**********
软件 AOV
**********

:link_to_translation:`en:[English]`

软件 AOV（Software AOV）按固定周期采集图像：完成一帧后进入睡眠，定时器到期后再唤醒并采集下一帧。睡眠期间关闭不需要的电路，从而降低平均功耗。

方案范围
--------

本方案是软件 AOV，不是传感器或 SoC 里的硬件原生 AOV。采一帧、停流、睡眠和再次开流都由应用调度，不要求摄像头提供硬件 AOV 模式。因此实现和部署更灵活：除 MIPI-CSI 外，也可以接到 DVP 以及其他接口的摄像头上。``esp_video`` 在 ESP32-P4 上给出 deep sleep 与 light sleep 两种示例，默认传感器经 MIPI-CSI 输出 RAW，并由片上 ISP 与 ``esp_ipa`` 闭环控制。换接口时保留这套调度，把摄像头初始化换成对应驱动。文中关于 ISP、CSI 控制器重建和功耗的数据对应该 MIPI-CSI 示例，换接口后需要重新测量。传感器内部已经完成彩色处理的 YUV/RGB 通路，图像指标由传感器自身决定，不在本方案范围内。

软件调度使相邻监控帧之间存在时间空隙。空隙包含停流、睡眠和再次开流；deep sleep 还包括芯片启动与摄像头初始化。这段时间没有图像输出，空隙内的场景变化不会被采集到。空隙长度随配置的睡眠时间和唤醒路径变化，不是连续视频里均匀的帧间隔。

本文只说明这种软件低帧率采集和睡眠，不涉及运动检测、目标识别或其他分析。IPA 状态的内容、兼容性检查和接口约定见 :doc:`fast_startup`。下文只说明软件 AOV 周期里何时需要保存和写回，以及两种睡眠对 RAM 中的软件状态、每次开流的硬件重建、启动时间和功耗的影响。

目标与适用场景
--------------

软件 AOV 用于不需要连续预览、但需要按周期留下图像，并且可以接受帧间空隙的产品，例如电池供电的摄像机、门铃和定时巡检。采集间隔由应用配置，示例中的单位为毫秒。

选择睡眠方式时，同时看两件事：

1. **唤醒后到得到可用图像的时间**，包括芯片启动、摄像头初始化和图像指标收敛。
2. **睡眠期间的电流**。间隔越长，睡眠电流在整周期平均电流中的比重越高。

Deep sleep 与 light sleep
-------------------------

.. list-table::
   :header-rows: 1
   :widths: 22 39 39

   * - 项目
     - Deep sleep
     - Light sleep
   * - 唤醒后的执行位置
     - 芯片复位，从 bootloader 和 ``app_main()`` 重新执行
     - ``esp_light_sleep_start()`` 返回后，从调用点的下一行继续执行
   * - 普通 RAM
     - 掉电丢失
     - 保持
   * - RTC memory
     - 保持，用于存放 ``esp_ipa_state_t``
     - 本路径不使用
   * - ISP 与 CSI 硬件
     - 掉电。每轮都要重新 ``example_video_init()``，并重新打开设备
     - 每轮 ``VIDIOC_STREAMOFF`` 删除 ISP 处理器和 CSI 控制器。不必重新 ``example_video_init()``。下一轮 ``VIDIOC_STREAMON`` 重新创建二者，并从 RAM 中的驱动状态写回配置
   * - IPA 状态
     - 冷启动调用 ``esp_video_isp_pipeline_get_ipa_state()`` 写入 RTC memory。定时器唤醒后，在 ``VIDIOC_STREAMON`` 之前调用 ``esp_video_isp_pipeline_set_ipa_state()``
     - 不调用上述接口。IPA 与 ISP 驱动的软件状态留在 RAM。色温只等待一次，之后各帧由 ``STREAMON`` 把这份状态写进新的处理器
   * - 一轮的时间构成
     - 启动、摄像头初始化、采集、睡眠
     - 采集、停流、睡眠、再开流。没有 bootloader，也不重新 ``example_video_init()``
   * - 睡眠电流
     - 低
     - 高。RAM 以及保持上电的 TOP、CNNT 域仍耗电
   * - 更合适的间隔
     - 间隔较长，平均电流优先
     - 间隔较短，唤醒延迟优先

Deep sleep
^^^^^^^^^^

Deep sleep 会关掉 CPU、普通 RAM 和大部分外设。定时器到期后芯片复位，IPA 计算过程中的普通变量全部消失，只有放在 RTC memory 中的 ``esp_ipa_state_t`` 还在。

因此第一轮上电不能把正在收敛的图像当作可用帧。冷启动打开数据流，直到 IPA 色温进入应用设定的范围，然后停流，调用 ``esp_video_isp_pipeline_get_ipa_state()`` 写入 ``RTC_DATA_ATTR`` 对象，再进入 deep sleep。这一轮没有可用帧。

定时器唤醒并完成流水线初始化之后、``VIDIOC_STREAMON`` 之前，调用 ``esp_video_isp_pipeline_set_ipa_state()`` 把同一份状态写回 ISP 和摄像头。写回成功后，紧接着 ``DQBUF`` 得到的第一帧使用的是已经收敛的曝光、增益和颜色参数，这一帧可以作为本轮结果。未写回就开流时，第一帧仍在等待统计闭环，亮度和颜色都不可用。写回失败时退回冷启动路径：重新收敛并保存，本轮不把图像当作可用帧。

停流之后再次 ``get_ipa_state()``，使下一轮使用最新结果。``get`` 与 ``set`` 之间须保持同一份 IPA JSON、同一宽度和高度。版本、CRC、分辨率或配置不匹配时，写回会被拒绝，返回值见 :doc:`fast_startup`。

睡眠时间从 ``esp_deep_sleep_start()`` 开始计算。完整周期是启动、初始化、采集与睡眠时间之和，长于单独配置的睡眠时间。缩短启动时间可以减小每轮的有效工作窗口，相关配置包括 ``CONFIG_BOOTLOADER_SKIP_VALIDATE_IN_DEEP_SLEEP``、关闭 ROM 日志，以及把 PSRAM 时序参数缓存在 RTC memory 中。后者依赖特定 esp-idf 提交上的补丁，步骤见 deep sleep 示例。

.. code-block:: text

    冷启动
         │
         ▼
    初始化视频 → STREAMON → 色温进入范围 → STREAMOFF
         │
         ├─ 超时或收帧失败：停止。不保存 IPA 状态，不进入 deep sleep
         ▼
    get_ipa_state → 写入 RTC memory → deep sleep
         │
         ▼ 定时器唤醒（芯片复位）
    初始化视频 → set_ipa_state
         │
         ├─ 写回被拒绝：退回冷启动
         ▼
    STREAMON → 第一帧即为本轮结果 → STREAMOFF
         │
         ▼
    再次 get_ipa_state → deep sleep

Light sleep
^^^^^^^^^^^

Light sleep 不复位。``esp_light_sleep_start()`` 返回后，栈、堆和全局变量都还在，程序从调用点之后继续执行。IPA 流水线和 ISP 驱动里的软件状态因此留在 RAM，不必把 ``esp_ipa_state_t`` 放进 RTC memory，也不调用 ``esp_video_isp_pipeline_get_ipa_state()`` 或 ``esp_video_isp_pipeline_set_ipa_state()``。

每轮睡眠前的 ``VIDIOC_STREAMOFF`` 会拆掉本轮硬件。``common_video_stop_impl()`` 先停止传感器输出，再调用 ``csi_video_stop()`` 删除 ISP 处理器，然后 ``esp_cam_ctlr_del()`` 删除 CSI 控制器。下一轮 ``VIDIOC_STREAMON`` 经 ``csi_video_start()`` 重新创建 CSI 控制器，并调用 ``esp_isp_new_processor()`` 新建 ISP 处理器。ESP-IDF v5.5.6 在申请处理器时执行 ``isp_ll_reset_module_clock()``，复位 ISP 模块。接着 ``isp_start_pipeline()`` 用 RAM 中仍在的驱动状态配置新处理器，其中 ``isp_start_ccm()`` 会再次写入 CCM。停流之后，上一轮写进硬件的配置已经随处理器和控制器一起释放。

下面两个 menuconfig 选项不决定这份 RAM 状态是否还在。

``keep_camera_power_domains()`` 把 TOP 和 CNNT 设为 ``ESP_PD_OPTION_ON``。ESP-IDF v5.5.6 的 ``get_power_down_flags()`` 只在该域不是 ``ESP_PD_OPTION_ON`` 且 ``top_domain_pd_allowed()`` 为真时置 TOP 掉电标志；CNNT 还要求 TOP 掉电标志已经置位。因此 ``Power down Digital Peripheral in light sleep`` 不会关掉示例已经强制保电的 TOP 和 CNNT。Kconfig 还写明 ``esp_sleep_pd_config()`` 的优先级高于该选项。ESP32-P4 芯片修订下限为 v3.0 且启用 PSRAM 时，这个外设掉电选项本身不可选。

在未定义 ``ESP32P4_SELECTS_REV_LESS_V3`` 的 ESP32-P4 上，``Power down CPU in light sleep``（``PM_POWER_DOWN_CPU_IN_LIGHT_SLEEP``）于睡眠前保存 CPU 运行上下文，醒来后恢复。``cpu_domain_pd_allowed()`` 要求各核的 critical、non-critical 和 CLIC retention 缓冲区都已分配，才允许 CPU 域掉电。v3.0 之前的 P4 没有这项独立选项，``get_power_down_flags()`` 里的 CPU 域掉电标志也被编译掉。

本方案的流程和文中功耗数据是在这两个选项都关闭时记录的。打开其中任一项之后的电流和唤醒结果没有在本示例上测量。下一轮仍通过 ``STREAMON`` 按 RAM 里的软件状态重建 ISP 和 CSI。

色温只在 ``example_video_init()`` 之后等待一次：开流，持续收帧，直到 ``esp_video_isp_pipeline_get_env_int32("ct")`` 落入设定范围。此阶段不停流。随后直接 ``DQBUF`` 取一帧，作为本轮结果，再 ``VIDIOC_STREAMOFF``。停流让传感器停止向 ISP 送数，并释放本轮的 ISP 处理器和 CSI 控制器。``example_video_deinit()`` 不在这个循环里调用，应用打开的设备 fd 和已映射的缓冲仍然保留。

``VIDIOC_STREAMOFF`` 会清空驱动中的 queued 列表和 done 列表。映射可以保留，但下一次 ``VIDIOC_STREAMON`` 之前必须重新 ``VIDIOC_QBUF``。只在打开设备时排队一次，第二轮开流将没有可用缓冲。

醒来后重新排队并开流。开流重建 ISP 处理器和 CSI 控制器，并把 RAM 中的配置写入新硬件，再 ``DQBUF`` 一帧，然后停流、睡眠。循环中不再等待色温。

.. code-block:: text

    初始化视频 → 保持 TOP、CNNT 上电 → 打开设备并映射缓冲
         │
         ▼
    STREAMON → 色温进入范围（不停流）
         │
         ├─ 超时或收帧失败：STREAMOFF 后退出。不进入 light sleep
         ▼
    DQBUF 一帧 → STREAMOFF（释放 ISP 处理器和 CSI 控制器） → light sleep
         │
         ▼ 定时器唤醒（函数返回，RAM 中的 IPA 与驱动状态仍在）
    重新 QBUF → STREAMON（新建处理器和控制器，并写回 CCM 等配置）
         → DQBUF 一帧 → STREAMOFF → light sleep

``CONFIG_ESP_VIDEO_STREAM_STOP_DRAIN_FRAMES`` 默认在停流后等待一个帧周期，使传感器退出当前帧之后再关闭 ISP，避免下一次开流出现 data type、行号或 FIFO 错误。示例的 ``sdkconfig.defaults.esp32p4`` 将其设为 0，停流立即返回，软件 AOV 周期更短。若再次开流出现上述错误，应把该值改回 1 后重新测量。

两种路径的取舍
^^^^^^^^^^^^^^

两种模式在运行期间的电流接近。Deep sleep 的睡眠电流低，每一轮包含 bootloader、PSRAM 初始化和摄像头寄存器序列。Light sleep 不包含这段启动，睡眠电流明显高于 deep sleep，数值见下文。间隔较长时，deep sleep 的平均电流更低；间隔缩短到与启动时间相近时，light sleep 的唤醒延迟更小，平均电流应在目标板上重新测量。

完全断电后，两种路径保存的运行状态都不再有效。下次上电按冷启动重新等待色温。睡眠间隔很长、光照变化很大时，沿用的曝光和颜色参数可能不再适合当前环境，画面会偏亮、偏暗或偏色。此时应重新收敛，deep sleep 路径还要放弃 RTC 中的旧状态。

功耗数据
--------

下表由文档作者使用自有仪器、在特定 ESP32-P4 核心开发板上测得，仅供参考。原始记录未记载仪器型号和核心板料号。更换开发板、传感器、分辨率、帧率或睡眠时间后，应重新测量。

.. list-table::
   :header-rows: 1
   :widths: 22 26 26 26

   * - 模式
     - 睡眠期间平均电流
     - 运行期间平均电流
     - 整周期平均电流
   * - Deep sleep
     - 0.46 mA
     - 42.5 mA
     - 4.66 mA
   * - Light sleep
     - 3.86 mA
     - 41.14 mA
     - 5.76 mA

表中的运行期间平均电流是系统处于采集窗口时的平均值，不是峰值。在作者的这次测试中，light sleep 的整周期平均电流高于 deep sleep。

参考示例
--------

- `esp_video/examples/software_aov/deep_sleep <https://github.com/espressif/esp-video-components/tree/master/esp_video/examples/software_aov/deep_sleep>`__：定时器唤醒的 deep sleep 循环。冷启动保存 IPA 状态，唤醒后在开流前写回。GPIO4、GPIO5、GPIO6 分别标记进入应用、视频初始化和可用帧。
- `esp_video/examples/software_aov/light_sleep <https://github.com/espressif/esp-video-components/tree/master/esp_video/examples/software_aov/light_sleep>`__：不复位的 light sleep 循环。色温只等待一次；GPIO4、GPIO5、GPIO6 以脉冲标记开流、采集和停流。

两个示例需要 ESP-IDF v5.5.2 及以上，默认使用 SC2336、RAW8 1920×1080、30 fps 和 ISP Pipeline Controller，设备节点为 ``/dev/video0``。睡眠时间在 menuconfig 中配置，单位为毫秒，默认 1000。示例只做低帧率采集和睡眠，不编码，不写存储介质，也不做识别或分析。操作步骤、接线和环境要求见各自的 README。

注意事项
--------

- 本方案是软件 AOV。帧与帧之间有停流、睡眠和再次开流造成的时间空隙，空隙内没有图像。本文不描述识别或分析。
- 示例默认使用 MIPI-CSI。同一套采集与睡眠调度可以改接到 DVP 或其他接口，换接口后启动时间和功耗需重新测量。
- Deep sleep 的 ``esp_ipa_state_t`` 体积较大。放入 RTC memory 前确认剩余空间足够。完全断电后这块数据丢失。
- Light sleep 示例在打开设备时映射缓冲，每次开流只重新排队。不要在每一轮 ``mmap`` 而不 ``munmap``。
- USB Serial/JTAG 在两种睡眠期间都会断开。需要在睡眠过程中持续查看日志时，改用外部 UART，并把 console 设为 Custom UART。
- 阶段耗时与 esp-idf 版本、是否应用 deep sleep 示例中的 PSRAM 补丁有关。请用示例提供的 GPIO 标记测量，不要把其他提交上的波形时间当作本示例的规格。
