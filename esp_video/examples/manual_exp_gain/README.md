# Manual Exposure and Gain

**Difficulty:** Basic

## Example Brief

This example shows the camera on a 1024×600 MIPI DSI panel, starts the ISP pipeline controller, disables AGC at startup, and keeps a capture stream running so IPA continues for other modules. Serial commands set and read sensor exposure and gain through V4L2 controls on the camera device.

## Typical Scenarios

- Watch exposure and gain changes on the LCD while locking them manually.
- Manually lock exposure and gain while IPA AWB or other algorithms stay active.
- Check exposure and gain ranges on the serial console.
- Compare automatic AGC with fixed exposure and gain settings.

## File Structure

```text
manual_exp_gain/
├── CMakeLists.txt
├── sdkconfig.defaults
├── sdkconfig.defaults.esp32p4
└── main/
    ├── manual_exp_gain_main.c
    ├── CMakeLists.txt
    └── idf_component.yml
```

## Environment Setup

### Hardware

- ESP32-P4-Function-EV-Board with a MIPI-CSI camera and the on-board 1024×600 EK79007 panel.
- The example defaults do not select a camera. `idf.py bmgr` for `esp32_p4_function_ev_board` enables the board sensor. The focus motor stays off.
- USB cable for flash and the serial console.

LCD bring-up uses `espressif/esp_board_manager` (`esp32_p4_function_ev_board`), the same path as `touch_3a`. Camera pins come from `examples/common_components/example_video_common`. Board manager owns the MIPI LDO.

### Default IDF version

ESP-IDF 5.4 or later, as declared in `main/idf_component.yml` and required by `esp_video`.

## Build and Flash

1. Install the board manager helper once, pinned to the same version as `touch_3a`:

```bash
pip install 'esp-bmgr-assist==0.8.3'
```

2. Select the board. This generates `components/gen_bmgr_codes/` and sets the target. Do not run `idf.py set-target` first.

```bash
cd esp_video/examples/manual_exp_gain
idf.py bmgr -b esp32_p4_function_ev_board
```

3. Build and flash:

```bash
idf.py -p PORT flash monitor
```

`idf.py bmgr` writes the board sensor into `components/gen_bmgr_codes/board_manager.defaults`. These example defaults do not override that sensor. The focus motor stays off. LCD init stays with `esp_board_manager`.

`sdkconfig.defaults.esp32p4` enables:

- `Component config` → `Espressif Video Configuration` → `Enable ISP based Video Device`
- `Enable ISP Pipeline Controller`
- `Component config` → `ESP System Settings` → `Channel for console output` → `USB Serial/JTAG Controller`

Select the sensor under `Component config` → `Espressif Camera Sensors Configurations` when the board module is not the one in the board profile. Use a RAW sensor that has an IPA configuration.

## How to Use

After boot the log prints LCD ready, the frame size, turns AGC off, prints the current exposure and gain, then waits for a line on the USB Serial/JTAG port used by `idf.py monitor`. Frames are scaled with PPA and shown on the panel.

| Command | Effect |
| --- | --- |
| `show` | Print AGC on/off, exposure in microseconds, and gain multiplier. |
| `exp <us>` | Set exposure time in microseconds (`V4L2_CID_EXPOSURE_ABSOLUTE`, 100 µs per step). |
| `gain <x>` | Set gain multiplier, for example `gain 1.0` or `gain 2.5`. The closest sensor gain menu entry is selected. |
| `agc off` | Disable AGC so manual exposure and gain are not overwritten. |
| `agc on` | Enable AGC again. |
| `help` | Print the command list. |
| `quit` | Stop the stream after the next frame. |

Startup already runs `agc off`. If AGC is turned on again, IPA will overwrite the manual exposure and gain on later frames.

## Log Output

```text
I (xxxx) manual_exp_gain: LCD 1024x600 ready (board manager)
I (xxxx) manual_exp_gain: camera /dev/video0 1280x720 on LCD 1024x600
I (xxxx) manual_exp_gain: agc off
I (xxxx) manual_exp_gain: agc=off
I (xxxx) manual_exp_gain: exposure=10000 us
I (xxxx) manual_exp_gain: gain=1.000x
I (xxxx) manual_exp_gain: commands:
I (xxxx) manual_exp_gain:   show              print AGC status, exposure (us), and gain
...
I (xxxx) manual_exp_gain: stream frames started
```

## Troubleshooting

- `LCD init failed` / DSI bring-up errors: run `idf.py bmgr -b esp32_p4_function_ev_board` before the build. The panel and MIPI LDO come from that board.
- `set agc off failed` with `ESP_ERR_INVALID_STATE`: keep `Enable ISP Pipeline Controller` on and use a RAW sensor with an IPA JSON.
- `failed to open` the camera device: the sensor interface in `example_video_common` does not match the board.
- `camera format is not YUV420 or UYVY`: PPA preview needs one of these ISP output formats.
- Manual `exp` / `gain` do not stick: run `agc off` first, then set exposure and gain again.
- `QUERY_EXT_CTRL GAIN failed`: the sensor driver does not expose `V4L2_CID_GAIN` for the current format.
- Serial commands ignored: use the USB Serial/JTAG console selected in `sdkconfig.defaults.esp32p4`.

## Technical Support

- [esp-video-components issues](https://github.com/espressif/esp-video-components/issues)
