.. _camera_sensor_driver:

Camera Sensor Driver Development
================================

:link_to_translation:`zh_CN:[中文]`

Overview
--------

The camera sensor data flow is as follows:

.. blockdiag::
    :caption: Camera Sensor Data Flow
    :align: center

    blockdiag Camera-sensor-Data-Flow {

        # global attributes
        node_height = 80;
        node_width = 120;
        span_width = 60;
        span_height = 40;
        default_shape = roundedbox;

        # labels of diagram nodes
        SENSOR [label="sensor\n pixel array", fontsize=14];
        AM [label="amplifier", fontsize=14];
        ADC [label="ADC", fontsize=14];
        ISP [label="ISP\npipelines", fontsize=14];
        FIFO [label="FIFO", fontsize=14];
        OUT [label="data output", fontsize=14];

        # node connections + labels
        SENSOR -> AM -> ADC;
        ISP -> FIFO [label="RGB\n or YUV", fontsize=12];
        FIFO -> OUT;

        # arrange nodes vertically
        group {
           orientation = portrait;
           ADC -> ISP [label="RAW", fontsize=12];
        }
    }

Based on the output data format, camera sensors fall into two categories:

- RAW sensors: these sensors can only output data in RAW format.
- YUV sensors: compared with RAW sensors, this type integrates ISP pipelines and can output data in YUV or RGB formats directly.

Driver Architecture
-------------------

The main code structure for each sensor driver includes:

.. code-block:: none

    ├── cfg
    │   ├── sc2336_default.json    # IPA (Image Processing Algorithms) parameter configuration file
    ├── include
    │   └── sc2336_types.h
    │   └── sc2336.h
    ├── private_include
    │   └── sc2336_regs.h
    │   └── sc2336_settings.h     # Initialization settings for the sensor driver
    └── Kconfig.sc2336
    └── sc2336.c                  # Sensor driver implementation

.. attention::

    - For a YUV sensor, a ``JSON`` file for configuring IPA parameters is not required.
    - To add a custom sensor driver, see :ref:`Prepare a Sensor Driver <cam-sensor-driver-prepare>`.
    - For the camera tuning support policy, see `Support Policy <https://github.com/espressif/esp-video-components/blob/master/esp_cam_sensor/SUPPORT_POLICY.md>`_.

Feature Overview
----------------

The esp_cam_sensor driver provides the following features:

- :ref:`Device Discovery <device_probe>` — includes detecting the sensor device ID and how to release resources when finished.
- :ref:`Parameter Control <para_controller>` — includes querying supported parameter information, and setting and querying parameter values.
- :ref:`I/O Control <io_controller>` — includes starting and stopping the device data stream, enabling test mode, and soft reset.
- :ref:`Format Management <format-management>` — describes how to enumerate supported formats, and how to set and query formats.
- :ref:`Get Name <sensor-get-name>` — describes how to query the current device name during use.
- :ref:`Generate XCLK Clock Signal <xclk-gen>` — lists how to generate XCLK clock signal on the SoC.
- :ref:`Kconfig Options <sensor-kconfig-options>` — lists supported Kconfig options that can affect the driver.

.. attention::

    The interfaces in the esp_cam_sensor component are primarily developed and maintained by low-level camera driver developers. Application developers can directly use the esp-video APIs and avoid calling interfaces in the esp_cam_sensor component.

.. _device_probe:

Device Discovery
^^^^^^^^^^^^^^^^

If the sensor is properly connected to the SoC and the driver is functioning, the driver will automatically detect the sensor ID and return ``esp_cam_sensor_device_t`` to represent the device. You must obtain this device descriptor before calling any other ``esp_cam_sensor_`` APIs.

Probe a Specific Device via a Sensor-Specific API
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Using OV2710 as an example, include the ``ov2710.h`` header in your application code, then call the ``ov2710_detect()`` function to check whether the OV2710 sensor is connected on that bus. For a related example, see `detect <https://github.com/espressif/esp-video-components/tree/master/esp_cam_sensor/test_apps/detect>`_.

Probe All Devices on a Specified SCCB Bus
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The esp_cam_sensor component provides an array to let you probe all camera sensors connected to a specified SCCB bus. Use ``esp_cam_sensor_detect_get_array()`` to obtain the start and end pointers of the array, ensuring compatibility with different device detection methods. For a related example, see `example_sensor_init <https://github.com/espressif/esp-idf/blob/master/examples/peripherals/camera/common_components/sensor_init/example_sensor_init.c>`_.

.. code-block:: c

    esp_cam_sensor_detect_fn_t *array_start = NULL;
    esp_cam_sensor_detect_fn_t *array_end = NULL;
    esp_cam_sensor_detect_get_array(&array_start, &array_end);
    for (esp_cam_sensor_detect_fn_t *p = array_start; p < array_end; ++p) {
        esp_cam_sensor_config_t cfg = {0};
        esp_cam_sensor_device_t *sensor_dev;

        cfg.sccb_handle = create_sccb_device(sccb_mark, p->port, &sccb_config, p->sccb_addr);
        if (!cfg.sccb_handle) {
            return ESP_FAIL;
        }

        cfg.reset_pin = reset_pin;
        cfg.pwdn_pin = pwdn_pin;
        sensor_dev = (*(p->detect))((void *)&cfg);
        if (!sensor_dev) {
            destroy_sccb_device(cfg.sccb_handle, sccb_mark, &sccb_config);
            continue;
        }
        break;
    }

Delete Device
~~~~~~~~~~~~~

If the camera device is no longer needed, call ``esp_cam_sensor_del_dev()`` to release resources.

.. _para_controller:

Parameter Control
^^^^^^^^^^^^^^^^^

You can query and set parameters of the camera sensor using the following functions:

- ``esp_cam_sensor_query_para_desc()`` is used to query a parameter's type and range.
- ``esp_cam_sensor_get_para_value()`` is used to query the current value of a parameter.
- ``esp_cam_sensor_set_para_value()`` is used to set a parameter's value.

.. _io_controller:

I/O Control
^^^^^^^^^^^

Input/output control of the camera sensor is done by calling ``esp_cam_sensor_ioctl()``.

.. _format-management:

Format Management
^^^^^^^^^^^^^^^^^

Camera sensor format information includes: output interface, output data format, and output frame rate. All information determined by the initialization register list is described by the ``esp_cam_sensor_format_t`` structure. By setting and querying this structure, the receiver can correctly initialize, receive, and process image data sent by the camera.

- ``esp_cam_sensor_query_format()`` is used to query supported formats.
- ``esp_cam_sensor_get_format()`` is used to query the active format.
- ``esp_cam_sensor_set_format()`` is used to set the active format.

.. _sensor-get-name:

Get Name
^^^^^^^^

Call ``esp_cam_sensor_get_name()`` to query the current sensor's name.

.. _xclk-gen:

Generate XCLK Clock Signal
^^^^^^^^^^^^^^^^^^^^^^^^^^

Proper initialization of a camera sensor requires power, a clock signal, the correct RST level, and the correct PWDN level. The clock signal can be provided by an external crystal oscillator or by the SoC host.

Espressif SoCs can generate this clock signal using peripherals such as LEDC, ESP_CLOCK, and LCD_CAM. For convenience, these peripherals are wrapped as functions named ``esp_cam_sensor_xclk_``. For examples of using XCLK, see `xclk_generator <https://github.com/espressif/esp-video-components/tree/master/esp_cam_sensor/test_apps/xclk_generator>`_.

.. attention::

    Different peripherals support different clock sources. For example, on some chips, the LEDC peripheral only supports an 80 MHz clock source; 80 MHz is not divisible by 24 MHz, so it cannot generate a 24 MHz XCLK. If you are unsure whether a peripheral can generate the specified clock frequency, contact your FAE.

.. _sensor-kconfig-options:

Kconfig Options
^^^^^^^^^^^^^^^

Each sensor has a configuration file; for OV2710, it is ``sensors/ov2710/Kconfig.ov2710``. This file lets you configure the default format to load, whether to enable auto-detection, and more.

In addition, there are some common configuration options:

- ``CONFIG_CAMERA_SENSOR_MOTOR_DETECT_METHOD`` selects the device detection method for camera sensors and auto-focus motors, supporting the following two modes:

  - ``CONFIG_CAMERA_SENSOR_MOTOR_DETECT_METHOD_DYNAMIC_LINK`` (default): Uses the ``esp_cam_sensor_detect_fn`` linker section to automatically include all available sensor detection functions and their configuration data in the current firmware. Even if the application code does not explicitly reference these functions, the linker retains them, enabling auto-detection of all enabled sensors. However, the firmware size is relatively larger.
  - ``CONFIG_CAMERA_SENSOR_MOTOR_DETECT_METHOD_STATIC_STORE``: Stores only the sensor detection functions and their configuration data actually referenced by the application as a C array in flash. Unreferenced detection functions and data can be excluded from the final firmware by the linker, reducing binary size.
- ``CONFIG_CAMERA_XCLK_USE_LEDC`` configures the XCLK generator to use the LEDC peripheral.
- ``CONFIG_CAMERA_XCLK_USE_ESP_CLOCK_ROUTER`` configures the XCLK generator to use the ESP_CLOCK peripheral.

API Reference
-------------

.. include-build-file:: inc/esp_cam_sensor.inc
.. include-build-file:: inc/esp_cam_sensor_xclk.inc
