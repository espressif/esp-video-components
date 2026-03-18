# Example Video Common Component

This component provides board-level initialization for esp_video, including MIPI-CSI sensor I2C port, DVP sensor I2C port, DVP interface configuration and SPI interface configuration. It is primarily designed for esp_video examples to simplify board-level configuration and initialization processes.

## Supported Boards and GPIO Pins

| Hardware | ESP32-P4-Function-EV-Board V1.4 | ESP32-P4-Function-EV-Board V1.5 | ESP32-P4-EYE | ESP32-S3-EYE |
|:-:|:-:|:-:|:-:|:-:|
| MIPI-CSI I2C SCL Pin        |  8 |  8 | 13 | NA |
| MIPI-CSI I2C SDA Pin        |  7 |  7 | 14 | NA |
| MIPI-CSI I2C Reset Pin      | NA | NA | 26 | NA |
| MIPI-CSI I2C Power-down Pin | NA | NA | 12 | NA |
| MIPI-CSI I2C XCLK           | NA | NA | 11 | NA |
|   |   |   |   |
| DVP I2C SCL Pin             | NA |  8 | NA |  5 |
| DVP I2C SDA Pin             | NA |  7 | NA |  4 |
| DVP I2C Reset Pin           | NA | 36 | NA | NA |
| DVP I2C Power-down Pin      | NA | 38 | NA | NA |
| DVP XCLK Pin                | NA | 20 | NA | 15 |
| DVP PCLK Pin                | NA |  4 | NA | 13 |
| DVP V-SYNC Pin              | NA | 37 | NA |  6 |
| DVP DE Pin                  | NA | 22 | NA |  7 |
| DVP D0 Pin                  | NA |  2 | NA | 11 |
| DVP D1 Pin                  | NA | 32 | NA |  9 |
| DVP D2 Pin                  | NA | 33 | NA |  8 |
| DVP D3 Pin                  | NA | 23 | NA | 10 |
| DVP D4 Pin                  | NA |  3 | NA | 12 |
| DVP D5 Pin                  | NA |  6 | NA | 18 |
| DVP D6 Pin                  | NA |  5 | NA | 17 |
| DVP D7 Pin                  | NA | 21 | NA | 16 |
|   |   |   |   |
| SPI I2C SCL Pin             | NA |  8 | NA |  5 |
| SPI I2C SDA Pin             | NA |  7 | NA |  4 |
| SPI I2C Reset Pin           | NA | NA | NA | NA |
| SPI I2C Power-down Pin      | NA | NA | NA | NA |
| SPI XCLK Pin                | NA | 20 | NA | 15 |
| SPI CS Pin                  | NA | 37 | NA |  6 |
| SPI SCLK Pin                | NA |  4 | NA | 13 |
| SPI Data0 I/O Pin           | NA | 21 | NA | 16 |
| SPI Data1 I/O Pin           | NA |  5 | NA | NA |
|   |   |   |   |
| SDMMC Data Bus Width        |  4 |  4 |  4 | 1  |
| SDMMC CMD Pin               | 44 | 44 | 44 | 38 |
| SDMMC CLK Pin               | 43 | 43 | 43 | 39 |
| SDMMC D0 Pin                | 39 | 39 | 39 | 40 |
| SDMMC D1 Pin                | 40 | 40 | 40 | NA |
| SDMMC D2 Pin                | 41 | 41 | 41 | NA |
| SDMMC D3 Pin                | 42 | 42 | 42 | NA |

**Note 1:** The ESP32-P4-Function-EV v1.4 board and ESP32-P4-EYE do not support DVP interface camera sensors by default. If you need to connect a DVP interface camera sensor to these boards, please select "Customized Development Board" in the menu and configure the GPIO pins and clock according to your specific hardware setup.

**Note 2:** Only the parlio mode supports more than 1-bit to transform data, so only the parlio mode supports "SPI Data1 I/O Pin".

**Note 3:** The GPIO pin order for the SP0A39 camera sensor module is different from the BF3901. Additionally, the SP0A39 can only be driven by Parallel IO and not by an SPI slave interface, so only ESP32-P4 and ESP32-C5 support this sensor. The GPIO pin assignments for the `ESP32-P4-Function-EV-Board V1.5` are as follows:

| Camera Sensor | BF3901 | SP0A39 |
|---------------|--------|--------|
| Data0 I/O Pin |   21   |    2   |
| Data1 I/O Pin |    5   |   32   |
| Data2 I/O Pin |   NA   |   33   |
| Data3 I/O Pin |   NA   |   23   |

### Customized Development Board Default Configuration

When using "ESP32-XX-DevKitC" development boards, you can try the default GPIO pin configuration for "Customized Development Board" as shown in the following table:

| Hardware | ESP32-P4 | ESP32-S3 | ESP32-C3 | ESP32-C6 | ESP32-C61 | ESP32-C5 |
|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
| MIPI-CSI CAM I2C SCL Pin        |  8 | NA | NA | NA | NA | NA |
| MIPI-CSI CAM I2C SDA Pin        |  7 | NA | NA | NA | NA | NA |
| MIPI-CSI CAM I2C Reset Pin      | NA | NA | NA | NA | NA | NA |
| MIPI-CSI CAM I2C Power-down Pin | NA | NA | NA | NA | NA | NA |
| MIPI-CSI CAM I2C XCLK           | NA | NA | NA | NA | NA | NA |
|   |   |   |   |
| DVP CAM I2C SCL Pin             |  8 |  5 | NA | NA | NA | NA |
| DVP CAM I2C SDA Pin             |  7 |  4 | NA | NA | NA | NA |
| DVP CAM I2C Reset Pin           | NA | NA | NA | NA | NA | NA |
| DVP CAM I2C Power-down Pin      | NA | NA | NA | NA | NA | NA |
| DVP CAM XCLK Pin                | 20 | 15 | NA | NA | NA | NA |
| DVP CAM PCLK Pin                |  4 | 13 | NA | NA | NA | NA |
| DVP CAM V-SYNC Pin              | 37 |  6 | NA | NA | NA | NA |
| DVP CAM DE Pin                  | 22 |  7 | NA | NA | NA | NA |
| DVP CAM D0 Pin                  |  2 | 11 | NA | NA | NA | NA |
| DVP CAM D1 Pin                  | 32 |  9 | NA | NA | NA | NA |
| DVP CAM D2 Pin                  | 33 |  8 | NA | NA | NA | NA |
| DVP CAM D3 Pin                  | 23 | 10 | NA | NA | NA | NA |
| DVP CAM D4 Pin                  |  3 | 12 | NA | NA | NA | NA |
| DVP CAM D5 Pin                  |  6 | 18 | NA | NA | NA | NA |
| DVP CAM D6 Pin                  |  5 | 17 | NA | NA | NA | NA |
| DVP CAM D7 Pin                  | 21 | 16 | NA | NA | NA | NA |
|   |   |   |   |
| SPI CAM0 I2C SCL Pin             |  8 |  5 |  5 |  5 |  5 |  5 |
| SPI CAM0 I2C SDA Pin             |  7 |  4 |  4 |  4 |  4 |  4 |
| SPI CAM0 I2C Reset Pin           | NA | NA | NA | NA | NA | NA |
| SPI CAM0 I2C Power-down Pin      | NA | NA | NA | NA | NA | NA |
| SPI CAM0 XCLK Pin                | 20 | 15 |  8 |  0 |  0 |  0 |
| SPI CAM0 CS Pin                  | 37 |  6 | 10 |  1 |  8 | 10 |
| SPI CAM0 SCLK Pin                |  4 | 13 |  6 |  6 |  6 |  6 |
| SPI CAM0 Data0 I/O Pin           | 21 | 16 |  7 |  7 |  7 |  7 |
| SPI CAM0 Data1 I/O Pin           |  5 | NA | NA | NA |  9 |  9 |
| SPI CAM0 Data2 I/O Pin           | 33 | NA | NA | NA | NA |  8 |
| SPI CAM0 Data3 I/O Pin           | 23 | NA | NA | NA | NA | 26 |
|   |   |   |   |
| SPI CAM1 I2C SCL Pin             |  5 |  1 | NA | NA | NA | NA |
| SPI CAM1 I2C SDA Pin             |  6 |  2 | NA | NA | NA | NA |
| SPI CAM1 I2C Reset Pin           | NA | NA | NA | NA | NA | NA |
| SPI CAM1 I2C Power-down Pin      | NA | NA | NA | NA | NA | NA |
| SPI CAM1 XCLK Pin                | 23 | 39 | NA | NA | NA | NA |
| SPI CAM1 CS Pin                  | 38 | 42 | NA | NA | NA | NA |
| SPI CAM1 SCLK Pin                | 22 | 41 | NA | NA | NA | NA |
| SPI CAM1 Data0 I/O Pin           |  3 | 40 | NA | NA | NA | NA |

**Note 1**: Only SoCs that have more than one SPI port, except for SPI flash and SPI RAM ports, can use the SPI CAM0 and SPI CAM1 simultaneously.

**Note 2**: If the SPI CAM0 and SPI CAM1 use the same camera sensor and it only owns one I2C slave address, the SPI CAM0 and SPI CAM1 should use different I2C ports to communicate with their target camera sensor.

**Note 3:** Only the Parallel IO mode supports more than 1-bit to transform data (up to 4-bit mode with Data0-Data3), so only the Parallel IO mode supports "SPI CAM0 Data1/Data2/Data3 I/O Pin".

## Usage Instructions

### MIPI-CSI Development Kit

To configure your development board with MIPI-CSI interface, follow these steps:

```
Example Video Initialization Configuration  --->
    Select Target Development Board (ESP32-P4-Function-EV-Board V1.5)
        ( ) ESP32-P4-Function-EV-Board V1.4
        (X) ESP32-P4-Function-EV-Board V1.5
        ( ) ESP32-P4-EYE

        ......

        ( ) Customized Development Board
    Select and Set Camera Sensor Interface  --->
        [*] MIPI-CSI  --->
        [ ] DVP  ----
```

If your development board is not listed in the menu, select "Customized Development Board" and configure the GPIO pins according to your board specifications:

```
Example Video Initialization Configuration  --->
    Select Target Development Board (Customized Development Board)
        ( ) ESP32-P4-Function-EV-Board V1.4
        ( ) ESP32-P4-Function-EV-Board V1.5
        ( ) ESP32-P4-EYE

        ......

        (X) Customized Development Board
    Select and Set Camera Sensor Interface  --->
        [*] MIPI-CSI  --->
            (0) SCCB(I2C) Port Number
            (100000) SCCB(I2C) Frequency (100K-400K Hz)
            (8) SCCB(I2C) SCL Pin
            (7) SCCB(I2C) SDA Pin
            (-1) Reset Pin
            (-1) Power Down Pin
            (-1) XCLK Pin
        [ ] DVP  ----
```

### DVP Development Kit

To configure your development board with DVP interface, follow these steps:

```
Example Video Initialization Configuration  --->
    Select Target Development Board (ESP32-P4-Function-EV-Board V1.5)
        ( ) ESP32-P4-Function-EV-Board V1.4
        (X) ESP32-P4-Function-EV-Board V1.5
        ( ) ESP32-P4-EYE

        ......

        ( ) Customized Development Board
    Select and Set Camera Sensor Interface  --->
        [ ] MIPI-CSI  ---
        [*] DVP  ---->
```

If your development board is not listed in the menu, select "Customized Development Board" and configure the GPIO pins according to your board specifications:

```
Example Video Initialization Configuration  --->
    Select Target Development Board (Customized Development Board)
        ( ) ESP32-P4-Function-EV-Board V1.4
        ( ) ESP32-P4-Function-EV-Board V1.5
        ( ) ESP32-P4-EYE

        ......

        (X) Customized Development Board
    Select and Set Camera Sensor Interface  --->
        [ ] MIPI-CSI  ---
        [*] DVP  ---->
            (0) SCCB(I2C) Port Number
            (100000) SCCB(I2C) Frequency (100K-400K Hz)
            (20000000) XCLK Frequency (Hz)
            (8) SCCB(I2C) SCL Pin
            (7) SCCB(I2C) SDA Pin
            (-1) Reset Pin
            (-1) Power Down Pin
            (20) XCLK Pin
            (4) PCLK Pin
            (37) VSYNC Pin
            (22) DE Pin
            (2) Data0 Pin
            (32) Data1 Pin
            (33) Data2 Pin
            (23) Data3 Pin
            (3) Data4 Pin
            (6) Data5 Pin
            (5) Data6 Pin
            (21) Data7 Pin
```

### SPI Development Kit

To enable SPI video device support, first enable the following configuration:

```
Component config  --->
    Espressif Video Configuration  --->
        [*] Enable SPI based Video Device  ----
```

Then configure your development board with SPI interface:

```
Example Video Initialization Configuration  --->
    Select Target Development Board (ESP32-P4-Function-EV-Board V1.5)
        ( ) ESP32-P4-Function-EV-Board V1.4
        (X) ESP32-P4-Function-EV-Board V1.5
        ( ) ESP32-P4-EYE

        ......

        ( ) Customized Development Board
    Select and Set Camera Sensor Interface  --->
        [ ] MIPI-CSI  ---
        [ ] DVP  ----
        [*] SPI  ---->
```

If your development board is not listed in the menu, select "Customized Development Board" and configure the GPIO pins according to your board specifications:

```
Example Video Initialization Configuration  --->
    Select Target Development Board (Customized Development Board)
        ( ) ESP32-P4-Function-EV-Board V1.4
        ( ) ESP32-P4-Function-EV-Board V1.5
        ( ) ESP32-P4-EYE

        ......

        (X) Customized Development Board
    Select and Set Camera Sensor Interface  --->
        [ ] MIPI-CSI  ---
        [ ] DVP  ----
        [*] SPI  ---->
            ()  SCCB(I2C) Port Number
            (100000) SCCB(I2C) Frequency (100K-400K Hz)
            (2) SPI Port Number
                Select XCLK Source (ESP Clock Router)  --->
            (24000000) XCLK Frequency (Hz)
            (8) SCCB(I2C) SCL Pin
            (7) SCCB(I2C) SDA Pin
            (-1) Reset Pin
            (-1) Power Down Pin
            (20) XCLK Output Pin
            (37) Chip Select Pin
            (4) Clock Pin
            (21) Data0 I/O Pin
```

### Dual SPI Camera Sensors

***ESP32-P4*** and ***ESP32-S3*** have more than two SPI ports (where one port is used for SPI flash and SPIRAM), allowing these two SoCs to connect to dual SPI camera sensors. To enable dual SPI camera sensors, first enable the following configuration:

```
Component config  --->
    Espressif Video Configuration  --->
        [*] Enable SPI based Video Device  --->
            [*] Enable The Second SPI Video Device
```

Then select the "Customized Development Board" configuration, as there is currently no dedicated dual SPI camera sensor development board available:

```
Example Video Initialization Configuration  --->
    Select Target Development Board (Customized Development Board)  --->
        ......
        (X) Customized Development Board
```

Configure the GPIO pins according to your board specifications:

```
Example Video Initialization Configuration  --->
    Select and Set Camera Sensor Interface  --->
        [*] SPI Camera Sensor 0  --->
            (0) SCCB(I2C) Port Number
            (100000) SCCB(I2C) Frequency (100K-400K Hz)
            (2) SPI Port Number
                Select XCLK Source (ESP Clock Router)  --->
            (24000000) XCLK Frequency (Hz)
            (8) SCCB(I2C) SCL Pin (NEW)
            (7) SCCB(I2C) SDA Pin (NEW)
            (-1) Reset Pin (NEW)
            (-1) Power Down Pin (NEW)
            (20) XCLK Output Pin (NEW)
            (37) Chip Select Pin (NEW)
            (4) Clock Pin (NEW)
            (21) Data0 I/O Pin (NEW)
       [*] SPI Camera Sensor 1  --->
            (1) SCCB(I2C) Port Number
            (100000) SCCB(I2C) Frequency (100K-400K Hz)
            (1) SPI Port Number
                Select XCLK Source (LEDC Timer)  --->
            (20000000) XCLK Frequency (Hz)
            (1) XCLK LEDC Timer Number
            (1) XCLK LEDC Timer Channel
            (5) SCCB(I2C) SCL Pin (NEW)
            (6) SCCB(I2C) SDA Pin (NEW)
            (-1) Reset Pin (NEW)
            (-1) Power Down Pin (NEW)
            (23) XCLK Output Pin (NEW)
            (38) Chip Select Pin (NEW)
            (22) Clock Pin (NEW)
            (3) Data0 I/O Pin (NEW)
```

### Use Pre-initialized SCCB(I2C) Bus

For some development boards, the MIPI-CSI, DVP, SPI camera sensors or motors share the same SCCB(I2C) port and GPIO pins, such as ESP32-P4-Function-EV-Board V1.5. You can select and set the following options:

```
Example Video Initialization Configuration  --->
    [*] Use Pre-initialized SCCB(I2C) Bus for All Camera Sensors And Motors  --->
        (0) SCCB(I2C) Port Number
        (8) SCCB(I2C) SCL Pin
        (7) SCCB(I2C) SDA Pin
```

### Storage Configuration

This component provides flexible file system storage across multiple media types. Supported storage functionalities include:

- FATFS on SPI Flash
- FATFS on SD/MMC cards
- USB MSC on SPI Flash
- USB MSC on SD/MMC cards

**Note:** SD/MMC card support (including FATFS on SD/MMC and USB MSC on SD/MMC) is available only for ESP32-S3 and ESP32-P4 targets.

If you select the "Customized Development Board" option, you must manually set the SD/MMC card parameters to match your board's hardware configuration. Example configuration:

```
Example Video Initialization Configuration  --->
    Select Target Development Board (Customized Development Board)  --->
        ......
        (X) Customized Development Board

    Storage Configuration  --->
        (/spiflash) SPI Flash Mount Point
        (storage) SPI Flash Partition Label
        (/sdmmc) SD/MMC Card Mount Point
        [ ] Format Storage If Mount Fails
        [ ] Always Format Storage At Startup
            SD/MMC Speed Mode (Default Speed (25 MHz))  --->
            SD/MMC Bus Width (1-line mode (D0 only))  --->
        [*] Debug SD/MMC Pins Connections And Pullup Strength
        [ ]     Enable ADC Signal Level Measurement For SD/MMC
        (44) CMD GPIO Pin Number
        (43) CLK GPIO Pin Number
        (39) D0 GPIO Pin Number
        [*] SD card powered by internal LDO
        (4)     Internal LDO ID
```

If your hardware features a 4-line data bus, choose `4-line mode (D0-D3)` as shown:

```
Example Video Initialization Configuration  --->
    Storage Configuration  --->
        SD/MMC Bus Width (4-line mode (D0-D3))  --->
            (X) 4-line mode (D0-D3)
            ( ) 1-line mode (D0 only)
```

Then, assign the GPIO numbers for D1–D3 pins to match your hardware wiring:

```
Example Video Initialization Configuration  --->
    Storage Configuration  --->
        ...
        (40) D1 GPIO Pin Number
        (41) D2 GPIO Pin Number
        (42) D3 GPIO Pin Number
        ...
```

To use USB MSC functionality, add `espressif/esp_tinyusb` as a dependency in your project's `main/idf_component.yml` file:

```yml
dependencies:
  ......
  espressif/esp_tinyusb:
    version: "~2.1.*"
    rules:
      - if: "target in [esp32p4, esp32s3]"
```

To enable SD/MMC GPIO pin debugging, make sure to include the `sd_card` component in your project's CMake setup:

```cmake
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(SD_CARD_COMPONENT_DIR "$ENV{IDF_PATH}/examples/storage/sd_card/sdmmc/components/sd_card")
if (EXISTS "${SD_CARD_COMPONENT_DIR}")
    list(APPEND EXTRA_COMPONENT_DIRS "${SD_CARD_COMPONENT_DIR}")
else()
    message(FATAL_ERROR "Could not find sd_card component at ${SD_CARD_COMPONENT_DIR}")
endif()

project(your_project)
```

Finally, activate the debugging features in menuconfig:

```
Example Video Initialization Configuration  --->
    Storage Configuration  --->
        ...
        [*] Debug SD/MMC Pins Connections And Pullup Strength
        [*]     Enable ADC Signal Level Measurement For SD/MMC
        ...
```
