****************
快速启动
****************

:link_to_translation:`en:[English]`

快速启动用于降低定时或事件唤醒抓拍的功耗与等待时间：休眠时关闭 MCU 与视频采集设备，需要时再唤醒并尽快得到亮度、颜色可用的图像。``esp_video`` 提供 ISP 流水线侧的 IPA 状态读写接口；应用把收敛后的状态放到 RTC memory（或产品自选的非易失介质）里，唤醒后在开流前写回，使第一帧复用上次已收敛的曝光、增益、白平衡与 ISP 参数。启用 ISP 流水线控制器后，保存与恢复都走这一对 API，不必逐模块下发寄存器。

目标与适用场景
--------------

快速启动主要用于休眠唤醒后的抓拍：在尽量短的时间内得到可用图像，从而缩短唤醒窗口、降低平均功耗。适用于以下场景：

1. **短时间定时抓拍**

   - 休眠间隔较短（例如数十秒到数分钟），两次抓拍之间的环境亮度、色温通常变化不大。
   - 适合直接复用休眠前缓存的 IPA 状态，往往可在第一帧得到稳定图像。

2. **长时间定时抓拍**

   - 休眠间隔较长，唤醒时的光照可能已明显变化。
   - 若仍沿用旧状态，亮度或颜色可能偏离；需结合外部光感估算，或接受多帧闭环收敛后再抓拍。

3. **随机唤醒抓拍**

   - 由按键、GPIO、网络等外部事件触发，唤醒时刻不确定，环境条件同样可能已变化。
   - 处理思路与长时间定时抓拍类似。

实现上述场景，系统需要同时具备以下能力：

1. **系统快速唤醒并进入工作状态**
2. **摄像头快速启动**
3. **采集图像的亮度、颜色等指标快速稳定**

其中第 1 项取决于 MCU 与操作系统，不在本节讨论范围。缩短系统启动与深度睡眠唤醒时间的配置，见《ESP-IDF 编程指南》的 `减少启动时间 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-guides/performance/speed.html#improving-startup-time>`__。本节主要论述第 2、第 3 项，以及当前软件提供的状态缓存与恢复能力。两者需要配合：仅缩短传感器初始化时间，而亮度和颜色仍需多帧才能收敛，仍然无法满足快速抓拍。

适用前提：输出 RAW、并由片上 ISP 与 ``esp_ipa`` 闭环控制的传感器（如 ESP32-P4）。已经在传感器内部完成彩色处理的 YUV/RGB 传感器，图像指标的稳定速度由传感器内部 ISP 决定，无法通过本节的 IPA 状态接口加速。

解决的问题
----------

未使用预置参数时，亮度和颜色依赖“统计 → 计算 → 下发”闭环，需要多帧才能稳定：

1. 若摄像头在第 **N** 帧才输出稳定图像，ISP 才能在该帧之后统计到稳定信息。
2. IPA 根据统计结果计算色温和环境亮度，再配置到 ISP 与摄像头。计算量较小时，ISP 可能在 **N + 1** 帧后输出稳定图像；计算量较大时，可能推迟到 **N + 2** 帧。
3. 摄像头曝光与增益一般在配置完成后的第 **2** 帧才生效。因此，即使传感器在第 N 帧已稳定，ISP 仍可能要到 **N + 3** 帧之后才输出稳定图像。

由此，若摄像头能在第 1 帧输出稳定图像，整条链路仍可能要到第 4 帧之后才稳定。以 1080P、每秒 30 帧为例，单帧约 33.3 ms，稳定时间约为 ``33.3 ms × 4 = 133.3 ms``。对快速启动抓拍来说，这一延迟偏长，通常难以接受。

本方案同时处理这几件事：

- 缩短唤醒后传感器寄存器初始化占用的总线时间（提高 SCCB / I2C 时钟）。
- 在环境变化不大时，用缓存的 IPA 状态跳过多帧闭环，使第一帧即可复用上次曝光、增益与白平衡。
- 写回时校验状态版本、CRC、分辨率，以及部分配置兼容性（配置版本、已加载模块、各模块调参表）。整份 IPA JSON 是否与保存时一致由应用保证；固件升级或修改 JSON 后，应用应自带配置 ID 或指纹，不一致时废弃缓存。

方案原理
--------

摄像头快速启动
^^^^^^^^^^^^^^

低功耗进入方式
""""""""""""""

摄像头进入低功耗通常有三种方式：

1. **通过 I2C 总线发送命令**：依赖摄像头硬件与驱动对休眠/待机寄存器的支持。
2. **控制 power down 引脚**：按芯片硬件特性拉高或拉低该引脚。
3. **切断数字电源和模拟电源**：从供电侧彻底断电。

方式 **3** 的降功耗效果最明显。方式 **1** 和 **2** 相对 **3** 没有明显优势：多数摄像头即使通过 **1** 或 **2** 进入低功耗，唤醒后仍需重新完成寄存器序列初始化，启动速度与断电后重新上电（方式 **3**）没有实质差别。因此，若产品允许切断传感器供电，可优先采用方式 **3**，以获得更低的待机功耗，而不必额外付出启动时间。

缩短寄存器初始化时间
""""""""""""""""""""

摄像头快速启动的关键，是缩短唤醒后写入寄存器序列的时间。寄存器通过 I2C（相机侧常称 SCCB）配置，传输耗时与时钟频率成反比。因此应 **在 MCU 与摄像头硬件共同支持的范围内，将 I2C 时钟频率设为尽可能高的值**。

以 SC2336 为例：软件默认将 I2C 时钟配置为 100 kHz，而该传感器硬件最高支持 400 kHz。将频率提高到 400 kHz，可缩短初始化阶段的总线占用时间。具体频率在 ``esp_video`` 初始化时通过 SCCB 配置中的 ``freq`` 字段设置（参见 ``esp_video_init_sccb_config_t``）。示例工程中该值的推荐范围为 100 kHz～400 kHz：100 kHz 兼容性更好，400 kHz 可减少初始化时间；线缆过长或个别传感器对时序更敏感时，过高频率可能导致通信异常，需按硬件实测选取。

图像指标快速稳定
^^^^^^^^^^^^^^^^

影响稳定速度的模块
""""""""""""""""""

图像指标中最重要的是 **颜色** 和 **亮度**，涉及如下模块：

- **颜色**：ISP 的 AWB（自动白平衡）与 CCM（色彩校正矩阵）
- **亮度**：摄像头曝光时间与增益，以及 ISP 的 LSC（镜头阴影校正）与 GAMMA

去噪、锐化等辅助模块可按固定参数启动，对稳定速度的影响可以忽略。

两种加速方案
""""""""""""

为缩短上述闭环等待，可采用两种方案，对应前面三类抓拍场景：

1. **缓存 ISP 与摄像头配置（适合短时间定时抓拍）**  
   休眠前保存当前 IPA 计算状态；唤醒并完成流水线初始化后、开流前写回，同时在运行中按需更新缓存。休眠间隔短时，环境亮度和色温往往接近上次抓拍，缓存参数通常仍适用。这是当前 ``esp_video`` / ``esp_ipa`` **已提供 API 与示例** 的路径。

2. **启动前用外部传感器估计环境（适合长时间定时抓拍、随机唤醒抓拍）**  
   休眠间隔长或唤醒时刻不确定时，环境可能已发生大的变化，不宜直接沿用旧参数。针对亮度：若使用光敏电阻等传感器，可在摄像头和 ISP 启动前估算出环境亮度，再由应用或 IPA 算出曝光、增益等并配置到摄像头。否则，基于前面的分析，只能在第 4 帧、甚至第 5 帧才能得到亮度稳定的图像。针对色温：色温传感器成本通常较高，一般只能通过实时计算得到数值，通常在第 3 到第 4 帧才能得到颜色稳定的图像。若环境亮度和色温变化不大，或即使变化较大也能接受，也可直接使用方案 1，从而在第 1 帧就得到稳定图像。

方案 1 的适用场景有限制，但效果最好，可在第 1 帧得到稳定图像。方案 2 依赖产品侧光感与标定，软件栈不强制绑定特定传感器型号。

缓存介质
""""""""

缓存介质取决于 MCU 的低功耗形态：

- **Deep sleep（深睡）**：参数可放在 RTC memory 中。读写快，适合运行中实时更新。仓库示例 ``examples/fast_startup/deep_sleep`` 用 ``RTC_DATA_ATTR`` 保存 ``esp_ipa_state_t``，并把 JPEG 序号放在同一块 RTC 数据里、位于 ``esp_ipa_state_t`` 之外，避免后续 ``get`` 清掉序号。
- **MCU 完全断电**：RTC memory 无法保持，需将参数写入 Flash 等非易失介质。Flash 读写较慢，不适合高频实时更新，应在断电前由应用调用 API 主动写入。

选择缓存方案时，应同时考虑掉电后数据是否仍有效，以及写入频率是否会增加 Flash 磨损。短时间定时抓拍且走 deep sleep 时，优先使用 RTC memory；只有在会完全断电时，才把参数落到 Flash。

状态里有什么
^^^^^^^^^^^^

``esp_ipa_state_t`` 保存的是各 IPA 模块 **已经算好的运行结果**（如曝光、增益、AWB 增益与色温、ACC/LSC/GAMMA 相关结果等），以及分辨率、配置版本、模块位图。它 **不包含** IPA JSON 正文，也没有整份配置的指纹。``get`` 与 ``set`` 之间应保持同一套 IPA 配置、同一宽度和高度，中间不要调用 ``esp_video_isp_pipeline_set_ipa_config()``（或底层的 ``esp_ipa_pipeline_set_config()``）去换参。

``set_ipa_state()`` 的兼容性检查只覆盖状态版本、CRC、分辨率、配置版本、已加载模块，以及各模块已纳入校验的调参表。版本不匹配返回 ``ESP_ERR_INVALID_VERSION``，CRC 不匹配返回 ``ESP_ERR_INVALID_CRC``，分辨率、配置版本、模块集合或调参表不匹配返回 ``ESP_ERR_INVALID_STATE``。CRC 只保护缓存本身未被改坏。两份 JSON 若配置版本和模块集合相同，且差异落在尚未纳入调参表校验的字段上，写回仍可能返回成功，并按当前配置把旧状态下发到 ISP 与摄像头。固件升级或修改 JSON 时，应用应自行保存配置 ID 或指纹，与缓存一并存放；不一致则丢弃缓存并重新收敛，不要依赖这次 API 一定失败。示例在写回失败时退回冷启动：重新开流，等到色温进入门限后再 ``get`` 并休眠，本次不把图像存到 SD 卡。

一次休眠—唤醒抓拍的实际顺序：

.. code-block:: text

    冷启动
         │
         ▼
    STREAMON → 等到色温进入门限 → STREAMOFF
         │
         ├─ 超时或收帧失败：停止。不保存 IPA 状态，不进入 deep sleep，不存图
         ▼
    get_ipa_state → 写入 RTC / Flash → deep sleep
         │
         ▼ 定时唤醒
    流水线 init → set_ipa_state
         │
         ├─ 写回被拒绝：退回冷启动，本次不存图
         ▼
    STREAMON → 第一帧编成 JPEG 存到 SD 卡 → STREAMOFF
         │
         ├─ 错误标志或空缓冲，或写文件失败：停止本次启动，不刷新 IPA，不休眠
         ▼
    再次 get_ipa_state → deep sleep
    （环境未大变时，第一帧即可复用上次的颜色与亮度）

如何使用
--------

先确认传感器为 RAW + 片上 ISP 路径，并已打开 ISP 流水线控制器，再按下面的顺序接入软件。

1. 使能 ISP 流水线控制器
^^^^^^^^^^^^^^^^^^^^^^^^^

状态读写依赖 ISP 流水线控制器与 ``esp_ipa``。

.. code-block:: none

    Component config  --->
     Espressif Video Configuration  --->
      [*] Enable ISP based Video Device  --->
         [*] Enable ISP Pipeline Controller

2. 提高传感器 SCCB 时钟（可选但推荐）
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

在传给 ``esp_video_init()`` 的 SCCB 配置里提高 ``freq``。该字段在 ``esp_video_init_sccb_config_t`` 顶层，与 ``i2c_config``（``port``、``scl_pin``、``sda_pin``）并列；取值不超过传感器手册与板级布线能力，常见范围为 100 kHz～400 kHz。

.. code-block:: c

    static const esp_video_init_sccb_config_t s_sccb_config = {
        .init_sccb = true,
        .i2c_config = {
            .port    = I2C_NUM_0,
            .scl_pin = GPIO_NUM_XX,
            .sda_pin = GPIO_NUM_XX,
        },
        .freq = 400000, /* 在硬件允许时提高，缩短寄存器初始化时间 */
    };

``examples/fast_startup/deep_sleep`` 不使用上面这份独立结构体。它调用 ``example_video_init()``，频率来自 ``EXAMPLE_MIPI_CSI_SCCB_I2C_FREQ``，即 menuconfig 的 ``CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_FREQ``（``Example Configuration`` → ``Select and Set Camera Sensor Interface`` → ``MIPI-CSI Camera Sensor`` → ``SCCB(I2C) Frequency (100K-400K Hz)``）。该项 Kconfig 默认 100000，范围 100000～400000；示例的 ``sdkconfig.defaults.esp32p4`` 已把它设为 400000。要改示例的总线频率，改这一项或该默认文件即可；另写一份未挂到 ``esp_video_init()`` 的 ``s_sccb_config`` 不会生效。

3. 收敛后保存 IPA 状态
^^^^^^^^^^^^^^^^^^^^^^

流水线已初始化且图像指标可用后，停流再读取状态。ESP32-P4 上 ISP 与 MIPI-CSI 绑定，``VIDIOC_STREAMOFF`` 后状态不再变化，适合此时落盘或写入 RTC。

.. code-block:: c

    #include "esp_video_isp_pipeline.h"

    static RTC_DATA_ATTR esp_ipa_state_t s_ipa_state;

    /* 开流、等待亮度/色温可用后： */
    ioctl(cam_fd, VIDIOC_STREAMOFF, &type);

    esp_err_t ret = esp_video_isp_pipeline_get_ipa_state(&s_ipa_state);
    if (ret != ESP_OK) {
        /* 未初始化或读取失败，不要进入依赖该状态的休眠路径 */
        return ret;
    }

    esp_deep_sleep_start();

应用自定义的“可用”判据即可。示例 ``fast_startup/deep_sleep`` 用 menuconfig 中的色温上下限与超时，通过 ``esp_video_isp_pipeline_get_env_int32("ct", &ct)`` 轮询 IPA 环境变量 ``ct``，进入范围后再 ``get`` 并休眠。

4. 唤醒后、开流前写回
^^^^^^^^^^^^^^^^^^^^^

再次 ``esp_video_init()`` 并完成采集设备与流水线初始化之后，在目标帧的 ``VIDIOC_STREAMON`` **之前** 调用写回。写回成功后再开流；失败则按冷启动重新收敛，不要带着半套旧参数开流。

.. code-block:: c

    esp_err_t ret = esp_video_isp_pipeline_set_ipa_state(&s_ipa_state);
    if (ret != ESP_OK) {
        /* ESP_ERR_INVALID_CRC / VERSION / STATE：丢弃缓存，走冷启动收敛 */
        /* ... STREAMON 并等待重新收敛 ... */
    } else {
        /* STREAMON 后第一帧即可使用恢复后的曝光、增益与 ISP 参数 */
        ioctl(cam_fd, VIDIOC_STREAMON, &type);
    }

.. list-table::
   :header-rows: 1
   :widths: 32 68

   * - API
     - 作用
   * - ``esp_video_isp_pipeline_get_ipa_state()``
     - 在流水线已初始化时读出当前计算状态，由调用方持有缓冲区。
   * - ``esp_video_isp_pipeline_set_ipa_state()``
     - 将已保存状态写回模块，并下发到 ISP 与摄像头。须在流水线 init 之后、目标帧 ``VIDIOC_STREAMON`` 之前调用。
   * - ``esp_ipa_state_t``
     - 状态载体。含 ``version``、``crc32``、``config_version``、宽高与各模块结果。写回会核对状态版本、CRC、分辨率、配置版本、模块集合和调参表；整份 IPA JSON 是否一致由应用用自有配置 ID 或指纹保证。

5. 参考示例
^^^^^^^^^^^

仓库示例 `esp_video/examples/fast_startup/deep_sleep <https://github.com/espressif/esp-video-components/tree/master/esp_video/examples/fast_startup/deep_sleep>`__ 是低功耗场景下的定时唤醒示例。它把已收敛的 IPA 状态放进 RTC memory，深度睡眠后由定时器唤醒，并在开流前写回。写回后颜色和亮度立刻按上次结果修正，从而更快得到颜色、亮度正常的一帧，再把该帧存成 JPEG。

冷启动只开摄像头，等到色温进入 menuconfig 范围后停流，调用 ``get_ipa_state`` 写入 RTC 并休眠。这条路径不挂载 SD 卡，也不存图。色温一直在范围外，或一帧没有在收帧时限内到达时，本次启动停止：不保存 IPA 状态，也不进入深度睡眠。

定时唤醒在 ``VIDIOC_STREAMON`` 之前调用 ``set_ipa_state``。写回被拒绝则退回冷启动，本次不存图。写回成功后，把相机第一次 ``DQBUF`` 得到的缓冲编成 JPEG，存为 ``/sdmmc/<Image file name>_no.<index>.jpg``。示例不丢弃帧；缓冲带错误标志或长度为 0 时停止本次启动。JPEG 质量来自 menuconfig 的 **JPEG compression quality (%)**。文件刷盘并关闭后，RTC 里的四位序号才加一；写入失败会删掉未完成的文件，序号保持不变。停流之后再次 ``get_ipa_state``，然后按 **Deep sleep time (seconds)** 进入深度睡眠。存图失败则停在休眠之前，不刷新 IPA 状态。

``sdkconfig.defaults.esp32p4`` 面向 ESP32-P4 Function EV Board v1.5 与 SC2336（1920×1080），已经使能 ISP 流水线控制器，并把 SCCB 频率设为 400 kHz。它还打开 ``CONFIG_BOOTLOADER_SKIP_VALIDATE_IN_DEEP_SLEEP``，关闭 ROM 日志，把 bootloader 日志降到 ERROR，用来缩短深度睡眠后的唤醒时间（更多选项见 `减少启动时间 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-guides/performance/speed.html#improving-startup-time>`__）。应用日志默认是 ERROR，最高是 WARN。只有 ``LOG_LOCAL_LEVEL`` 至少为 ``ESP_LOG_DEBUG`` 时，才会编译 ``fast_startup`` 和 ``ISP`` 的调试输出，并在启动时抬高这两个 tag。

注意事项
--------

- 方案 1 假设休眠前后环境接近。间隔很长或室外光照剧变时，第一帧可能偏亮、偏暗或偏色；此时应重新收敛，或叠加方案 2 的光感预估，而不是一味加大缓存命中预期。
- ``get`` / ``set`` 之间保持同一份 IPA JSON 和同一采集分辨率。分辨率，以及配置版本、模块集合、已纳入校验的调参表发生变化时，写回会失败。其余 JSON 字段变化时，``set_ipa_state()`` 仍可能成功，并把旧状态按新配置下发。升级固件或修改 JSON 后，用应用自己的配置 ID 或指纹判断缓存是否仍匹配，不匹配就废弃并重新收敛。
- ``esp_ipa_state_t`` 体积较大（各模块数组按最大容量存放）。放入 RTC 前确认剩余 RTC 内存足够；完全断电场景再考虑 Flash，并控制写入频率。
- 写回只恢复计算状态并清空部分帧计数与延时时钟；滤光片、补光灯等产品外设仍由应用按自身策略控制。
- 提高 SCCB 频率与状态缓存彼此独立：前者缩短“传感器能出图”的时间，后者缩短“图可用”的时间。只做其中一项往往不够。
