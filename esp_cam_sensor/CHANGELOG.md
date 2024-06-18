## 0.5.0

- Added support for GC0308 DVP camera sensor driver
- Added support for SC101IOT DVP camera sensor driver

## 0.4.0

- Added support for SC202CS MIPI camera sensor driver
- Added support for SC2336 RAW8 format with 800x800、1280x720、1920x1080 resolution
- Changed the test/apps/detect demo to use the formal esp_sccb_intr component
- Changed the SC2336 to use RAW8 format by default

## 0.3.2

- Added support for OV5645 RGB565 and YUV420 on 960p resolution
- Removed the override_path of esp_sccb_intf in component dependencies

## 0.3.1

- Added support for OV5645 640x480、1920x1080、2592x1944 resolution
- Fix OV2640 compile issue

## 0.3.0

- Added support for OV2640 dvp camera sensor driver

## 0.2.2

- Fixed wrong initialization sequence for OV5647

## 0.2.1

- Add line_sync_en parameter to mipi_info struct
- Use default format as the current format in sensor_detect

## 0.2.0

- Add support for OV5645 camera sensor driver
- Add support for OV5647 camera sensor MIPI driver
- Add sensor_port parameter in SC2336 DETECT_FN definition
- Fix SC2336 format_array for 1080p+30fps support

## 0.1.0

- Initial version for esp_cam_sensor component
- Add support for SC2336 camera sensor driver
