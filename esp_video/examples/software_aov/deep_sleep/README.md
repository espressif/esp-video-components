| Supported Targets | ESP32-P4 |
| ----------------- | -------- |

# 基于 Deep Sleep 的软件 AOV 示例

本示例在 ESP32-P4 上按周期采集一帧：定时器唤醒后完成摄像头初始化并采集，然后按 menuconfig 中的时间进入 Deep Sleep。默认只采集，不编码，不写入存储介质，也不做识别或分析。睡眠时间的单位为毫秒，从 `esp_deep_sleep_start()` 开始计算。

这是软件 AOV，不是硬件原生 AOV。采一帧再睡眠由应用完成，不要求传感器提供硬件 AOV 模式，因此除默认的 MIPI-CSI 外，也可以改接到 DVP 或其他接口的摄像头；换接口时替换初始化路径，并重新测量启动时间和功耗。相邻两帧之间包含睡眠和再次启动，这段时间没有图像。

冷启动与定时器唤醒的差别、IPA 状态写回条件，以及功耗数据，见[软件 AOV](../../../../docs/zh_CN/Solutions/software_aov.rst)。

本示例仅支持 ESP32-P4，需要 ESP-IDF v5.5.2 及以上。v5.5.0 和 v5.5.1 的 `esp_sleep.h` 只有 `esp_sleep_get_wakeup_cause()`，没有本示例使用的 `esp_sleep_get_wakeup_causes()`。默认传感器为 SC2336，以 RAW8 1920x1080 30fps 送入 CSI。应用不调用 `VIDIOC_S_FMT`，`/dev/video0` 沿用该模式的默认 RGB565 输出，并启用 ISP Pipeline Controller。启动耗时随 esp-idf 版本变化。下文的补丁针对 tag `v5.5.6`。换用其他版本时，请用 GPIO 标记重新测量，不要沿用其他版本上的时间。

## 如何使用示例

### 软件需求

将 esp-idf 切换到 tag `v5.5.6`，再在 IDF 根目录应用本目录的补丁：

```
cd $IDF_PATH
git fetch --tags
git checkout v5.5.6
git apply /path/to/esp_video/examples/software_aov/deep_sleep/patch/0001-feat-esp_hw_support-Speed-up-PSRAM-initialization-by.patch
```

`patch/0001-feat-esp_hw_support-Speed-up-PSRAM-initialization-by.patch` 按 tag `v5.5.6` 的 `esp_hw_support` 生成，把 PSRAM 时序参数放入 RTC memory。其他版本可能无法直接应用。补丁的作用见方案文档。

应用补丁后，在 menuconfig 中打开：

```
Component config  --->
    ESP PSRAM  --->
        [*] Cache PSRAM MSPI timing tuning results in RTC memory
```

官方 v5.5.6 没有这一项。未打补丁时菜单里不会出现，不要把它写进 `sdkconfig`。

### 硬件需求

* ESP32-P4 开发板（推荐 ESP32-P4-Function-EV-Board V1.5；本示例 GPIO 按此板配置）
* 已支持的 MIPI-CSI 摄像头模组（默认 SC2336）
* USB Type-C 线（供电与烧录）
* 逻辑分析仪（可选，用于测量各阶段耗时）

板级 I2C、复位等引脚见 [example_video_common](../../common_components/example_video_common/README.md)。在 ESP32-P4-Function-EV-Board 的 MIPI-CSI 默认路径下，SCCB 一般为 SDA=GPIO7、SCL=GPIO8。

> **注意：** 本示例默认关闭 DVP。标记使用 GPIO4、GPIO5、GPIO6。改用 DVP 或 SPI 摄像头时，板级引脚可能与这些 GPIO 冲突，须修改源码宏或板级配置。

### GPIO 标记

定义见 `main/software_aov_deep_sleep_main.c`。三个引脚在应用内保持高电平，进入 Deep Sleep 后由硬件掉电回到低电平。唤醒源是 RTC 定时器，没有唤醒引脚。

| GPIO | 宏名 | 方向 | 作用 |
| ---- | ---- | ---- | ---- |
| GPIO4 | `GPIO_MARKER_APP_MAIN` | 输出 | `init_marker_gpios()` 完成后置 1，表示已进入应用主流程 |
| GPIO5 | `GPIO_MARKER_INIT_DONE` | 输出 | `example_video_init()` 成功后置 1 |
| GPIO6 | `GPIO_MARKER_CAPTURE_DONE` | 输出 | `set_ipa_state()` 成功且第一帧 `DQBUF` 完成时置 1。冷启动保持 0 |

### 配置工程

板型、摄像头接口、GPIO 和时钟见 [example_video_common](../../common_components/example_video_common/README.md)。默认板型为 ESP32-P4-Function-EV-Board V1.5。

```
Example Configuration  --->
    Deep sleep time (ms)
    Minimum color temperature (K)
    Maximum color temperature (K)
    Color temperature wait timeout (ms)
    Frame receive timeout (ms)
```

`CONFIG_EXAMPLE_SLEEP_TIME_MS` 默认 1000，范围 1～86400000，单位毫秒。色温上下限只用于冷启动。超时或收帧失败时，不保存 IPA 状态，不进入 Deep Sleep。

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

`sdkconfig.defaults.esp32p4` 已写入上述传感器与 ISP 选项，并包含：

* `CONFIG_BOOTLOADER_SKIP_VALIDATE_IN_DEEP_SLEEP=y`
* `CONFIG_BOOT_ROM_LOG_ALWAYS_OFF=y`，bootloader 与应用日志降为 error / warn
* `CONFIG_ESP_VIDEO_STREAM_STOP_DRAIN_FRAMES=0`
* Flash 为 QIO、16 MB，PSRAM 速率 250 MHz，SCCB 频率 400 kHz

`CONFIG_ESP_VIDEO_STREAM_STOP_DRAIN_FRAMES=0` 的影响见方案文档。若开流后出现 data type、行号或 FIFO 错误，将其改回 1。

也可在 menuconfig 中选用更低分辨率或更高帧率，以缩短出图时间。

### 增加二级 Bootloader 标记（可选）

若需在逻辑分析仪上标记二级 bootloader 入口，可在 IDF 的 `components/bootloader/subproject/main/bootloader_start.c` 中，于 `call_start_cpu0` 入口加入：

```c
#include "hal/gpio_ll.h"

int pin = 36;
gpio_ll_output_enable(&GPIO, pin);
gpio_ll_set_level(&GPIO, pin, 0);
```

### 编译与烧录

```
cd esp_video/examples/software_aov/deep_sleep
idf.py set-target esp32p4
idf.py -p PORT flash monitor
```

退出串口监视器：``Ctrl-]``。更多步骤见 [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/index.html)。

默认日志级别为 error / warn，下面的 INFO 日志不一定出现。需要查看时，把 Default log verbosity 和 Maximum log verbosity 提高到 Info 后重新编译。

冷启动：

```
I (xx) software_aov: Software AOV boot, wakeup cause=0x1
I (xx) software_aov: cold boot, converge IPA before saving state
I (xx) software_aov: color temperature 5000 K is in range
I (xx) software_aov: saved IPA state
I (xx) software_aov: Enter deep sleep, timer wakeup in 1000 ms
```

定时器唤醒。`restored IPA state, first frame is usable` 表示写回成功，随后的 `Captured one usable frame` 为本轮结果：

```
I (xx) software_aov: Software AOV boot, wakeup cause=0x10 timer
I (xx) software_aov: restored IPA state, first frame is usable
I (xx) software_aov: Captured one usable frame, size=4147200
I (xx) software_aov: saved IPA state
I (xx) software_aov: Enter deep sleep, timer wakeup in 1000 ms
```

冷启动为 `0x1`（`BIT(ESP_SLEEP_WAKEUP_UNDEFINED)`），没有 `timer` 后缀。只启用定时器时为 `0x10`（`BIT(ESP_SLEEP_WAKEUP_TIMER)`），并带有 `timer` 后缀。毫秒数来自 `CONFIG_EXAMPLE_SLEEP_TIME_MS`。

默认 `DQBUF.bytesused` 为 4,147,200 字节，即 1920×1080 RGB565（每像素 2 字节）。这是 `/dev/video0` 的输出大小，不是传感器 RAW8 输入的 2,073,600 字节。改用其他格式时，以 `VIDIOC_G_FMT` 的协商结果为准。

> **注意：** `/dev/ttyACM0`（USB Serial/JTAG）在 Deep Sleep 后不可用。若需在睡眠和唤醒期间持续观察串口，请使用外接 USB 转串口模块，并在 menuconfig 中将 console 改为 Custom UART：
>
> ```
> Component config ---> ESP-STDIO ---> Channel for console output ---> Custom UART
> ```

## 示例流程

1. **初始化标记 GPIO**  
   GPIO4、GPIO5、GPIO6 配为输出并使能内部下拉。GPIO5、GPIO6 置 0，GPIO4 置 1。

2. **初始化视频子系统**  
   `example_video_init()` 成功后将 GPIO5 置 1。GPIO4 到 GPIO5 的间隔约为视频初始化耗时。

3. **冷启动**  
   上电，或 `set_ipa_state()` 被拒绝时走此路径。打开 `/dev/video0` 并开流，直到色温进入 menuconfig 范围，然后停流。GPIO6 保持 0。停流后 `esp_video_isp_pipeline_get_ipa_state()` 写入 RTC memory。色温超时或收帧失败时，本次启动结束：不保存状态，不进入 Deep Sleep。

4. **定时器唤醒**  
   缓冲已经排队，但尚未 `STREAMON`。`esp_video_isp_pipeline_set_ipa_state()` 成功后再开流，`DQBUF` 的第一帧完成后将 GPIO6 置 1。写回失败则退回步骤 3，本轮不将 GPIO6 置 1。

5. **再次保存并进入 Deep Sleep**  
   停流后再次 `get_ipa_state()`。然后调用 `esp_sleep_enable_timer_wakeup()` 和 `esp_deep_sleep_start()`。唤醒时间由 `CONFIG_EXAMPLE_SLEEP_TIME_MS` 换算为微秒。定时器到期后芯片复位，从步骤 1 重新执行。

## 使用逻辑分析仪测量

### 接线

| 分析仪通道 | 连接脚 | 说明 |
| ---------- | ------ | ---- |
| CH-A | GPIO4 | APP_MAIN |
| CH-B | GPIO5 | INIT_DONE |
| CH-C | GPIO6 | CAPTURE_DONE |

* 分析仪 GND 必须与开发板 GND 共地。
* 不需要为唤醒接按键或上拉。上电后示例自行进入 Deep Sleep，到时再唤醒。
* 通道阈值按 3.3 V 逻辑设置。采样率 10 kSa/s～100 kSa/s 通常足够。记录时长至少覆盖两轮。默认睡眠 1000 ms，加上启动和采集，一轮约为数秒。
* 可用 GPIO4 或 GPIO6 的上升沿触发。观察 GPIO6 时打开触发前采样。
* 探头须耐受 3.3 V，不要把 5 V 逻辑接到 ESP 引脚。

建议测量的区间：

| 区间 | 含义 |
| ---- | ---- |
| GPIO5 上升沿 − GPIO4 上升沿 | 视频初始化耗时 |
| GPIO6 上升沿 − GPIO5 上升沿 | 写回 IPA 状态、开流，直到取得本轮帧 |
| 下一轮 GPIO4 上升沿 − 本轮 GPIO6 上升沿 | 采集完成到下一次进入应用，含睡眠、定时器等待和再次启动 |

若已增加二级 bootloader 标记，可将「定时器到期到 GPIO4」再分为 ROM、bootloader 和应用启动。

### 操作顺序

1. 烧录并上电。串口出现 `Enter deep sleep, timer wakeup in ... ms` 后进入睡眠。
2. 启动分析仪，按重复触发采集。
3. 等待定时器唤醒，确认 GPIO4、GPIO5、GPIO6 依次出现上升沿。
4. 用游标测量上表中的区间，以及相邻两轮 GPIO4 上升沿的间隔。

### 异常对照

| 现象 | 可能原因 | 处理 |
| ---- | -------- | ---- |
| 只有 GPIO4，没有 GPIO5 | `example_video_init()` 失败 | 检查摄像头供电、排线、SCCB 地址与引脚、menuconfig 中的传感器和板型 |
| 冷启动只有 GPIO4 和 GPIO5，没有 GPIO6 | 冷启动只保存状态 | 等定时器唤醒后再观察 GPIO6 |
| 定时器唤醒后没有 GPIO6 | `set_ipa_state()` 被拒绝，或开流、`DQBUF` 失败 | 查看 `saved IPA state rejected`；检查 MIPI 链路、传感器格式与分辨率 |
| 睡眠时间明显短于配置值 | `Deep sleep time (ms)` 小于进入 Deep Sleep 的开销 | 将睡眠时间增大到数百毫秒以上再测 |
| GPIO4、GPIO5、GPIO6 时序混乱 | 探头接地不良、脚位接错，或 GPIO 被其他外设复用 | 确认未启用占用这三只引脚的 DVP 或 SPI 配置 |
| 串口没有 `Captured one usable frame` 或 `Enter deep sleep` | 默认日志级别较低 | 以 GPIO6 是否置 1 为准，或把日志级别提高到 Info |

## 源码说明

主文件：`main/software_aov_deep_sleep_main.c`  
工程名：`software_aov_deep_sleep`

| 宏 / 项 | 默认值 | 说明 |
| ------- | ------ | ---- |
| `CONFIG_EXAMPLE_SLEEP_TIME_MS` | 1000 | Deep Sleep 定时器时间，单位毫秒 |
| `BUFFER_COUNT` | 3 | V4L2 buffer 个数 |
| `CAM_DEV_PATH` | `EXAMPLE_CAM_DEV_PATH`（MIPI-CSI 时为 `/dev/video0`） | 摄像头设备路径 |
| `GPIO_MARKER_*` | 4 / 5 / 6 | 标记脚。更换时避开摄像头、Flash 和 USB |

* 采集函数不调用 `VIDIOC_S_FMT`。默认帧大小见上文日志。
* 本示例以 `set_ipa_state()` 成功且 `DQBUF` 返回、`V4L2_BUF_FLAG_DONE` 置位作为本轮有效帧。冷启动的帧，以及写回失败后重新收敛的帧，都不是这帧。
* 测量时尽量固定曝光条件和环境光。需要写入存储介质或发送到网络时，在 GPIO6 置 1 之后扩展；该引脚只表示软件已经取到一帧。

## 检查清单

* 目标芯片：ESP32-P4
* 默认摄像头：SC2336 MIPI-CSI，`/dev/video0`
* 唤醒源：定时器，时间见 `Example Configuration` → `Deep sleep time (ms)`
* GPIO4 为高：应用已开始
* GPIO5 为高：视频初始化完成
* GPIO6 为高：写回成功后的第一帧
* 冷启动时 GPIO6 保持低
* 定时器唤醒后依次为 GPIO4、GPIO5、GPIO6，然后按配置的毫秒数重复
