ESP-Video-Components Development Reference
==========================================

:link_to_translation:`zh_CN:[中文]`

This is the documentation hub for Espressif's camera application development components.

`ESP-Video-Components <https://github.com/espressif/esp-video-components>`_ is a camera application framework developed by Espressif, which mainly provides the following features:

- Application development support for four camera interfaces: MIPI, DVP, SPI, and USB
- Support for using multiple camera sensors simultaneously
- Support for acquiring data streams from camera sensors via the POSIX API and the `Linux V4L2 API <https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/v4l2.html>`_
- Support for access control for ISP and codec devices
- Support for integrating the `esp-ipa <https://github.com/espressif/esp-video-components/tree/master/esp_ipa>`_ image processing algorithm library
- Support for controlling parameters of relevant algorithms in esp-ipa via JSON files

By using ESP-Video-Components, you can build vision applications efficiently.

==================================  ================
|Get Started|_                      |Camera Sensors|_
----------------------------------  ----------------
`Get Started`_                      `Camera Sensors`_
==================================  ================

.. |Get Started| image:: ../_static/get-started.png
.. _Get Started: Get_Started/index.html

.. |Camera Sensors| image:: ../_static/sensors.png
.. _Camera Sensors: ESP_Camera_Sensor/index.html

.. toctree::
   :hidden:

   Get Started <Get_Started/index>
   Espressif Camera Peripheral Development and Tuning Guide <ESP_Camera_Sensor/index>
   Index of Abbreviations <index_of_abbreviations>
   Technology Selection <Technology_Selection>
   Disclaimer <disclaimer>
   Changelog <Changelog>
