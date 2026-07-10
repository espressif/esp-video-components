Get Started
===========

:link_to_translation:`zh_CN:[中文]`

.. important::

    Before using ESP-Video-Components, please read the :doc:`../disclaimer` and follow the terms and precautions therein.

Overview
--------

ESP-Video-Components is Espressif's official development and application framework for camera systems. It mainly includes the following components:

- `esp_cam_sensor <https://github.com/espressif/esp-video-components/tree/master/esp_cam_sensor>`_: This component provides sensor drivers for three camera interfaces: MIPI, DVP, and SPI.
- `esp_ipa <https://github.com/espressif/esp-video-components/tree/master/esp_ipa>`_: This component provides an image processing algorithm library. Usually, for camera sensors that output data in RAW format, real-time control of algorithms such as AE and AWB is required to obtain clearer images.
- `esp_sccb_intf <https://github.com/espressif/esp-video-components/tree/master/esp_sccb_intf>`_: This component provides the driver for the camera control bus.
- `esp_video <https://github.com/espressif/esp-video-components/tree/master/esp_video>`_: This component relies on the esp_cam_sensor, esp_ipa, and esp_h264 components to implement an API compatible with the Linux V4L2 standard. Adding this component to a project enables quick implementation of the desired visual features.

The chips supported by ESP-Video-Components vary by camera sensor interface:

.. list-table::
  :header-rows: 1

  * - SoC
    - MIPI-CSI
    - DVP
    - USB
    - SPI
  * - ESP32-P4
    - supported
    - supported
    - supported
    - supported
  * - ESP32-S3
    -
    - supported
    - supported
    - supported
  * - ESP32-C3
    -
    -
    -
    - supported
  * - ESP32-C5
    -
    -
    -
    - supported
  * - ESP32-C6
    -
    -
    -
    - supported
  * - ESP32-C61
    -
    -
    -
    - supported

System Architecture
-------------------

.. figure:: ../../_static/get_started/esp-video-framework.png
   :align: center
   :alt: esp_video System Framework
   :figclass: align-center

   esp_video System Framework

The system is organized into five layers: PC-side tuning and analysis tools, application layer, application framework layer, device layer, and kernel layer.

- PC-side tuning and analysis tools are used for previewing images, analyzing image quality, tuning image system parameters online, and calibrating image processing unit parameters.
- The application layer primarily uses various APIs provided by esp_video for application development.
- esp_video is the application framework layer. It not only manages the various devices and libraries in the device layer, but also provides efficient and user-friendly APIs to applications. As middleware for the entire system, it provides a unified interface to upper layers and a unified standard to lower-layer components to facilitate compatible control of various devices.
- The device layer includes the underlying implementation of camera sensor devices and image processing control algorithm libraries, and provides calling interfaces to upper layers.
- The kernel layer consists of the operating system and device driver code provided by ESP-IDF.

Development Board Overview
--------------------------

The following development boards feature camera interfaces and can be used for testing. You may also refer to the camera interfaces of these boards for hardware design.

.. only:: esp32p4

    - `ESP32-P4X-Function-EV-Board <https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4x-function-ev-board/index.html>`_
    - `ESP32-P4X-EYE <https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4x-eye/index.html>`_

Build Your First Project
------------------------

Install ESP-IDF
^^^^^^^^^^^^^^^

Please refer to the `Get Started <https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/index.html>`__ chapter of the *ESP-IDF Programming Guide* to set up your development environment. If this is your first time using ESP-IDF, it is recommended that you familiarize yourself with the basic development workflow using the `hello_world <https://github.com/espressif/esp-idf/tree/master/examples/get-started/hello_world>`_ example first.

Run Examples
^^^^^^^^^^^^

The esp-video-components repository contains commonly used `examples <https://github.com/espressif/esp-video-components/tree/master/esp_video/examples>`_. Run the command ``git clone --recursive https://github.com/espressif/esp-video-components.git`` to obtain the source code, then refer to the `README <https://github.com/espressif/esp-video-components/tree/master/esp_video/examples/capture_stream#readme>`_ in the **capture_stream** example to compile and run it.

.. note::

    If you are unable to obtain the latest components from GitHub, you can use `jihulab <https://jihulab.com/esp-mirror/espressif/esp-video-components>`_ as a backup source.

Add Components to a Custom Project
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

You can add the esp_video component by executing the command ``idf.py add-dependency esp_video`` in the root directory of the project. When you need to modify the source code of a component, refer to the `main/idf_component.yml <https://github.com/espressif/esp-video-components/blob/master/esp_video/examples/capture_stream/main/idf_component.yml>`_ file of the capture_stream example and specify the local component path using the ``override_path`` directive.

For more information on component management and usage, please refer to `Component Management and Usage <https://docs.espressif.com/projects/esp-techpedia/en/latest/esp-friends/advanced-development/component-management.html>`_.
