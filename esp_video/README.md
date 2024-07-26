# Espressif Video Component

Espressif video component provides a solution to call POSIX API plus Linux V4L2 commands to capture data streams from multi camera sensors, and transform stream data pixel format according to Linux V4L2 M2M codec device.

[![Component Registry](https://components.espressif.com/components/espressif/esp_video/badge.svg)](https://components.espressif.com/components/espressif/esp_video)

Now we have implementations based on:

- esp_cam_sensor

## Video Device

| Hardware | Video Device | Type | Input Format | Output Format |
|:-:|:-:|:-:|:-|:-|
| MIPI-CSI | /dev/video0 | Capture | / | camera output pixel format or ISP output format(1) |
| DVP | /dev/video2 | Capture  | / | camera output pixel format |
| JPEG encode | /dev/video10 | M2M | RGB565: V4L2_PIX_FMT_RGB565<br> RGB888: V4L2_PIX_FMT_RGB24<br> YUV422: V4L2_PIX_FMT_YUV422P<br> Gray8: V4L2_PIX_FMT_GREY | JPEG: V4L2_PIX_FMT_JPEG |
| H.264 encode | /dev/video11 | M2M | YUV420: V4L2_PIX_FMT_YUV420 | H.264: V4L2_PIX_FMT_H264 |

- (1): if camera output pixel format is RAW8, ISP can transform it to other pixel format: RGB565, RGB888, YUV420 and YUV422
