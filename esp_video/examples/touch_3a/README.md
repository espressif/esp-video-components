| Supported Targets | ESP32-P4 |
|-------------------|----------|

# Touch 3A Example

*(See the [README.md](../README.md) file in the upper level [examples](../) directory for more information about examples.)*

## Overview

This example shows the camera on a 1024×600 MIPI DSI panel with GT911 touch for AE, AF, and AWB:

- Tap the preview for AE spot metering (about one fifth of the frame around the touch point)
- AF follows JSON `af_win0`: a near tap only raises window-0 software weight (`ori_weight + offset`); a far tap recenters `af_win0` only, then applies the same weight bump
- Bottom bar selects AWB (`AUTO`, `3000 K`, `5000 K`, `6500 K`), raises AE weight add and luma target offset with `SUN+`, and restores JSON defaults with `RST`

## Touch Controls

After boot the preview fills the panel. A control bar stays at the bottom. A tap uses the same cover scale, center crop, and output offset as the preview scaler, then clamps the result to the visible camera rectangle. `(0, 0)` is the top-left of the panel.

| Touch | Description |
|:------|:------------|
| Preview area | Always raise AE weights on a spot about one fifth of the frame. For AF: if the tap is within half the diagonal of current `af_win0`, keep hardware windows and set `weight[0] = ori_weight + 8` (cap 255), other windows keep ori weights. If farther, recenter only `af_win0` on the tap, keep win1/win2, then apply the same weight bump. Draw a green crosshair. |
| `AUTO` | Restore the JSON AWB mode. |
| `3K` / `5K` / `65K` | Hold AWB at 3000 K, 5000 K, or 6500 K. |
| `SUN+` | Add 4 to the focus-region AE weight add (`ae_add`, start 8) and add 4 to the luma target offset. Re-apply the current focus window when one is active. |
| `RST` | Restore JSON AE weights, AF windows and weights, target offset 0, and the JSON AWB mode. Clear the crosshair. Refresh cached `af_win0` and ori weights. |

> **Note**: The next IPA process writes the AF windows and restarts the focus scan from the minimum position. A pipeline without AF logs `af ... skipped` and still applies AE. A pipeline without a color-temperature table logs `awb ... skipped`.

## Getting Started

### Hardware Configuration

Before using this example, please refer to the [video initialization configuration guide](../common_components/example_video_common/README.md) for board-level configuration, camera sensor interface, GPIO pins, and clock settings.

LCD bring-up uses `espressif/esp_board_manager` (`esp32_p4_function_ev_board`: EK79007 MIPI DSI 1024×600). GT911 touch shares the camera SCCB I2C bus (GPIO8 SCL, GPIO7 SDA) after `example_video_init()`.

This example also needs:

- ESP32-P4-Function-EV-Board with a MIPI-CSI camera, on-board 1024×600 EK79007 panel, and GT911
- USB cable for flash and the serial console

The demo sensor profile is SC2337P, RAW10 1920×1080 at 30 fps. The focus motor is off. This board has no DW9714, so AF moves the statistics window only.

### Project Configuration

`sdkconfig.defaults.esp32p4` already enables the ISP pipeline and SC2337P, with the focus motor off. Use `idf.py menuconfig` only to change the sensor. Do not run `idf.py set-target` first; `bmgr` below sets the target from the board YAML.

## Building and Running

1. **Install the board manager helper** (once; pin the version):
   ```bash
   pip install 'esp-bmgr-assist==0.8.3'
   ```

2. **Select the board** (generates `components/gen_bmgr_codes/`):
   ```bash
   cd esp_video/examples/touch_3a
   idf.py bmgr -b esp32_p4_function_ev_board
   ```

3. **Build and flash the project:**
   ```bash
   idf.py -p PORT flash monitor
   ```

   *(Press `Ctrl-]` to exit the serial monitor)*

4. **For complete setup instructions**, see the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/index.html).

> **Note**: `idf.py bmgr` writes SC2336 into `components/gen_bmgr_codes/board_manager.defaults`. This example's `sdkconfig.defaults.esp32p4` overrides the sensor, so the build keeps SC2337P and leaves the focus motor off. LCD init stays with `esp_board_manager`. GT911 stays on the camera SCCB bus.

## Expected Output

When running this example, you should see output similar to this in the serial monitor:

```
...
I (xxxx) touch_3a: LCD 1024x600 ready
I (xxxx) touch_3a: GT911 at 0x5d
I (xxxx) touch_3a: [ 1 ] touch 3A ready, camera 1920x1080 on LCD 1024x600
I (xxxx) touch_3a: af_win0 (100,80)-(419,319), near=50% diagonal
I (xxxx) touch_3a: tap near af_win0: AF weight only; tap far: move af_win0; bar: AWB/SUN+/RST
I (xxxx) touch_3a: stream frames started
...
```

After a preview tap, the log prints AE meter and AF near/far results, for example:

```
I (xxxx) touch_3a: ae meter cam=(959,540) win=384x216 ae_add=8
I (xxxx) touch_3a: af far, move win0 to (959,540) size=320x240 w0=9
```

## Using the Touch UI

1. Wait until `touch 3A ready` and `stream frames started` are printed, and the preview is visible.
2. Tap the preview to set AE spot metering and AF (near = weight only, far = move `af_win0`).
3. Use the bottom bar for AWB, `SUN+`, or `RST`.
4. Tap `RST` to restore JSON AE/AF/AWB defaults and clear the crosshair.

## Troubleshooting

### Common Issues

**1. GT911 not found**

```
GT911 not found
```

**Solutions:**
- The touch chip must share the camera SCCB I2C port
- Confirm wiring and that the address is 0x5D or 0x14

**2. Touch init failed**

```
touch init failed: ESP_ERR_INVALID_STATE
```

**Solutions:**
- Keep MIPI-CSI and SCCB defaults from `example_video_common`
- Confirm camera init creates the shared I2C bus before touch probe

**3. ISP pipeline not running**

```
meter reset failed / set weight failed: ESP_ERR_INVALID_STATE
```

**Solutions:**
- Keep `Enable ISP Pipeline Controller` on
- Use a RAW sensor that has an IPA configuration

**4. LCD stays black**

**Solutions:**
- PSRAM must be HEX mode
- Panel timing must match this 1024×600 EK79007
- Look for `LCD init failed` or `PPA init failed` before the stream starts

**5. Touch X/Y reversed**

**Solutions:**
- GT911 mirror flags in `touch_3a_main.c` match Function-EV board defaults (`mirror_x` and `mirror_y` enabled)
- Change them when the panel orientation differs
