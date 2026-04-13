| Supported Targets | ESP32-P4 | ESP32-S3 | ESP32-C3 | ESP32-C6 | ESP32-C5 |
|-------------------|----------|----------|----------|----------|----------|

# Espressif Video V4L2 Command Example

*(See the [README.md](../README.md) file in the upper level [examples](../) directory for more information about examples.)*

## Overview

This example provides a command-line interface for controlling V4L2 video devices, similar to the Linux `v4l2-utils` package. It enables you to:

- Query device information, capabilities, and supported formats
- Set control parameters (gain, exposure, etc.)
- Capture video frames to files
- Convert image formats using memory-to-memory devices
- Control ISP (Image Signal Processor) pipeline parameters

### Supported Commands

- **v4l2-ctl**: Main command for controlling V4L2 video devices (list devices, query info, set controls, capture frames, format conversion)
- **v4l2-bf**: Control Bayer filter (BF) for ISP devices
- **v4l2-ccm**: Control color correction matrix (CCM) for ISP devices
- **v4l2-gamma**: Control gamma correction for ISP devices

### Important Notes

> **Note 1**: `v4l2-ctl` is designed to be similar to `v4l-utils`, but not all parameters and output information are fully compatible. Some features may differ from the standard Linux implementation.

> **Note 2**: The ISP-specific commands (`v4l2-bf`, `v4l2-ccm`, `v4l2-gamma`) are only available for ISP-based video devices. They are **not supported** for direct capture devices such as MIPI-CSI, DVP, or SPI camera interfaces.

> **Note 3**: If the ISP pipeline controller and image processing algorithms are enabled, the ISP configuration parameters will be automatically updated by the pipeline controller. To manually control these parameters using commands, you need to disable the ISP pipeline controller:

```
    Component config  --->
        Espressif Video Configuration  --->
            [*] Enable ISP based Video Device  --->
                [ ] Enable ISP Pipeline Controller
```

## Commands Introduction

### v4l2-ctl

Control V4L2 type video device.

#### 1. --list-devices

List all available V4L2 video devices.

**Syntax:**
```
v4l2-ctl --list-devices
```

**Description:**
- Displays all registered V4L2 video devices, including MIPI-CSI, DVP, SPI camera devices, and USB UVC devices
- Shows device names and their corresponding paths (e.g., `/dev/video0`, `/dev/video1`)

**Example:**
```
v4l2-ctl --list-devices
```

#### 2. --all

Display comprehensive device information including driver info, capabilities, formats, and controls.

**Syntax:**
```
v4l2-ctl --all
```

**Description:**
- Shows driver information (name, version, capabilities)
- Lists all supported video formats with their dimensions and pixel formats
- Displays all available controls with their types, ranges, and default values

**Example:**
```
v4l2-ctl --all
```

#### 3. --list-formats

Display all supported video formats for the specified device.

**Syntax:**
```
v4l2-ctl --list-formats [OPTIONS]
```

**Options:**
- `-d, --device <dev>`: Specify the device to query. Default: `/dev/video0`

**Description:**
- Lists all pixel formats supported by the device
- For memory-to-memory (M2M) devices, shows both capture and output formats
- Format information includes pixel format fourcc codes

**Example:**
```
v4l2-ctl --list-formats
v4l2-ctl --list-formats -d /dev/video0
```

#### 4. -c/--set-ctrl

Set control values for the video device.

**Syntax:**
```
v4l2-ctl -c/--set-ctrl <ctrl>=<val>[,<ctrl>=<val>...] [OPTIONS]
```

**Options:**
- `-d, --device <dev>`: Specify the device to control. Default: `/dev/video0`

**Description:**
- Sets one or more control values
- Multiple controls can be set in a single command by separating them with commas
- Control names are case-sensitive and must match exactly as shown in `--all` output
- Only integer values are currently supported

**Examples:**
```
v4l2-ctl -c Gain=1
v4l2-ctl -c Gain=10,Exposure=500
v4l2-ctl -d /dev/video0 -c Vertical\ Flip=1
```

#### 5. --stream-to

Capture video frames and save them to a file.

**Syntax:**
```
v4l2-ctl --stream-to=<file> [OPTIONS]
```

**Options:**
- `-d, --device <dev>`: Specify the capture device. Default: `/dev/video0`
- `--set-fmt-video <params>`: Set video capture format. Parameters: `width=<w>,height=<h>,pixelformat=<pf>`
  - `pixelformat` can be either the format index from `--list-formats` or a fourcc string (e.g., `RGBP`, `BA81`)
- `--stream-mmap <count>`: Number of memory-mapped buffers to allocate. Default: 3
- `--stream-skip <count>`: Number of initial frames to skip before capturing. Default: 3
- `--stream-count <count>`: Number of frames to capture. Default: 1

**Description:**
- Captures video frames using memory-mapped buffers
- Supports various pixel formats including RAW Bayer, RGB, YUV formats
- The output file format depends on the pixel format used (raw binary data)
- Ensure the storage device (e.g., SD card or USB MSC) is mounted and accessible

**Examples:**
```
# Capture RAW8 (BGGR8) format image
v4l2-ctl -d /dev/video0 --stream-mmap=4 --stream-skip=10 --stream-count=1 --set-fmt-video=pixelformat=BA81 --stream-to=output1.raw

# Capture RGB565 format image
v4l2-ctl -d /dev/video0 --set-fmt-video=width=1280,height=720,pixelformat=RGBP --stream-mmap=3 --stream-skip=3 --stream-count=1 --stream-to=output1.rgb

# Capture YUV420 format image
v4l2-ctl -d /dev/video0 --set-fmt-video=width=1280,height=720,pixelformat=YU12 --stream-mmap=3 --stream-skip=3 --stream-count=1 --stream-to=output1.yuv
```

#### 6. --stream-from + --stream-to

Transform an input image file to a different format using a memory-to-memory (M2M) video device.

**Syntax:**
```
v4l2-ctl --stream-from=<input_file> --stream-to=<output_file> [OPTIONS]
```

**Options:**
- `-d, --device <dev>`: Specify the M2M device (e.g., JPEG encoder). Default: `/dev/video0`
- `--set-fmt-video <params>`: Set video capture (output) format. Parameters: `width=<w>,height=<h>,pixelformat=<pf>`
- `--set-fmt-video-out <params>`: Set video output (input) format. Parameters: `width=<w>,height=<h>,pixelformat=<pf>`

**Description:**
- Performs format conversion using M2M video devices (e.g., JPEG encoder/H.264 encoder)
- Reads input image from a file and writes the converted output to another file
- Commonly used for converting RGB/YUV formats to JPEG or H.264
- Both input and output formats must be specified

**Example:**
```
# Convert RGB565 format image to JPEG format
v4l2-ctl -d /dev/video10 --set-fmt-video=width=1280,height=720,pixelformat=JPEG --set-fmt-video-out=width=1280,height=720,pixelformat=RGBP --stream-from=output1.rgb --stream-to=output1.jpg
```

### v4l2-bf

Control the Bayer filter (BF) process for ISP-based video devices.

**Note:** This command is only available for ISP video devices and is not supported for capture devices (MIPI-CSI, DVP, SPI).

**Syntax:**
```
v4l2-bf --all|--enable|--disable [OPTIONS]
```

**Options:**
- `-d, --device <dev>`: Specify the ISP device. Default: `/dev/video0`
- `-l, --level <level>`: Set BF denoising level (integer value)
- `-m, --matrix <params>`: Set BF denoising matrix. Format: `-m <offset>=<value>[,<offset>=<value>...]`

**Description:**
- `--all`: Display current BF state and configuration
- `--enable`: Enable BF with optional level or matrix configuration
- `--disable`: Disable BF processing

**Examples:**
```
# Display current BF configuration
v4l2-bf --all

# Enable BF with denoising level 10
v4l2-bf --enable -l 10

# Enable BF with custom matrix (9x9 matrix values)
v4l2-bf --enable -m 0=0,1=1,2=3,3=1,4=3,5=1,6=0,7=1,8=0

# Set specific matrix element
v4l2-bf --enable -m 0=0

# Disable BF
v4l2-bf --disable
```

### v4l2-gamma

Control the gamma correction process for ISP-based video devices.

**Note:** This command is only available for ISP video devices and is not supported for capture devices (MIPI-CSI, DVP, SPI).

**Syntax:**
```
v4l2-gamma --all|--enable|--disable [OPTIONS]
```

**Options:**
- `-d, --device <dev>`: Specify the ISP device. Default: `/dev/video0`
- `-g, --gamma <value>`: Set gamma correction value (floating point, typically 0.0-2.0)

**Description:**
- `--all`: Display current gamma state and configuration
- `--enable`: Enable gamma correction with the specified value
- `--disable`: Disable gamma correction

**Examples:**
```
# Display current gamma configuration
v4l2-gamma --all

# Enable gamma correction with value 0.9
v4l2-gamma --enable -g 0.9

# Disable gamma correction
v4l2-gamma --disable
```

### v4l2-ccm

Control the color correction matrix (CCM) process for ISP-based video devices.

**Note:** This command is only available for ISP video devices and is not supported for capture devices (MIPI-CSI, DVP, SPI).

**Syntax:**
```
v4l2-ccm --all|--enable|--disable [OPTIONS]
```

**Options:**
- `-d, --device <dev>`: Specify the ISP device. Default: `/dev/video0`
- `-m, --matrix <params>`: Set CCM matrix values. Format: `-m <offset>=<value>[,<offset>=<value>...]`
  - Matrix values can be floating point numbers
  - The CCM is typically a 3x3 matrix (9 elements total)

**Description:**
- `--all`: Display current CCM state and configuration
- `--enable`: Enable CCM with the specified matrix values
- `--disable`: Disable color correction matrix processing

**Examples:**
```
# Display current CCM configuration
v4l2-ccm --all

# Enable CCM with matrix values (diagonal elements)
v4l2-ccm --enable -m 0=2,4=1.9,8=2

# Update specific matrix element
v4l2-ccm --enable -m 3=-0.4

# Disable CCM
v4l2-ccm --disable
```

## Getting Started

### Hardware Configuration

Before using this example, please refer to the [video initialization configuration guide](../common_components/example_video_common/README.md) for detailed information about:
- Board-level configuration
- Camera sensor interface setup
- GPIO pin assignments
- Clock frequency settings

### Project Configuration

Open the project configuration menu:

```bash
idf.py menuconfig
```

#### Camera Sensor Configuration

Navigate to **Espressif Camera Sensors Configurations**:
- Select the camera sensor you want to use
- Choose the target output format for the sensor

#### Example-Specific Configuration

1. **Set the target platform:**
   ```bash
   idf.py set-target esp32p4
   idf.py menuconfig
   ```

2. **Camera sensor interface selection:**
   
   The example will initialize all enabled camera sensors:
   
   ```
   Example Video Initialization Configuration  --->
       Select and Set Camera Sensor Interface  --->
           [*] MIPI-CSI  ---
   ```

3. **Shared I2C bus configuration:**
   
   If your camera sensors share the same I2C GPIO pins (e.g., MIPI-CSI and DVP sensors on the ESP32-P4-Function-EV-Board V1.5):
   
   ```
   Example Video Initialization Configuration  --->
       [*] Use Pre-initialized SCCB(I2C) Bus for All Camera Sensors And Motors
           (0) SCCB(I2C) Port Number
           (8) SCCB(I2C) SCL Pin
           (7) SCCB(I2C) SDA Pin
   ```

4. **Select target camera sensors:**
   
   Choose sensors based on your development board:
   
   ```
   Component config  --->
       Espressif Camera Sensors Configurations  --->
           Camera Sensor Configuration  --->
               Select and Set Camera Sensor  --->
                   [ ] GC0308  ----
                   ......
                   [*] SC2336  --->
   ```

5. **Optimize DVP interface performance:**
   
   For better frame rates with DVP interface camera sensors:
   
   ```
   Component config  --->
       Espressif Camera Sensors Configurations  --->
           Camera Sensor Configuration  --->
               Select and Set Camera Sensor  --->
                   [*] OV2640  ---->
                       Select default output format for DVP interface (JPEG 640x480 25fps, DVP 8-bit, 20M input)  --->
                           ( ) YUV422 640x480 6fps, DVP 8-bit, 20M input
                           (X) JPEG 640x480 25fps, DVP 8-bit, 20M input
                           ( ) RGB565 240x240 25fps, DVP 8-bit, 20M input
   ```

## Building and Running

1. **Build and flash the project:**
   ```bash
   idf.py -p PORT flash monitor
   ```
   
   *(Press `Ctrl-]` to exit the serial monitor)*

2. **For complete setup instructions**, see the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/index.html).

## Expected Output

### Initialization

When running this example, you should see output similar to this in the serial monitor:

```
...
I (1241) main_task: Started on CPU0
I (1251) esp_psram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1251) main_task: Calling app_main()
I (1251) example_init_video: MIPI-CSI camera sensor I2C port=0, scl_pin=8, sda_pin=7, freq=100000
I (1251) sc2336: Detected Camera sensor PID=0xcb3a

Type 'help' to get the list of commands.
Use UP/DOWN arrows to navigate through command history.
Press TAB when typing command name to auto-complete.
I (1351) main_task: Returned from app_main()
video:>
...
```

### Command Examples

#### List Available Devices

```
video:> v4l2-ctl --list-devices
MIPI-CSI (esp32p4:MIPI-CSI):
	/dev/video0
```

#### Display Device Information

```
video:> v4l2-ctl --all
```

**Output:**
```
Driver Info:
        Driver name      : MIPI-CSI
        Card type        : MIPI-CSI
        Bus info         : esp32p4:MIPI-CSI
        Driver version   : 1.4.1
        Capabilities     : 84200001
                Video Capture
                Streaming
                Extended Pix Format
                Device Capabilities
        Device Caps      : 4200001
                Video Capture
                Streaming
                Extended Pix Format
Format Video Capture:
        Width/Height     : 1280/720
        Pixel Format     : BA81

        Width/Height     : 1280/720
        Pixel Format     : RGBP

        Width/Height     : 1280/720
        Pixel Format     : RGB3

        Width/Height     : 1280/720
        Pixel Format     : YU12

        Width/Height     : 1280/720
        Pixel Format     : 422P


Controls

                           Gain 0x00980913 (intmenu)    : min=0 max=192 step=1 default=0
                       Exposure 0x00980911 (int)        : min=8 max=1244 step=1 default=894
                Group Operation 0x009a092a (u8)         : min=0 max=255 step=1 default=0
        Exposure Time, Absolute 0x009a0902 (int)        : min=2 max=332 step=1 default=238
                  Vertical Flip 0x00980915 (int)        : min=0 max=1 step=1 default=0
```

#### List Supported Formats

```
video:> v4l2-ctl --list-formats
ioctl: VIDIOC_ENUM_FMT
	Type : Video Capture

	[0]: BA81
	[1]: RGBP
	[2]: RGB3
	[3]: YU12
	[4]: 422P
```

#### Set Control Values

```
video:> v4l2-ctl -c Gain=10
video:> v4l2-ctl -c Exposure=500,Vertical\ Flip=1
```

#### Capture Image

```
video:> v4l2-ctl -d /dev/video0 --set-fmt-video=width=1280,height=720,pixelformat=RGBP --stream-mmap=3 --stream-skip=3 --stream-count=1 --stream-to=/data/image.rgb
I (12345) v4l2-ctl: skip_count=3, mmap_count=3, stream_count=1
I (12500) v4l2-ctl: wrote 1843200 bytes to file=/data/image.rbg
```

## Troubleshooting

### Common Issues

**1. I2C Transaction Errors**

```
E (1595) i2c.master: I2C transaction unexpected nack detected
E (1595) i2c.master: s_i2c_synchronous_transaction(870): I2C transaction failed
```

**Solutions:**
- Verify that the camera sensor is properly connected to the development board
- Check that the I2C pins (SCL/SDA) are correctly configured in menuconfig
- Ensure the I2C pull-up resistors are present on your board
- Verify the camera sensor power supply is stable

## Usage Tips

### Device Naming

- Device paths follow the pattern `/dev/video<N>` where `<N>` is a number (e.g., `/dev/video0`, `/dev/video1`)
- You can use either the full path (`/dev/video0`) or just the number (`0`) with the `-d` option
- USB UVC devices are typically named `/dev/video10` through `/dev/video19`

### Command Features

- **Command History**: Use UP/DOWN arrow keys to navigate through previously entered commands
- **Auto-completion**: Press TAB to auto-complete command names while typing
- **Help**: Type `help` to get the list of all available commands

### File Storage

- Before capturing images, ensure that a storage device (SD card or USB MSC) is properly mounted
- The default storage path is `/data/` for SD card or USB MSC
- Make sure you have sufficient free space on the storage device

### Format Conversion

- For format conversion (e.g., RGB to JPEG), use a memory-to-memory (M2M) device such as the JPEG encoder
- JPEG encoder devices are typically `/dev/video10` or higher
- Both input and output formats must be specified when using `--stream-from` and `--stream-to`

### ISP Commands

- ISP-specific commands (`v4l2-bf`, `v4l2-ccm`, `v4l2-gamma`) only work with ISP video devices
- These commands are not available for direct capture devices (MIPI-CSI, DVP, SPI)
- To use ISP commands, ensure ISP video device is enabled in menuconfig

## Additional Notes

- All commands support command history navigation using UP/DOWN arrow keys
- Press TAB to auto-complete command names while typing
- Type `help` to get the list of all available commands
- Control names in `--set-ctrl` are case-sensitive and must match exactly as shown in `--all` output
- For controls with spaces in their names, use backslash to escape (e.g., `Vertical\ Flip=1`)
