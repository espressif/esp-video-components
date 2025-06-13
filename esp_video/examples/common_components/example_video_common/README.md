# Example Video Common Component

This component implements the board-level initialization for esp_video, including the MIPI-CSI sensor I2C port, DVP sensor I2C port, and DVP interface. It should be used mainly in the examples of esp_video to simplify the esp_video board-level configuration and initialization.

## Supported Boards and GPIO Pins

| Hardware | ESP32-P4-Function-EV-Board V1.4 | ESP32-P4-Function-EV-Board V1.5 | ESP32-P4-EYE |
|:-:|:-:|:-:|:-:|
| MIPI-CSI I2C SCL Pin        |  8 |  8 | 13 |
| MIPI-CSI I2C SDA Pin        |  7 |  7 | 14 |
| MIPI-CSI I2C Reset Pin      | NA | NA | 26 |
| MIPI-CSI I2C Power-down Pin | NA | NA | 12 |
| MIPI-CSI I2C XCLK           | NA | NA | 11 |
|   |   |   |   |
| DVP I2C SCL Pin             | NA |  8 | NA |
| DVP I2C SDA Pin             | NA |  7 | NA |
| DVP I2C Reset Pin           | NA | 36 | NA |
| DVP I2C Power-down Pin      | NA | 38 | NA |
| DVP XCLK Pin                | NA | 20 | NA |
| DVP PCLK Pin                | NA |  4 | NA |
| DVP V-SYNC Pin              | NA | 37 | NA |
| DVP DE Pin                  | NA | 22 | NA |
| DVP D0 Pin                  | NA |  2 | NA |
| DVP D1 Pin                  | NA | 32 | NA |
| DVP D2 Pin                  | NA | 33 | NA |
| DVP D3 Pin                  | NA | 23 | NA |
| DVP D4 Pin                  | NA |  3 | NA |
| DVP D5 Pin                  | NA |  6 | NA |
| DVP D6 Pin                  | NA |  5 | NA |
| DVP D7 Pin                  | NA | 21 | NA |
|   |   |   |   |
| SPI I2C SCL Pin             | NA |  8 | NA |
| SPI I2C SDA Pin             | NA |  7 | NA |
| SPI I2C Reset Pin           | NA | NA | NA |
| SPI I2C Power-down Pin      | NA | NA | NA |
| SPI XCLK Pin                | NA | 20 | NA |
| SPI CS Pin                  | NA | 37 | NA |
| SPI SCLK Pin                | NA |  4 | NA |
| SPI Data0 I/O Pin           | NA | 21 | NA |

* Note: ESP32-P4-Function-EV v1.4 board and ESP32-P4-EYE don't support the DVP interface camera sensor by default, so if you want to connect the DVP interface camera sensor to these boards, please select the customized board in the menu and configure the GPIO pin and clock based on the actual situation

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

    (33) DVP SCCB I2C SCL Pin
    (32) DVP SCCB I2C SDA Pin
    (-1) DVP Camera Sensor Reset Pin
    (-1) DVP Camera Sensor Power Down Pin
    (20) DVP XCLK Pin
    (21) DVP PCLK Pin
    (23) DVP VSYNC Pin
    (22) DVP DE Pin
    (53) DVP D0 Pin
    (54) DVP D1 Pin
    (52) DVP D2 Pin
    (1) DVP D3 Pin
    (0) DVP D4 Pin
    (45) DVP D5 Pin
    (46) DVP D6 Pin
    (47) DVP D7 Pin

    ......
```
### SPI Development Kit

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