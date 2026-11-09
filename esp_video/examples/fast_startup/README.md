# Fast Startup Examples

The examples in this directory demonstrate fast startup for low-power scenarios.

* [deep_sleep](deep_sleep) is a timer-wakeup example. It stores the IPA state in RTC memory, enters deep sleep, and restores that state on timer wakeup. Restoring the converged state corrects color and brightness immediately, so a frame with normal color and brightness is produced sooner, then saved to the SD card.
* [nvs](nvs) is a power-off example. It keeps the IPA state in an NVS partition, restores it before stream on at the next boot, and saves the first frame to the SD card. NVS is rewritten only when brightness or color temperature has drifted beyond a menuconfig threshold, so most boots do not write flash.
