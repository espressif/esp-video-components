Technology Selection
====================

:link_to_translation:`zh_CN:[中文]`

This document mainly introduces how to choose the appropriate Espressif hardware products, camera sensor models, and the initial preparations for the project.

.. important::

    If you have any questions regarding the selection of Espressif hardware products or software solutions, please contact `Espressif Sales <https://www.espressif.com/en/contact-us/sales-questions>`_ or `Technical Support <https://www.espressif.com/en/contact-us/technical-inquiries>`_.

Hardware Selection
------------------

Before using esp_video, you need to select a suitable Espressif chip to integrate into your product. Hardware selection is a complex process that requires consideration of various factors such as functionality, power consumption, cost, and size. Please read the following content to help you select the hardware.

.. list::

    - `Product Selector Tool <https://products.espressif.com/#/product-selector?language=en&names=>`_ can help you understand the hardware differences of different Espressif products.
    - `Datasheets <https://www.espressif.com/en/support/documents/technical-documents?keys=&field_download_document_type_tid%5B%5D=510>`_ can help you understand the hardware capabilities supported by the chip/module.
    - `Board Selection Guide <https://docs.espressif.com/projects/esp-techpedia/en/latest/esp-friends/get-started/board-selection.html>`_ can help you compare the differences between chips, modules, and development boards and provide selection guidance.

Hardware selection recommendations:

- For sensors that output RAW format data, you must select a chip with an ISP, such as ESP32-P4.
- For sensors that output YUV or RGB data, you can select any ESP32-xx chip that has a camera interface.

Software Solution Selection
----------------------------

- **Image transmission:** For resolutions up to 720p, use chips with SPI or DVP interfaces. For resolutions above 720p, use chips with USB or MIPI-CSI interfaces.
- **Video streaming:** For high frame rate requirements, use chips with USB or MIPI-CSI interfaces.
- **AI:** Use chips with an AI acceleration instruction set.
