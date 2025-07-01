# Example Video Common Component

This component implements the board-level initialization for esp_video, including the MIPI-CSI sensor I2C port, DVP sensor I2C port, and DVP interface. It should be used mainly in the examples of esp_video to simplify the esp_video board-level configuration and initialization.

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

* Note: ESP32-P4-Function-EV v1.4 board and ESP32-P4-EYE don't support the DVP interface camera sensor by default, so if you want to connect the DVP interface camera sensor to these boards, please select the customized board in the menu and configure the GPIO pin and clock based on the actual situation

### Customized Development Board Default Configuration

You can try the default GPIO pins configuration of the "Customized Development Board" in the following table when using the "ESP32-XX-DevKitC" development boards:

| Hardware | ESP32-P4 | ESP32-S3 | ESP32-C3 | ESP32-C6 | ESP32-C61 | ESP32-C5 |
|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
| MIPI-CSI I2C SCL Pin        |  8 | NA | NA | NA | NA | NA |
| MIPI-CSI I2C SDA Pin        |  7 | NA | NA | NA | NA | NA |
| MIPI-CSI I2C Reset Pin      | NA | NA | NA | NA | NA | NA |
| MIPI-CSI I2C Power-down Pin | NA | NA | NA | NA | NA | NA |
| MIPI-CSI I2C XCLK           | NA | NA | NA | NA | NA | NA |
|   |   |   |   |
| DVP I2C SCL Pin             |  8 |  5 | NA | NA | NA | NA |
| DVP I2C SDA Pin             |  7 |  4 | NA | NA | NA | NA |
| DVP I2C Reset Pin           | NA | NA | NA | NA | NA | NA |
| DVP I2C Power-down Pin      | NA | NA | NA | NA | NA | NA |
| DVP XCLK Pin                | 20 | 15 | NA | NA | NA | NA |
| DVP PCLK Pin                |  4 | 13 | NA | NA | NA | NA |
| DVP V-SYNC Pin              | 37 |  6 | NA | NA | NA | NA |
| DVP DE Pin                  | 22 |  7 | NA | NA | NA | NA |
| DVP D0 Pin                  |  2 | 11 | NA | NA | NA | NA |
| DVP D1 Pin                  | 32 |  9 | NA | NA | NA | NA |
| DVP D2 Pin                  | 33 |  8 | NA | NA | NA | NA |
| DVP D3 Pin                  | 23 | 10 | NA | NA | NA | NA |
| DVP D4 Pin                  |  3 | 12 | NA | NA | NA | NA |
| DVP D5 Pin                  |  6 | 18 | NA | NA | NA | NA |
| DVP D6 Pin                  |  5 | 17 | NA | NA | NA | NA |
| DVP D7 Pin                  | 21 | 16 | NA | NA | NA | NA |
|   |   |   |   |
| SPI I2C SCL Pin             |  8 |  5 |  5 |  5 |  5 |  5 |
| SPI I2C SDA Pin             |  7 |  4 |  4 |  4 |  4 |  4 |
| SPI I2C Reset Pin           | NA | NA | NA | NA | NA | NA |
| SPI I2C Power-down Pin      | NA | NA | NA | NA | NA | NA |
| SPI XCLK Pin                | 20 | 15 |  8 |  0 |  0 |  0 |
| SPI CS Pin                  | 37 |  6 | 10 |  1 |  8 | 10 |
| SPI SCLK Pin                |  4 | 13 |  6 |  6 |  6 |  6 |
| SPI Data0 I/O Pin           | 21 | 16 |  7 |  7 |  7 |  7 |

## How to use this component

### MIPI-CSI Development Kit

Select your development board and MIPI-CSI interface as follows:

```
Example Video Initialization Configuration  --->
    Select Target Development Board (ESP32-P4-Function-EV-Board V1.4)
        (X) ESP32-P4-Function-EV-Board V1.4
        ( ) ESP32-P4-Function-EV-Board V1.5
        ( ) ESP32-P4-EYE

        ......

        ( ) Customized Development Board
    Camera Sensor Interface (MIPI-CSI)  --->
        (X) MIPI-CSI
        ( ) DVP
        ( ) SPI
```

If your development board is not in the menu, please select "Customized Development Board" and configure the GPIO pins based on your development board as follows:

```
Example Video Initialization Configuration  --->
    Select Target Development Board (ESP32-P4-Function-EV-Board V1.4)
        ( ) ESP32-P4-Function-EV-Board V1.4
        ( ) ESP32-P4-Function-EV-Board V1.5
        ( ) ESP32-P4-EYE

        ......

        (X) Customized Development Board

    ......

    (8) MIPI CSI SCCB I2C SCL Pin
    (7) MIPI CSI SCCB I2C SDA Pin
    (-1) MIPI CSI Camera Sensor Reset Pin
    (-1) MIPI CSI Camera Sensor Power Down Pin

    ......
```

### DVP Development Kit

Select your development board and DVP interface as follows:

```
Example Video Initialization Configuration  --->
    Select Target Development Board (ESP32-P4-Function-EV-Board V1.4)
        (X) ESP32-P4-Function-EV-Board V1.4
        ( ) ESP32-P4-Function-EV-Board V1.5
        ( ) ESP32-P4-EYE

        ......

        ( ) Customized Development Board
    Camera Sensor Interface (MIPI-CSI)  --->
        ( ) MIPI-CSI
        (X) DVP
        ( ) SPI
```

If your development board is not in the menu, please select "Customized Development Board" and configure the GPIO pins based on your development board as follows:

```
Example Video Initialization Configuration  --->
    Select Target Development Board (ESP32-P4-Function-EV-Board V1.4)
        ( ) ESP32-P4-Function-EV-Board V1.4
        ( ) ESP32-P4-Function-EV-Board V1.5
        ( ) ESP32-P4-EYE

        ......

        (X) Customized Development Board

    ......

    (8) DVP SCCB I2C SCL Pin
    (7) DVP SCCB I2C SDA Pin
    (-1) DVP Camera Sensor Reset Pin
    (-1) DVP Camera Sensor Power Down Pin
    (20) DVP XCLK Pin
    (4) DVP PCLK Pin
    (37) DVP VSYNC Pin
    (22) DVP DE Pin
    (2) DVP D0 Pin
    (32) DVP D1 Pin
    (33) DVP D2 Pin
    (23) DVP D3 Pin
    (3) DVP D4 Pin
    (6) DVP D5 Pin
    (5) DVP D6 Pin
    (21) DVP D7 Pin

    ......
```
### SPI Development Kit

Enable SPI video device as follows:

```
Component config  --->
    Espressif Video Configuration  --->
        [*] Enable SPI based Video Device  ----
```

Select your development board and SPI interface as follows:

```
Example Video Initialization Configuration  --->
    Select Target Development Board (ESP32-P4-Function-EV-Board V1.5)
        ( ) ESP32-P4-Function-EV-Board V1.4
        (X) ESP32-P4-Function-EV-Board V1.5
        ( ) ESP32-P4-EYE

        ......

        ( ) Customized Development Board
    Camera Sensor Interface (MIPI-CSI)  --->
        ( ) MIPI-CSI
        ( ) DVP
        (X) SPI
```

If your development board is not in the menu, please select "Customized Development Board" and configure the GPIO pins based on your development board as follows:

```
Example Video Initialization Configuration  --->
    Select Target Development Board (ESP32-P4-Function-EV-Board V1.4)
        ( ) ESP32-P4-Function-EV-Board V1.4
        ( ) ESP32-P4-Function-EV-Board V1.5
        ( ) ESP32-P4-EYE

        ......

        (X) Customized Development Board

    ......

        (8) SPI SCCB I2C SCL Pin
        (7) SPI SCCB I2C SDA Pin
        (-1) SPI Camera Sensor Reset Pin
        (-1) SPI Camera Sensor Power Down Pin
        (20) Output XCLK Pin for SPI Sensor
        (37) SPI Camera Sensor CS Pin
        (4) SPI Camera Sensor SCLK Pin
        (21) SPI Camera Sensor Data0 I/O Pin

    ......
```