| Supported Targets | ESP32-P4 |
| ----------------- | -------- |

# 基于 Light Sleep 的软件 AOV 示例

本示例在 ESP32-P4 上按周期采集一帧：初始化并等待 IPA 色温进入范围后采集，再按 menuconfig 中的时间进入 Light Sleep。默认只采集，不编码，不写入存储介质，也不做识别或分析。睡眠时间的单位为毫秒，从 `esp_light_sleep_start()` 开始计算。一轮的时间是采集、停流、睡眠和再次开流之和。

这是软件 AOV，不是硬件原生 AOV。采一帧再睡眠由应用完成，不要求传感器提供硬件 AOV 模式，因此除默认的 MIPI-CSI 外，也可以改接到 DVP 或其他接口的摄像头；换接口时替换初始化路径，并重新测量功耗。相邻两帧之间包含停流、睡眠和再次开流，这段时间没有图像。

Light sleep 不复位，不调用 `esp_video_isp_pipeline_get_ipa_state()` 或 `esp_video_isp_pipeline_set_ipa_state()`。停流会释放 ISP 与 CSI，下一轮开流按 RAM 中的状态重建。与 deep sleep 的差别、重建路径及功耗数据见[软件 AOV](../../../../docs/zh_CN/Solutions/software_aov.rst)。

`keep_camera_power_domains()` 把 TOP、CNNT 设为 `ESP_PD_OPTION_ON`。ESP-IDF v5.5.6 只在域未被强制为 ON 时才允许 TOP 掉电，且只有 TOP 已掉电才允许 CNNT 掉电，因此 `Power down Digital Peripheral in light sleep` 不会关掉这两个域。支持 CPU retention 的 ESP32-P4（非 v3.0 之前）上，`Power down CPU in light sleep` 会保存并恢复 CPU 上下文；`cpu_domain_pd_allowed()` 还要求 retention 缓冲区已分配。IPA 与驱动状态留在 RAM，不依赖关闭这两个选项。本示例只在两个选项都关闭时测过。细节见[软件 AOV](../../../../docs/zh_CN/Solutions/software_aov.rst)。

本示例仅支持 ESP32-P4，需要 ESP-IDF v5.5.2 及以上。v5.5.0 和 v5.5.1 的 `esp_sleep.h` 只有 `esp_sleep_get_wakeup_cause()`，没有本示例使用的 `esp_sleep_get_wakeup_causes()`。默认传感器为 SC2336，以 RAW8 1920x1080 30fps 送入 CSI。应用不调用 `VIDIOC_S_FMT`，`/dev/video0` 沿用该模式的默认 RGB565 输出，并启用 ISP Pipeline Controller。

## 如何使用示例

### 硬件需求

* ESP32-P4 开发板（推荐 ESP32-P4-Function-EV-Board V1.5）
* 已支持的 MIPI-CSI 摄像头模组（默认 SC2336）
* USB Type-C 线（供电与烧录）

板级 I2C、复位等引脚见 [example_video_common](../../common_components/example_video_common/README.md)。在 ESP32-P4-Function-EV-Board 的 MIPI-CSI 默认路径下，SCCB 一般为 SDA=GPIO7、SCL=GPIO8。

> **注意：** 标记使用 GPIO4、GPIO5、GPIO6。改用 DVP 或 SPI 摄像头时，板级引脚可能与这些 GPIO 冲突。

### GPIO 标记

定义见 `main/software_aov_light_sleep_main.c`。三个引脚输出短脉冲：操作开始前置 1，操作返回后置 0。标记在色温等待完成之后才初始化，因此进入循环之前的第一次开流没有 GPIO4 脉冲。

| GPIO | 宏名 | 方向 | 作用 |
| ---- | ---- | ---- | ---- |
| GPIO4 | `GPIO_MARKER_STREAM_ON` | 输出 | 醒来后、开流之前置 1，开流返回后置 0 |
| GPIO5 | `GPIO_MARKER_CAPTURE` | 输出 | `DQBUF` 之前置 1，采集函数返回后置 0 |
| GPIO6 | `GPIO_MARKER_STREAM_OFF` | 输出 | 停流之前置 1，停流返回后置 0 |

### 配置工程

板型、摄像头接口、GPIO 和时钟见 [example_video_common](../../common_components/example_video_common/README.md)。

```
Example Configuration  --->
    Light sleep time (ms)
    Minimum color temperature (K)
    Maximum color temperature (K)
    Color temperature wait timeout (ms)
    Frame receive timeout (ms)
```

`CONFIG_EXAMPLE_SLEEP_TIME_MS` 默认 1000，范围 1～86400000，单位毫秒。色温上下限只用于初始化后的第一次开流。超时或收帧失败时，不采集本轮帧，不进入 Light Sleep。

```
Component config  --->
    Espressif Camera Sensors Configurations  --->
        [*] SC2336  ---->
            Default format select for MIPI (...)  --->
                (X) RAW8 1920x1080 30fps, MIPI 2lane ...

    Espressif Video Configuration  --->
        [*] Enable ISP based Video Device  --->
            [*] Enable ISP Pipeline Controller
```

`sdkconfig.defaults.esp32p4` 已写入上述传感器与 ISP 选项。其中：

* `CONFIG_BOOTLOADER_LOG_LEVEL_ERROR=y`：当前 esp-idf 的 bootloader 在默认 Info 日志下会超过分区表偏移 `0x8000` 允许的 24 KB
* `CONFIG_BOOT_ROM_LOG_ALWAYS_OFF=y`
* `CONFIG_ESP_VIDEO_STREAM_STOP_DRAIN_FRAMES=0`
* Flash 为 QIO、16 MB，PSRAM 速率 250 MHz，SCCB 频率 400 kHz，应用日志为 error / warn

`CONFIG_ESP_VIDEO_STREAM_STOP_DRAIN_FRAMES=0` 的影响见方案文档。若再次开流出现 data type、行号或 FIFO 错误，将其改回 1。

### 编译与烧录

```
cd esp_video/examples/software_aov/light_sleep
idf.py set-target esp32p4
idf.py -p PORT flash monitor
```

退出串口监视器：``Ctrl-]``。更多步骤见 [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/index.html)。

默认日志级别为 error / warn。需要查看下列 INFO 日志时，把 Default log verbosity 和 Maximum log verbosity 提高到 Info 后重新编译。

```
I (xx) software_aov: wait until color temperature is in range
I (xx) software_aov: color temperature 5000 K is in range
I (xx) software_aov: Captured one frame, size=4147200
I (xx) software_aov: Enter light sleep for 1000 ms
I (xx) software_aov: Woke up from light sleep, cause=0x10 timer, slept 1000 ms
I (xx) software_aov: Captured one frame, size=4147200
```

默认 `DQBUF.bytesused` 为 4,147,200 字节，即 1920×1080 RGB565（每像素 2 字节）。这是 `/dev/video0` 的输出大小，不是传感器 RAW8 输入的 2,073,600 字节。改用其他格式时，以 `VIDIOC_G_FMT` 的协商结果为准。

`cause` 中带有 timer 时，数值是 `ESP_SLEEP_WAKEUP_TIMER` 的位掩码。`slept` 由 `esp_timer` 测量，包含进入和退出睡眠的开销，略大于配置值。

ESP32-P4 的 USB Serial/JTAG 在 Light Sleep 期间会断开，醒来后重新枚举。睡眠前后各有一小段日志可能看不到。若要在睡眠期间保持串口，请改用外部 UART，并在 menuconfig 中把 console 设为 Custom UART：

```
Component config ---> ESP-STDIO ---> Channel for console output ---> Custom UART
```

## 示例流程

1. **初始化视频子系统**  
   调用 `example_video_init()`，打开 `/dev/video0`，申请 3 个 V4L2 buffer 并完成一次 `mmap`。随后把 TOP、CNNT 电源域设为睡眠期间保持供电。

2. **等待色温**  
   排队并 `STREAMON`。持续收帧，直到色温落入 menuconfig 范围。此阶段不停流。超时、收帧失败，或随后的 GPIO 初始化失败，都先 `STREAMOFF` 再关闭设备。停流失败时不关闭设备、不释放缓冲区。不进入 Light Sleep。

3. **采集一帧并停流**  
   在已经打开的数据流上 `DQBUF` 一帧。`V4L2_BUF_FLAG_DONE` 置位后记为本轮结果，然后 `STREAMOFF`。

4. **Light Sleep**  
   `esp_sleep_enable_timer_wakeup()` 使用 `CONFIG_EXAMPLE_SLEEP_TIME_MS` 换算的微秒数，然后调用 `esp_light_sleep_start()`。

5. **唤醒后继续**  
   函数返回后重新 `QBUF` 并 `STREAMON`，回到步骤 3。不再等待色温。映射在打开设备时完成，循环中不再 `mmap`。

## 源码说明

主文件：`main/software_aov_light_sleep_main.c`  
工程名：`software_aov_light_sleep`

| 宏 / 项 | 默认值 | 说明 |
| ------- | ------ | ---- |
| `CONFIG_EXAMPLE_SLEEP_TIME_MS` | 1000 | Light Sleep 定时器时间，单位毫秒 |
| `BUFFER_COUNT` | 3 | V4L2 buffer 个数 |
| `CAM_DEV_PATH` | `EXAMPLE_CAM_DEV_PATH`（MIPI-CSI 时为 `/dev/video0`） | 摄像头设备路径 |
| `GPIO_MARKER_*` | 4 / 5 / 6 | 开流、采集、停流的脉冲标记 |

* 采集函数不调用 `VIDIOC_S_FMT`。默认帧大小见上文日志。
* 完全断电或复位后 RAM 丢失，下次上电会重新等待色温。
* 睡眠间隔很长、光照变化很大时，RAM 中保留的曝光和颜色参数可能不再适合当前环境。
* 本示例不写入存储介质，也不编码。

## 检查清单

* 目标芯片：ESP32-P4
* 默认摄像头：SC2336 MIPI-CSI，`/dev/video0`
* 已测配置：关闭 `Power down CPU in light sleep` 和 `Power down peripherals in light sleep`。TOP、CNNT 由示例保持上电
* 唤醒源：定时器，时间见 `Example Configuration` → `Light sleep time (ms)`
* 色温只在初始化之后等待一次
* 循环顺序为：GPIO5 脉冲（采集）、GPIO6 脉冲（停流）、睡眠、GPIO4 脉冲（开流）
