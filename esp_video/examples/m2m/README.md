| Supported Targets | ESP32-P4 | ESP32-S31 |
| ----------------- | -------- | --------- |

# M2M Codec Test Example

(See the [README.md](../README.md) file in the upper level [examples](../) directory for more information about examples.)

> **Note:** This example is intended solely for software and hardware performance evaluation and validation. **It is not recommended for direct use in production projects.** If you need to integrate any of the related code into your own application, you should add proper exception management based on your software's execution flow.

This example measures capture and hardware codec pipeline performance. V4L2 operations are wrapped by `example_v4l2.c` with a high-level API (`open_camera`/`camera_capture_image`, `open_jpeg_encoder`/`jpeg_encode`, `open_jpeg_decoder`/`jpeg_decode`, `open_h264_encoder`/`h264_encode`, and device connect helpers). Frame statistics are collected in `m2m_main.c`.

Each run always performs a **capture-only** benchmark first, then runs the selected M2M test mode.

Use menuconfig to select the test mode:

- **Encode**: onboard camera capture device -> hardware MJPEG or H.264 encoder
- **Decode**: USB UVC MJPEG capture -> hardware JPEG decoder (default)
- **Pipeline**: USB UVC MJPEG capture -> hardware JPEG decoder -> hardware H.264 encoder

Each test reports:

- **Capture test**: FPS, skipped frames, average frame size, and frame interval (min/max/avg)
- **Encode / Decode test**: pipeline FPS, codec-only FPS, skipped frames, average output size, and per-frame codec time (min/max/avg)
- **Pipeline test**: per-stage FPS and time/frame for JPEG decode and H.264 encode

## How to use example

### Configure the Project

Please refer to the [example video common configuration](../common_components/example_video_common/README.md) for board-level settings.

Select the test mode under `Example Configuration`:

```
Example Configuration  --->
    M2M test mode  --->
        ( ) Encode (capture -> codec)
        (X) Decode (JPEG capture -> decode)
        ( ) Pipeline (JPEG capture -> decode -> H.264 encode)
    Frame rate test duration (seconds)  (5)
```

#### Encode mode

- Use an onboard camera sensor (MIPI-CSI / DVP / SPI) configured in `Example Configuration`.
- Enable the target hardware codec under `Component config -> Espressif Video Configuration`.
- MJPEG encoder takes priority when both MJPEG and H.264 encoders are enabled.
- Optional encoder settings are available under `Example Configuration -> Video Encoding Configuration`:
  - JPEG compression quality (MJPEG encoder only)
  - H.264 intra frame period, target bitrate, and min/max QP

Default target settings:

| Target    | Camera sensor | Enabled codecs        |
| --------- | ------------- | --------------------- |
| ESP32-P4  | SC2336        | MJPEG + H.264 encoder |
| ESP32-S31 | SC101IOT      | MJPEG encoder         |

#### Decode mode

- Enable `USB-UVC` camera sensor under `Example Configuration`.
- Ensure `ESP_VIDEO_ENABLE_JPEG_DEC_VIDEO_DEVICE` is enabled in `Component config -> Espressif Video Configuration`.
- The capture device must output MJPEG (`V4L2_PIX_FMT_JPEG`).

- **Note**: The camera sensor should output JPEG image with YUV420 down sampling format.

#### Pipeline mode

- Requires both JPEG hardware decoder and H.264 hardware encoder to be enabled.
- Use a USB UVC camera that outputs MJPEG, same as decode mode.
- H.264 encoder parameters can be tuned under `Example Configuration -> Video Encoding Configuration`.
- The pipeline uses V4L2 device links (`camera_connect`, `jpeg_decode_connect`) to connect capture, decode, and encode stages.

- **Note**: The camera sensor should output JPEG image with YUV420 down sampling format.

### Build and Flash

```
idf.py -p PORT flash monitor
```

CI presets are provided for automated builds:

- `sdkconfig.ci.esp32p4.decoder` — decode mode
- `sdkconfig.ci.esp32p4.pipeline` — pipeline mode

## Example Output

### Capture + Encode (MJPEG)

```
I (1210) m2m_example: Capture test: 5 seconds
I (6210) m2m_example: Capture statistics:
I (6210) m2m_example:   Duration:       5.00 s
I (6210) m2m_example:   Frames:         150
I (6210) m2m_example:   Skipped:        0
I (6210) m2m_example:   FPS:            30.00
I (6210) m2m_example:   Avg frame size: 460800 bytes
I (6210) m2m_example:   Frame interval: min 32.10 ms, max 34.50 ms, avg 33.33 ms
I (6220) m2m_example: JPEG encode test: 5 seconds
I (11220) m2m_example: JPEG Encode statistics:
I (11220) m2m_example:   Duration:       5.00 s
I (11220) m2m_example:   Frames:         142
I (11220) m2m_example:   Skipped:        0
I (11220) m2m_example:   Pipeline FPS:   28.40
I (11220) m2m_example:   Codec FPS:      35.20
I (11220) m2m_example:   Avg output size: 85234 bytes
I (11220) m2m_example:   Codec time/frame: min 25.10 ms, max 30.80 ms, avg 28.40 ms
```

### Capture + Decode (MJPEG)

```
I (2375) m2m_example: Capture test: 5 seconds
I (7385) m2m_example: Capture statistics:
I (7385) m2m_example:   Duration:       5.01 s
I (7385) m2m_example:   Frames:         98
I (7385) m2m_example:   Skipped:        0
I (7395) m2m_example:   FPS:            19.75
I (7395) m2m_example:   Avg frame size: 64342 bytes
I (7395) m2m_example:   Frame interval: min 49.59 ms, max 100.31 ms, avg 51.16 ms
I (7405) example_v4l2: JPEG decoder:
I (7415) example_v4l2: version: 2.2.0
I (7415) example_v4l2: driver:  JPEG_DEC
I (7415) example_v4l2: card:    JPEG_DEC
I (7425) example_v4l2: bus:     esp32p4:JPEG_DEC
I (7425) example_v4l2: JPEG decoder opened
I (7425) example_v4l2: Camera connected to JPEG decoder: 1280x720, in=JPEG
I (7435) m2m_example: JPEG decode test: 5 seconds
I (12465) m2m_example: JPEG Decode statistics:
I (12465) m2m_example:   Duration:       5.02 s
I (12465) m2m_example:   Frames:         100
I (12465) m2m_example:   Skipped:        0
I (12465) m2m_example:   Pipeline FPS:   19.92
I (12475) m2m_example:   Codec FPS:      112.83
I (12475) m2m_example:   Avg output size: 1382400 bytes
I (12475) m2m_example:   Codec time/frame: min 8.77 ms, max 9.36 ms, avg 8.86 ms
```

### Capture + Pipeline

```
I (1210) m2m_example: Capture test: 5 seconds
I (6210) m2m_example: Capture statistics:
I (6210) m2m_example:   Duration:       5.00 s
I (6210) m2m_example:   Frames:         150
I (6210) m2m_example:   Skipped:        0
I (6210) m2m_example:   FPS:            30.00
I (6210) m2m_example:   Avg frame size: 45678 bytes
I (6210) m2m_example:   Frame interval: min 32.10 ms, max 34.50 ms, avg 33.33 ms
I (6220) m2m_example: Pipeline test (capture -> JPEG decode -> H.264 encode): 5 seconds
I (11220) m2m_example: JPEG decode statistics:
I (11220) m2m_example:   Duration:       5.00 s
I (11220) m2m_example:   Frames:         140
I (11220) m2m_example:   Skipped:        2
I (11220) m2m_example:   FPS:            28.00
I (11220) m2m_example:   Total time:     5000.00 ms
I (11220) m2m_example:   Time/frame:     min 30.10 ms, max 38.50 ms, avg 35.71 ms
I (11230) m2m_example: H.264 encode statistics:
I (11230) m2m_example:   Duration:       5.00 s
I (11230) m2m_example:   Frames:         140
I (11230) m2m_example:   Skipped:        0
I (11230) m2m_example:   FPS:            28.00
I (11230) m2m_example:   Total time:     4900.00 ms
I (11230) m2m_example:   Time/frame:     min 32.00 ms, max 40.20 ms, avg 35.00 ms
```

## Troubleshooting

* If the test is skipped with `skip test because capture format is not supported`, the camera does not provide a format compatible with the selected mode. Use MJPEG output for decode/pipeline modes, or a raw/YUV format supported by the encoder for encode mode.
* If decode or pipeline mode fails to open the camera, enable `USB-UVC` under `Example Configuration` and connect a UVC camera that outputs MJPEG.
* If VFS registration fails for a codec device, check `idf.py menuconfig` and ensure the corresponding hardware codec device is enabled.
* If pipeline mode is not available in menuconfig, enable both JPEG hardware decoder and H.264 hardware encoder in `Component config -> Espressif Video Configuration`.
