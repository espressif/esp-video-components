# Software AOV Examples

These examples are a software AOV flow, not a hardware-native AOV mode in the sensor or SoC. The application captures one frame, stops the stream, and sleeps. The sleep time is set in menuconfig in milliseconds. The same schedule can run on a MIPI-CSI camera, a DVP camera, or another interface supported by `esp_video`. The projects here use MIPI-CSI on ESP32-P4; switching the interface means replacing the camera init path and measuring power again.

Successive frames have a time gap. The gap covers stream stop, sleep, and the next stream start. Deep sleep also includes chip boot, so events during the gap are not captured.

The examples only show this low-rate capture and sleep. They do not include recognition or analysis.

* [deep_sleep](deep_sleep) resets on each timer wakeup.
* [light_sleep](light_sleep) resumes after `esp_light_sleep_start()` returns.

Both examples require ESP-IDF v5.5.2 or later.

The difference between the two modes, including power figures, is in the [Software AOV solution](../../../docs/zh_CN/Solutions/software_aov.rst).
