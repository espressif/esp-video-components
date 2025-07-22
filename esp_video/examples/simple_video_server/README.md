| Supported Targets | ESP32-P4 | ESP32-S3 | ESP32-C3 | ESP32-C6 | ESP32-C5 |
| ----------------- | -------- | -------- | -------- | -------- | -------- |

# Simple Video Server Example

(See the [README.md](../README.md) file in the upper level [examples](../) directory for more information about examples.)

## Overview

This example starts several HTTP servers on a local network using different ports. You can access these servers through a web browser.

The example provides several APIs to fetch resources as follows:

| TCP Port | URL | Method | Description |
| :------: | :-----: | :----: | ------------------------------------------------------------ |
| 80 | /stream | GET | Serves HTML web script for browser-based video display |
| 80 | /capture_image?source={n} | GET | Returns JPEG format images from the selected camera sensor. Parameter `n` indicates the camera sensor number: 0 for the first camera sensor, 1 for the second camera sensor. Example: `/capture_image?source=0` |
| 80 | /capture_binary?source={n} | GET | Returns binary data describing the original image from the selected camera sensor. Parameter `n` indicates the camera sensor number: 0 for the first camera sensor, 1 for the second camera sensor. Example: `/capture_binary?source=0` |
| 81 | /stream | GET | Provides continuous MJPEG stream from the **first** camera sensor (*1) |
| 82 | /stream | GET | Provides continuous MJPEG stream from the **second** camera sensor (*1) |

* (*1): The server continuously pushes JPEG images from the background to the client. When you save images from the webpage, the saved images may not be in real-time.

By default, the example starts an mDNS domain name system, allowing server access via domain name. For example, you can access the image capture URL by entering `http://esp-web.local/capture_image/source=0` in your browser. You can also access URLs using IP addresses.

## How to Use This Example

### Configure the Project

Open the project configuration menu (`idf.py menuconfig`).

#### Configure the Hardware

Please refer to the example video initialization configuration [document](../common_components/example_video_common/README.md) for detailed information about board-level configuration, including camera sensor interface, GPIO pins, clock frequency, and other settings.

#### Connection Configuration

In the `Example Connection Configuration` menu:

* **Wi-Fi Interface**: If you select the Wi-Fi interface, you must also configure:
  * Wi-Fi SSID and password for your ESP32 to connect to
  * Wi-Fi SoftAP SSID and password if you want the ESP32 to work as an Access Point

* **Ethernet Interface**: If you select the Ethernet interface, you must also configure:
  * PHY model in the `Ethernet PHY` option (e.g., IP101)
  * PHY address in the `PHY Address` option (determined by your board schematic)
  * EMAC Clock mode and GPIO used by SMI

For devices that do not support native WiFi, [esp_wifi_remote](https://github.com/espressif/esp-protocols/tree/master/components/esp_wifi_remote) is used by default to provide an additional WiFi interface. In the `Wi-Fi Remote` menu:

* Choose the slave target to connect to the MCU

#### Configure Camera Sensor

In the `Espressif Camera Sensors Configurations` menu:

* Select the camera sensor you want to connect
* Select the target format for this sensor

#### Configure Example

Run the following commands to select the ESP32-P4 platform and enter menuconfig:

```sh
idf.py set-target esp32p4
idf.py menuconfig
```

Configure the camera sensor video buffer number:

```
Example Configuration  --->
    (2) Camera video buffer number
```

More buffers provide better performance and reduce frame drops but consume more memory. For sensors with large resolutions such as 1080P, it's recommended to use 2 buffers.

Configure the output JPEG format image compression quality:

```
Example Configuration  --->
    (80) JPEG compression quality (%)
```

**Note**: This value may not be supported by all camera sensors. If your selected camera sensor doesn't support this value, the example will calculate the nearest supported value to configure the camera sensor.

Additional HTTP and mDNS configurations:

```
Example Configuration  --->
    (123456789000000000000987654321) HTTP part boundary
    (web-cam) mDNS instance
    (esp-web) mDNS host name
```

If you have no special requirements, please keep these HTTP and mDNS default configurations.

Select and configure the camera sensor interface; this example will try to initialize all enabled camera sensors and push their image streams to the client:

```
Example Video Initialization Configuration  --->
    Select and Set Camera Sensor Interface  --->
        [*] MIPI-CSI  ---
        [*] DVP  ---->
```

If these camera sensor uses the same I2C GPIO pins, such as MIPI-CSI and DVP camera sensors in the development board of `ESP32-P4-Function-EV-Board V1.5`, please select the following option:

```
Example Video Initialization Configuration  --->
    ......
    [*] Use Pre-initialized SCCB(I2C) Bus for All Camera Sensors And Motors
        (0) SCCB(I2C) Port Number
        (8) SCCB(I2C) SCL Pin
        (7) SCCB(I2C) SDA Pin
    ......
```

Select the target sensors based on your development board:

```
Component config  --->
    Espressif Camera Sensors Configurations  --->
        Camera Sensor Configuration  --->
            Select and Set Camera Sensor  --->
                ......
                [ ] GC0308  ----
                [*] GC2145  --->
                [*] OV2640  ---->
                ......
```

For better FPS of the DVP interface camera sensor, you can select the following option:

```
Component config  --->
    Espressif Camera Sensors Configurations  --->
        Camera Sensor Configuration  --->
            Select and Set Camera Sensor  --->
                ......
                [*] OV2640  ---->
                    Select default output format for DVP interface (JPEG 640x480 25fps, DVP 8-bit, 20M input)  --->
                        ......
                        ( ) YUV422 640x480 6fps, DVP 8-bit, 20M input
                        (X) JPEG 640x480 25fps, DVP 8-bit, 20M input
                        ( ) RGB565 240x240 25fps, DVP 8-bit, 20M input
                        ......
                ......
```

Build the project and flash it to the board, then run the monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type `Ctrl-]`.)

See the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/index.html) for complete steps to configure and use ESP-IDF to build projects.

When running this example, you will see the following log output on the serial monitor:

```
...
I (1628) main_task: Started on CPU0
I (1638) esp_psram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1638) main_task: Calling app_main()
I (1648) mdns_mem: mDNS task will be created from internal RAM
I (1698) esp_eth.netif.netif_glue: 60:55:f9:fb:c2:3a
I (1698) esp_eth.netif.netif_glue: ethernet attached to netif
I (3298) ethernet_connect: Waiting for IP(s).
I (3298) ethernet_connect: Ethernet Link Up
I (4648) ethernet_connect: Got IPv6 event: Interface "example_netif_eth" address: fe80:0000:0000:0000:6255:f9ff:fefb:c23a, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (5298) esp_netif_handlers: example_netif_eth ip: 172.168.30.45, mask: 255.255.255.0, gw: 172.168.30.1
I (5298) ethernet_connect: Got IPv4 event: Interface "example_netif_eth" address: 172.168.30.45
I (5298) example_common: Connected to example_netif_eth
I (5308) example_common: - IPv4 address: 172.168.30.45,
I (5308) example_common: - IPv6 address: fe80:0000:0000:0000:6255:f9ff:fefb:c23a, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (5318) example_init_video: MIPI-CSI camera sensor I2C port=0, scl_pin=8, sda_pin=7, freq=100000
I (5328) example_init_video: DVP camera sensor I2C port=1, scl_pin=8, sda_pin=7, freq=100000
I (5378) ov2640: Detected Camera sensor PID=0x26
I (5378) gc2145: Detected Camera sensor PID=0x2145
I (5808) example: video0: width=640 height=480 format=RGBP
W (5908) example: JPEG compression quality=80 is out of sensor's range, reset to 63
I (5908) example: video1: width=640 height=480 format=JPEG
I (5908) example: Starting stream server on port: '80'
I (5918) example: Camera web server starts
I (5918) main_task: Returned from app_main()
...
```

Enter `http://esp-web.local/stream` or `172.168.30.45/stream` (you can find the IP address in the log information above) in your browser to access the video stream. You will see the web interface as follows:

![camera_web_pic](./pic/camera_web_pic.jpg)

You can capture and download JPEG format images using the `Capture Image0` button (left video stream) or `Capture Image1` button (right video stream), and capture and download raw binary data describing the original image using the `Capture Binary0` button (left video stream) or `Capture Binary1` button (right video stream).

## Troubleshooting

1. **I2C Transaction Error:**

   ```
   E (1595) i2c.master: I2C transaction unexpected nack detected
   E (1595) i2c.master: s_i2c_synchronous_transaction(870): I2C transaction failed
   ```

   **Solution**: Check that the camera sensor is properly connected to the board and that the pins are correctly configured in menuconfig.
