| Supported Targets | ESP32-P4 |
|-------------------|----------|

# AE Meter Example

*(See the [README.md](../README.md) file in the upper level [examples](../) directory for more information about examples.)*

## Overview

This example shows the camera on a 1024×600 MIPI DSI panel and accepts serial commands for AE, AF, and AWB:

- `meter` / `win` place AF windows and map them onto AE blocks to update the weight table
- `afw` / `winw` change AF window weights
- `up` / `down` add or subtract luma from the JSON AE target
- `ct` holds AWB at a Kelvin value from the color-temperature table (mired interpolation between entries)
- `scan ct` / `scan ae` / `scan af` walk a range with a step and dwell time
- `reset` restores JSON AE weights, AF windows and weights, target offset 0, and the JSON AWB mode

## Serial Commands

Type commands on the USB Serial/JTAG port used for flash (the same port `idf.py monitor` opens). UART0 does not receive commands. Coordinates are the camera frame, not the 1024×600 panel. `(0, 0)` is the top-left pixel of the captured frame.

| Command | Description |
|:--------|:------------|
| `meter <x> <y>` | Place all three AF windows on the default rectangle around this center (about one fifth of the frame). Add `aew` to every overlapping AE block. Clear the luma target offset. |
| `meter <x> <y> <aew>` | Same default window, then set how much to add to overlapping AE blocks. |
| `win <n> <l> <t> <w> <h>` | Add AF window `n` (`0`, `1`, or `2`) at this rectangle. `add win` is the same command. A sixth number is `aew`. |
| `del win <n>` | Delete AF window `n`. Its AF weight becomes 0. When no window remains, AE weights return to the JSON values. |
| `winw <n> <weight>` | Set the AF weight of window `n`. Range is 0 to 255. |
| `aew <n>` | Add `n` to the JSON AE weight of every block that overlaps an AF window. Range is 0 to 255. Startup value is 8. |
| `up [n]` | Add `n` to the JSON luma target and print the target offset. Without `n` the step is 1. |
| `down [n]` | Subtract `n` from the JSON luma target and print the target offset. Without `n` the step is 1. |
| `ct <kelvin>` | Hold AWB at this color temperature. An exact table entry is used as stored. A value between two entries interpolates R/G and B/G in mired. A value outside the table is clamped to the nearest entry. |
| `scan ct <start> <end> <step> <ms>` | Hold AWB at each Kelvin value from `start` to `end`. `step` may be negative. Wait `ms` milliseconds after each value. At most 512 steps. |
| `scan ae <start> <end> <step> <ms>` | Set the luma target offset to each value from `start` to `end`. `step` may be negative. Wait `ms` milliseconds, then print the offset, exposure, and gain. At most 512 steps. |
| `scan af <step> <ms>` | Move the current AF window from left to right, then top to bottom. `step` is pixels and must be greater than 0. Set a window with `meter` or `win` first. At most 512 cells. |
| `afw <w0> <w1> <w2>` | Replace the three AF window weights. `reset` restores the JSON weights. |
| `reset` | Restore the JSON AE weights, AF windows and weights, target offset 0, and the JSON AWB mode. |
| `show` | Print the luma target offset, current luma, exposure, gain, the AE weight grid, the AF windows and weights, and the AWB color temperature. |
| `quit` | Stop the stream after the next frame. |

> **Note**: Every 5×5 AE block that overlaps an active AF window becomes JSON weight plus `aew`. `meter` and `reset` clear the luma target offset. `win`, `winw`, `aew`, and `scan af` keep the current offset. The next IPA process writes the AF windows and restarts the focus scan from the minimum position. A pipeline without AF logs `af meter skipped` and still applies AE. A pipeline without a color-temperature table logs `awb ct skipped`.

## Getting Started

### Hardware Configuration

Before using this example, please refer to the [video initialization configuration guide](../common_components/example_video_common/README.md) for board-level configuration, camera sensor interface, GPIO pins, and clock settings.

LCD bring-up uses `espressif/esp_board_manager` (`esp32_p4_function_ev_board`: EK79007 MIPI DSI 1024×600). Camera init still uses `example_video_common`.

This example also needs:

- ESP32-P4-Function-EV-Board with a MIPI-CSI camera and the on-board 1024×600 EK79007 panel
- USB cable for flash and the serial console

The demo sensor profile is JXH63P (PID `0x848`) with DW9714, RAW10 1280×720 at 30 fps.

### Project Configuration

`sdkconfig.defaults.esp32p4` already enables the ISP pipeline, JXH63P, and DW9714. Use `idf.py menuconfig` only to change the sensor. Do not run `idf.py set-target` first; `bmgr` below sets the target from the board YAML.

## Building and Running

1. **Install the board manager helper** (once; pin the version):
   ```bash
   pip install 'esp-bmgr-assist==0.8.3'
   ```

2. **Select the board** (generates `components/gen_bmgr_codes/`):
   ```bash
   cd esp_video/examples/ae_meter
   idf.py bmgr -b esp32_p4_function_ev_board
   ```

3. **Build and flash the project:**
   ```bash
   idf.py -p PORT flash monitor
   ```

   *(Press `Ctrl-]` to exit the serial monitor)*

4. **For complete setup instructions**, see the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/index.html).

> **Note**: `idf.py bmgr` writes SC2336 into `components/gen_bmgr_codes/board_manager.defaults`. This example's `sdkconfig.defaults.esp32p4` overrides the sensor, so the build keeps JXH63P and DW9714.

## Expected Output

When running this example, you should see output similar to this in the serial monitor:

```
...
I (xxxx) ae_meter: frame=1280x720 ae_add=8
I (xxxx) ae_meter: commands:
I (xxxx) ae_meter:   meter <x> <y> [aew]        default window around this center, all 3 AF
I (xxxx) ae_meter:   win <n> <l> <t> <w> <h> [aew]  add AF window n, n is 0 1 or 2
I (xxxx) ae_meter:   ...
I (xxxx) ae_meter:   reset                    restore JSON AE weights, AF windows, AWB mode
I (xxxx) ae_meter:   show                     print AE weights, AF windows, AWB ct
I (xxxx) ae_meter:   quit                     stop the stream
...
```

After a successful `meter` or `show`, the log prints the AE weight grid, AF windows, and AWB color temperature.

## Using the Console

1. Wait until the command list is printed.
2. Enter a line such as `meter 640 360` or `show`.
3. Use `up` / `down` / `ct` / `scan ...` as needed. AE keeps running; brightness and white balance settle over the next frames.
4. Enter `reset` to restore JSON defaults, or `quit` to stop the stream.

## Troubleshooting

### Common Issues

**1. ISP pipeline not running**

```
meter failed / set target offset failed: ESP_ERR_INVALID_STATE
```

**Solutions:**
- Keep `Enable ISP Pipeline Controller` on and confirm the ISP pipeline controller is initialized
- Use a RAW sensor that has an IPA configuration

**2. Coordinates out of range**

```
meter x y is outside
```

**Solutions:**
- Use pixel coordinates inside the frame size printed at start

**3. Camera device open failed**

```
failed to open ...
```

**Solutions:**
- Check that the sensor interface in `example_video_common` matches the board

**4. LCD stays black**

**Solutions:**
- PSRAM must be HEX mode
- Panel timing must match this 1024×600 EK79007
- Look for `LCD init failed` or `PPA init failed` before the stream starts

**5. Unsupported camera format for the panel**

```
camera format is not YUV420 or UYVY
```

**Solutions:**
- Enable the ISP video device so the panel receives a YUV frame to scale
