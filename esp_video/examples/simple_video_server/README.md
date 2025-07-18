| Supported Targets | ESP32-P4 | ESP32-S3 | ESP32-C3 | ESP32-C6 | ESP32-C5 |
| ----------------- | -------- | -------- | -------- | -------- | -------- |

# Simple Video Server Example

(See the [README.md](../README.md) file in the upper level [examples](../) directory for more information about examples.)

## Overview

The example starts two HTTP servers on a local network by using different ports. You can use a browser to access the servers.
This example designs several APIs to fetch resources as follows:

| URL     | Method | Description                                                  |
| ------- | ------ | ------------------------------------------------------------ |
| /pic    | GET    | Used for clients to get a jpeg image, Refreshing the webpage can retrieve a new image, which can be saved by right clicking on the save button on the webpage. |
| /record | GET    | Used for clients to get binary data describing the original image. |
| /stream | GET    | Used for clients to get continuous MJPEG stream. The server continuously pushes JPEG images from the background to the client. So when you save images on the webpage, the saved images may not be in real-time. |

By default, the example will start an MDNS domain name system. Therefore, the server can be accessed by domain name. For example, accessing the URL for obtaining images by entering URL `http://esp-web.local/pic` in the browser. Also, accessing URLs through the use of IP addresses is also allowed.

## How to use example

### Configure the project

Open the project configuration menu (`idf.py menuconfig`).

#### Configure the hardware

Please refer to the example video initialization configuration [document](../common_components/example_video_common/README.md) for more details about the board-level configuration, including the camera sensor interface, GPIO pins, clock frequency, and so on.

#### Connection Configuration:
In the `Example Connection Configuration` menu:

* If you select the Wi-Fi interface, you also have to set:
  * Wi-Fi SSID and Wi-Fi password that your esp32 will connect to.
  * Wi-Fi SoftAP's SSID and password if you want esp32 work as an Access Point.

* If you select the Ethernet interface, you also have to set:
  * PHY model in `Ethernet PHY` option, e.g. IP101.
  * PHY address in `PHY Address` option, which should be determined by your board schematic.
  * EMAC Clock mode, GPIO used by SMI.

#### Configuration of the camera sensor
In the `Espressif Camera Sensors Configurations` menu:

* Select the camera sensor you want to connect to.
* Select the default format for this sensor.

The default format of the camera sensor determines the data format that can be used in the program. Therefore, when the camera sensor is selected to work in `YUV422` format in the configuration menu, the format that should be configured in the `app_main.c` is `V4L2_PIX_FMT_YUV422P`:

```c
app_video_open(EXAMPLE_CAM_DEV_PATH, V4L2_PIX_FMT_YUV422P);
```

If the default format selected in the configuration menu is `RAW8`, the ISP can automatically generate interpolated data formats(e.g., RGB888, RGB565, YUV422, YUV420, etc). You can configure the output format to RAW8 or YUV422, etc.

For devices that do not support native WiFi, [esp_wifi_remote](https://github.com/espressif/esp-protocols/tree/master/components/esp_wifi_remote) is used to provide an additional wifi interface by default. In the `Wi-Fi Remote` menu:

* Choose the slave target connect to the MCU.

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Example Output

Running this example, you will see the following log output on the serial monitor:

```
...
I (1606) main_task: Started on CPU0
I (1616) esp_psram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1616) main_task: Calling app_main()
I (1676) esp_eth.netif.netif_glue: 60:55:f9:f8:80:8a
I (1676) esp_eth.netif.netif_glue: ethernet attached to netif
I (3276) app_eth: Ethernet Started
I (3276) gpio: GPIO[22]| InputEn: 1| OutputEn: 1| OpenDrain: 1| Pullup: 1| Pulldown: 0| Intr:0 
I (3276) app_eth: Ethernet Link Up
I (3276) app_eth: Ethernet HW Addr 60:55:f9:f8:80:8a
I (3286) gpio: GPIO[23]| InputEn: 1| OutputEn: 1| OpenDrain: 1| Pullup: 1| Pulldown: 0| Intr:0 
I (3296) sc2336: Detected Camera sensor PID=0xcb3a with index 0
I (3366) app_video: version: 0.1.0
I (3366) app_video: driver:  MIPI-CSI
I (3366) app_video: card:    MIPI-CSI
I (3376) app_video: bus:     esp32p4:MIPI-CSI
I (3376) app_video: width=1280 height=720
I (3386) app_video: Capture RGB 5-6-5 format
I (3396) app_web: Starting stream HTTP server on port: '80'
I (3396) main_task: Returned from app_main()
I (4276) esp_netif_handlers: eth ip: 192.168.47.100, mask: 255.255.255.0, gw: 192.168.47.1
I (4276) app_eth: Ethernet Got IP Address
I (4276) app_eth: ~~~~~~~~~~~
I (4276) app_eth: ETHIP:192.168.47.100
I (4286) app_eth: ETHMASK:255.255.255.0
I (4286) app_eth: ETHGW:192.168.47.1
I (4286) app_eth: ~~~~~~~~~~~
I (7216) app_web: jpeg size = 50749
I (7966) app_web: jpeg size = 50749
I (8996) app_web: jpeg size = 50560
...
```

Enter `http://esp-web.local/pic` or `192.168.47.100/pic` in the browser to access the image.
Enter `http://esp-web.local/record` or `192.168.47.100/record` in the browser to download the original binary file.
Enter `http://esp-web.local:81/stream` or `192.168.47.100:81/stream` in the browser to access the video stream.

## Troubleshooting

1. Error occurred:

   ```
   E (1595) i2c.master: I2C transaction unexpected nack detected
   E (1595) i2c.master: s_i2c_synchronous_transaction(870): I2C transaction failed
   ```

   - Check that the camera sensor is connected to the board and that the pins are correctly configured in the menuconfig.

2. Error occurred:

   ```
   E (1857) dvp_video: format is not supported
   E (1857) esp_video: video->ops->set_format=102
   ```

- Check if the parameters of command `VIDIOC_S_FMT` in the code match the format selected in the menuconfig.
