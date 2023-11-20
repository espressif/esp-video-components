# SCCB

## Overview

The Serial Camera Control Bus (SCCB) interface controls the image sensor operation. Refer to the *OmniVision Technologies Serial Camera Control Bus (SCCB) Specification* for detailed usage of the serial control port.

In ESP32-Libcamera, the SCCB bus is implemented through I2C or I3C peripherals.

Note: This needs to allow multiple sccb ports to be initialized. SCCB port that allows multiple I2C instantiations, SCCB port that allows I3C instantiation. Keep I2C and I3C mutually exclusive. The coexistence issue of I2C and I3C can be decided at a later stage.

## [API Reference]()

### Header File[]()

- sccb/include/sccb.h

## [Examples]()

Similar to I2C and I3C, SCCB allows co-existence of multiple slaves on the same bus. When there are multiple slaves(the camera sensor, as one of the slaves), The `esp_mipi_csi_driver_install()` will decide whether to use an initialized port according to the configured parameter `pin_sccb_sda`.

## Using uninitialized sccb port

## Using an already initialized sccb port







