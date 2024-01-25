| Supported Targets | ESP32-P4 |
| ----------------- | -------- |

## CSI Camera USB

This example demonstrates how to capture image data from a MIPI-CSI camera, process the image through the SoC's built-in ISP, use the SoC's built-in JPEG hardware encoder to compress the image, and finally display the image on a computer through the USB 2.0 High-Speed interface.

## How to use example

### Hardware Required

* An ESP32-P4_Function_EV_Board.
* A 500w pixel OV5647 camera with 15 pins FPC ([Camera Specifications](https://www.waveshare.net/wiki/RPi_Camera_(B))).
* Two USB-C cables, one for power supply and programming, and the other for USB communication.
* Please refer to the following steps for the connection:
    * **Step 1**. Connect the FPC of Camera with the `MIPI_CSI` interface of board.
    * **Step 2**. Use a USB-C cable to connect the `USB 2.0` port to a PC (`Windows` or `Mac` OS).
    * **Step 3**. Use a USB-C cable to connect the `USB-UART` port to a PC (Used for power supply and viewing serial output).
    * **Step 4**. Turn on the power switch of the board.

<div align=center>
    <img src="../../../docs/_static/p4_sample/ESP32-P4%20function%20test%20board%20v1.0B_front.png" width="500"/>
</div>

### Configure the project

Run `idf.py menuconfig` and go to `USB Device UVC`.

### Build and Flash

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Run Example

Boards programmed with this example can be directly tested on `Windows` and `Mac` operating systems following the steps below. However, `Linux` system support is currently unavailable.

* **For Windows OS**: Open the system `Camera` application to view the images.
* **For Mac OS**: Open the `FaceTime` application, and select `ESP UVC Device` (as shown in the figure) to view the images.

<div align=center>
    <img src="../../../docs/_static/p4_sample/Mac_camera.png" width="300"/>
</div>

## Known Issues

* The current ESP-IDF version is still unstable, and encountering crashes during demo usage is considered a normal phenomenon. We recommend keeping a close eye on our software updates.
* The current adaptation of ESP32-P4 to the camera is not yet perfect, resulting in color deviation in the camera output images, which is considered a normal phenomenon. We recommend staying informed about updates to the ESP-IDF version to access more support for the Image Signal Processing (ISP) module.

## Troubleshooting

For any technical queries, please open an [issue](https://glab.espressif.cn/esp-idf-preview/esp-idf-p4/issues) on GLab. We will get back to you soon.
