| Supported Targets | ESP32-P4 |
| ----------------- | -------- |

# Fast startup with deep sleep

This example demonstrates fast startup in a timer-wakeup low-power scenario. It stores the IPA state in RTC memory, enters deep sleep, and restores that state on timer wakeup. Restoring the converged state corrects color and brightness immediately, so a frame with normal color and brightness is produced sooner, then saved to the SD card.

On a cold boot the camera streams until the IPA color temperature is inside the menuconfig range. The example then stops the stream, reads the IPA state into RTC memory, and sleeps. This path does not mount the SD card or save a picture. If the color temperature stays outside the range, or a frame does not arrive in time, this boot stops without saving IPA state and without entering deep sleep.

After timer wakeup it writes the saved state back to the ISP and camera before stream on, encodes the first captured frame to JPEG, and saves it to the SD card. The file name uses an index kept in RTC memory. If the saved state is rejected, this boot falls back to the cold path and does not save a picture.

The first frame after wakeup is the first buffer dequeued from the camera. No frame is discarded. An error flag or an empty buffer stops this boot.

## Hardware Required

* An ESP32-P4 board with a MIPI-CSI camera and an SD card slot.
* A camera sensor supported by [esp_cam_sensor](https://github.com/espressif/esp-video-components/tree/master/esp_cam_sensor).

## Configure the Project

Select the board, sensor and SD pins in `Example Configuration`, the same way as the [image storage](../../image_storage/sd_card/README.md) example.

```
Example Configuration  --->
    Minimum color temperature (K)
    Maximum color temperature (K)
    Color temperature wait timeout (ms)
    Frame receive timeout (ms)
    Deep sleep time (seconds)
    JPEG compression quality (%)
    Image file name
```

Enable the ISP pipeline controller:

```
Component config  --->
    Espressif Video Configuration  --->
        Enable ISP based Video Device  --->
            [*] Enable ISP Pipeline Controller
```

`sdkconfig.defaults.esp32p4` already selects this for the ESP32-P4 function EV board v1.5 and SC2336.

## Build and Flash

```
idf.py set-target esp32p4
idf.py -p PORT flash monitor
```

`sdkconfig.defaults.esp32p4` sets the default log level to ERROR and the maximum log level to WARN. Debug traces for the `fast_startup` and `ISP` tags are compiled in only when `LOG_LOCAL_LEVEL` is at least `ESP_LOG_DEBUG`. The example then raises both tags at startup.

After open, both the camera and JPEG devices get a finite `VIDIOC_S_DQBUF_TIMEOUT` from **Frame receive timeout (ms)**, so wakeup first-frame and JPEG `DQBUF` cannot block forever. The JPEG device also gets **JPEG compression quality (%)**. Cold boot then streams the camera until `ct` is inside the configured range. Each recycle `DQBUF` uses the smaller of that receive timeout and the remaining color-temperature wait deadline. A frame that does not arrive within that time stops this boot. After the range is reached, the example stops the stream. A successful boot reads the IPA state into RTC memory and sleeps. Timer wakeup writes that state back before stream on. If the write is rejected, the boot uses the cold path instead. Otherwise it saves `/sdmmc/<Image file name>_no.<index>.jpg`, stops the stream, reads the IPA state again, and sleeps. The index is four digits and increases only after the JPEG is flushed and closed. A failed write removes the incomplete file and leaves the index unchanged. The base name comes from `Image file name` in menuconfig. The directory is the SD/MMC mount point, `/sdmmc` by default.
