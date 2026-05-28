ESP-Video-Components Development Reference
===============================================

:link_to_translation:`zh_CN:[中文]`

This is the documentation center for Espressif's Camera Application Development Framework (`esp-video-components <https://github.com/espressif/esp-video-components>`_). 

`ESP-Video-Components <https://github.com/espressif/esp-video-components>`_ is a camera application framework developed by Espressif.

The main functions provided by ESP-Video-Components include:

- Peripheral drivers supporting four camera interfaces: MIPI, DVP, SPI, and USB
- Allow the simultaneous use of multiple camera sensors
- Supports obtaining data streams from camera sensors via POSIX API and `Linux V4L2 API <https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/v4l2.html>`_
- Support access control for ISP and Codec devices
- Supports automatic invocation of the image processing algorithm library esp-ipa
- Support parameter control for relevant algorithms in esp-ipa via JSON files

By utilizing the ESP-Video-Components, users can quickly build visual applications.

==================  ==================
|Get Started|_       |Camera Sensor|_      
------------------  ------------------
`Get Started`_       `Camera Sensor`_      
==================  ==================

.. |Get Started| image:: ../_static/get-started.png
.. _Get Started: Get_Started/index.html

.. |Camera Sensor| image:: ../_static/sensors.png
.. _Camera Sensor: ESP_Camera_Sensor/index.html

.. toctree::
   :hidden:

   Get Started <Get_Started/index>
   Camera Sensor Development and Testing Guide <ESP_Camera_Sensor/index>
   Index of Abbreviations <index_of_abbreviations>
   Technology selection <Technology_Selection>
   Disclaimer <disclaimer>
   Changelog <Changelog>
