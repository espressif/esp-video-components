*************
Software AOV
*************

:link_to_translation:`zh_CN:[中文]`

.. note::

   The English version of this guide is not available yet.
   Please refer to the Chinese documentation:

   - `Software AOV <https://docs.espressif.com/projects/esp-video-components/zh_CN/latest/esp32p4/Solutions/software_aov.html>`__

   This solution is software AOV, not a hardware-native AOV mode. The application captures one frame and then sleeps, so the same flow can run on MIPI-CSI, DVP, and other camera interfaces. Successive frames have a time gap. The guide covers only low-rate capture and sleep, not recognition or analysis.
