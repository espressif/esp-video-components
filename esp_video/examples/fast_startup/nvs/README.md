| Supported Targets | ESP32-P4 |
| ----------------- | -------- |

# Fast startup with NVS

This example demonstrates fast startup when the device is fully powered off between captures. It keeps the IPA state in a dedicated NVS partition, writes it back to the ISP and camera before stream on at the next boot, and saves the first frame to the SD card, so the first frame already has normal color and brightness. Power can be cut without warning, so the state is saved while running, as soon as the IPA is stable.

## Boot flow

```
boot → load esp_ipa_state_t from NVS
  │
  ├─ no cache ──► cold boot
  │     stream on → [save first frame as _cold_first] → wait until stable → save picture → stream off
  │     → get_ipa_state → write NVS
  │
  └─ cache ──► set_ipa_state before stream on
        ├─ rejected ──► cold boot
        └─ stream on → save the first frame → wait until stable → stream off
           → get_ipa_state → write NVS only if brightness or color temperature changed enough
```

* **Stable**: exposure, sensor gain and color temperature stay within their tolerance from frame to frame for several frames in a row. Color temperature is skipped when the AWB does not publish one.
* **Cold boot** always writes NVS. **Warm boot** writes only when the change exceeds the rewrite threshold, so most boots do not write flash.
* The cache lives in its own `ipa_nvs` partition (`partitions.csv`). The example relies only on the checks in `set_ipa_state()`, so run `idf.py erase-flash` after changing the IPA JSON.

## Hardware Required

* An ESP32-P4 board with a MIPI-CSI camera and an SD card slot.
* A camera sensor supported by [esp_cam_sensor](https://github.com/espressif/esp-video-components/tree/master/esp_cam_sensor).

## Configure the Project

Select the board, sensor and SD pins in `Example Configuration`, the same way as the [image storage](../../image_storage/sd_card/README.md) example. Example options:

```
Example Configuration  --->
    Stable frames in a row
    Stable tolerance: exposure and gain (%)
    Stable tolerance: color temperature (K)
    Stable wait timeout (ms)
    Frame receive timeout (ms)
    NVS rewrite threshold: brightness change (%)
    NVS rewrite threshold: color temperature change (K)
    JPEG compression quality (%)
    Image file name
    [*] Save first frame on cold boot
    [ ] Restart after capture
```

`sdkconfig.defaults.esp32p4` already enables the ISP pipeline controller for the ESP32-P4 function EV board v1.5 and SC2336, and enables `CONFIG_BOOTLOADER_SKIP_VALIDATE_ON_POWER_ON` to shorten power-on boot. Read the help of that option before using it in a product.

## Build and Flash

```
idf.py set-target esp32p4
idf.py -p PORT flash monitor
```

The console is USB Serial/JTAG. After a power cycle the host reopens the port too late to see the boot log, so use **Restart after capture**, or switch the console to UART, to watch the warm path.

To see per-frame debug logs, uncomment the `LOG_LOCAL_LEVEL` lines at the top of `main/nvs_main.c`.
