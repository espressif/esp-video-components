.. _camera_motor_driver:

Camera Auto-Focus Motor Driver Development
==========================================

:link_to_translation:`zh_CN:[中文]`

Overview
--------

A camera auto-focus motor device is a dedicated component used to implement auto-focus, typically controlled over an I2C interface. As shown below, the Image Signal Processor (ISP) calculates sharpness metrics for each frame and reports them to the Image Processing Algorithms (IPA); the IPA then analyzes these statistics, commands the auto-focus motor to move, and completes focusing.

.. blockdiag::
    :caption: Camera Auto-Focus Motor Data Flow
    :align: center

    blockdiag Camera-motor-Data-Flow {

        # global attributes
        node_height = 60;
        node_width = 100;
        span_width = 50;
        span_height = 20;
        default_shape = roundedbox;

        # labels of diagram nodes
        SENSOR [label="sensor\n frames", fontsize=12];
        ISP [label="ISP\npipelines", fontsize=12];
        IPA [label="IPA", fontsize=12];
        MOTOR [label="motor", fontsize=12];

        # node connections + labels
        SENSOR -> ISP -> IPA;
        IPA -> MOTOR [label="params", fontsize=8];
    }

Driver Architecture
-------------------

The code structure of the camera auto-focus motor driver is as follows:

.. code-block:: none

    ├── include
    │   └── dw9714_types.h
    │   └── dw9714.h
    ├── private_include
    │   └── dw9714_settings.h    # Initialization settings of the motor driver
    └── Kconfig.dw9714
    └── dw9714.c                 # Implementation of the motor driver

.. attention::

    - You can adapt an existing driver for an auto-focus motor with similar specifications to bring up your own driver.
    - There are many possible hardware connection methods between the auto-focus motor and the camera module. Write the driver based on actual hardware needs. It is recommended to reuse pins on the camera sensor and share the SCCB bus with the sensor to save SoC pins.

Feature Overview
----------------

The camera auto-focus motor driver provides the following features:

- :ref:`Device Discovery <device_find>` — includes read/write checks of the motor device and how to release resources after use.
- :ref:`Parameter Control <para_ctrl>` — includes querying supported parameter information, and setting and querying parameter values.
- :ref:`I/O Control <io_ctrl>` — includes controls such as device reset and sleep.
- :ref:`Format Management <fmt-management>` — describes how to enumerate supported formats, and how to set and query formats.
- :ref:`Get Name <motor-get-name>` — describes how to query the current device name during use.
- :ref:`Kconfig Options <motor-kconfig-options>` — lists supported Kconfig options that can affect the driver.

.. _device_find:

Device Discovery
^^^^^^^^^^^^^^^^

If the motor device is correctly connected to the SoC and the driver is functioning, the driver will automatically perform SCCB read/write checks on the motor and return ``esp_cam_motor_device_t`` to represent the device. You must obtain this device descriptor before calling any other ``esp_cam_motor_`` APIs.

Probe a Specific Device via the Motor-Specific API
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Using DW9714 as an example, include the ``dw9714.h`` header in your application code, then call ``dw9714_detect()`` to check whether the DW9714 motor is present on the bus.

Probe All Devices on a Specified SCCB Bus
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The camera auto-focus motor driver provides an array that lets you probe all auto-focus motor devices connected to the specified SCCB bus. Use ``esp_cam_motor_detect_get_array()`` to obtain the start and end pointers of the array, ensuring compatibility with different device detection methods.

.. code-block:: c

    esp_cam_motor_detect_fn_t *array_start = NULL;
    esp_cam_motor_detect_fn_t *array_end = NULL;
    esp_cam_motor_detect_get_array(&array_start, &array_end);
    for (esp_cam_motor_detect_fn_t *p = array_start; p < array_end; p++) {
        esp_cam_motor_config_t cfg = {0};
        esp_cam_motor_device_t *motor_dev;
        const esp_video_init_cam_motor_config_t *cm = config->cam_motor;
        cfg.sccb_handle = create_sccb_device(sccb_mark, ESP_VIDEO_INIT_MOTOR_SCCB, &cm->sccb_config, p->sccb_addr);
        if (!cfg.sccb_handle) {
            return ESP_FAIL;
        }

        cfg.reset_pin = cm->reset_pin;
        cfg.pwdn_pin = cm->pwdn_pin;
        cfg.signal_pin = cm->signal_pin;
        motor_dev = (*(p->detect))((void *)&cfg);
        if (!motor_dev) {
            ESP_LOGE(TAG, "failed to detect motor with address=%x", p->sccb_addr);
            continue;
        }
        break;
    }

Delete Device
~~~~~~~~~~~~~

If the motor is no longer needed, call ``esp_cam_motor_del_dev()`` to release resources.

.. _para_ctrl:

Parameter Control
^^^^^^^^^^^^^^^^^

You can query and set parameters of the camera auto-focus motor using the following functions:

- ``esp_cam_motor_query_para_desc()`` is used to query parameter types and ranges.
- ``esp_cam_motor_get_para_value()`` is used to get current parameter values.
- ``esp_cam_motor_set_para_value()`` is used to set parameter values.

.. _io_ctrl:

I/O Control
^^^^^^^^^^^

I/O control of the motor can be performed by calling ``esp_cam_motor_ioctl()``.

.. _fmt-management:

Format Management
^^^^^^^^^^^^^^^^^

Camera auto-focus motor format information includes operating mode, step information, default position, and more. All information determined by the initialization register list is described by the ``esp_cam_motor_format_t`` structure. By setting and querying this structure, the IPA can more clearly control the focus motor to achieve focus.

- ``esp_cam_motor_query_formats()`` is used to query supported format information.
- ``esp_cam_motor_get_format()`` is used to query the format currently in use.
- ``esp_cam_motor_set_format()`` is used to set the format to use.

.. attention::

    When adjusting the operating mode of the auto-focus motor, note the following:

    - Ringing effect: Due to inertia, moving the motor can cause oscillation. Auto-focus motor driver ICs typically provide vibration-suppression modes, such as the DW9714's DLC (Dual Level Control) mode.
    - Boundary control: Due to inertia, approaching the boundary requires boundary control; otherwise the motor may hit the boundary. If the motor is moved to the farthest position from the image sensor array by default after power-up, a power-off may cause the motor to rapidly return to its initial position, hit the boundary, and produce noise. This is expected behavior.
    - Step size control: Do not move in a single large step; instead, move in multiple smooth small steps to gradually reach the target position. After writing the driver, you can start a timer, increment or decrement the step size in the timer callback, and then use a preview-capable example to observe whether the motor movement is smooth.
    - Due to gravity, the motor requires different step values depending on its orientation (horizontal, vertical up, vertical down). If an orientation sensor is available, use it for optimization.

.. _motor-get-name:

Get Name
^^^^^^^^

Call ``esp_cam_motor_get_name()`` to query the current motor device name.

.. _motor-kconfig-options:

Kconfig Options
^^^^^^^^^^^^^^^

Each auto-focus motor has a configuration file. Using DW9714 as an example, it corresponds to ``esp_cam_sensor/motors/dw9714/Kconfig.dw9714``. Through this file, you can configure the default format to load, whether to enable auto-detection, and more.

In addition, there are some common configuration options:

- ``CONFIG_CAMERA_SENSOR_MOTOR_DETECT_METHOD`` selects the device detection method for camera sensors and auto-focus motors, supporting the following two modes:

  - ``CONFIG_CAMERA_SENSOR_MOTOR_DETECT_METHOD_DYNAMIC_LINK`` (default): Uses the ``esp_cam_motor_detect_fn`` linker section to automatically include all available auto-focus motor detection functions and their configuration data in the current firmware. Even if the application code does not explicitly reference these functions, the linker retains them, enabling auto-detection of all enabled motors. However, the firmware size is relatively larger.
  - ``CONFIG_CAMERA_SENSOR_MOTOR_DETECT_METHOD_STATIC_STORE``: Stores only the auto-focus motor detection functions and their configuration data actually referenced by the application as a C array in flash. Unreferenced detection functions and data can be excluded from the final firmware by the linker, reducing binary size.

API Reference
-------------

.. include-build-file:: inc/esp_cam_motor.inc
