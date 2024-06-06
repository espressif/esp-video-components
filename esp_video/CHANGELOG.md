## 0.2.0

- Support V4L2 `V4L2_MEMORY_USERPTR`
- Fix CSI video device stop issue
- Fix I2C clock frequency can't be used when input I2C handle

## 0.1.2

- Update the API and macro to support ESP-IDF new version

## 0.1.1

- Modify API `esp_video_create` and put camera device management from video core to hardware device driver

## 0.1.0

- Initial version for esp_video component

### Enhancements

- MIPI CSI video device
- Virtual file system to control video device
- Linux V4L2 commands mainly includes(actually not support commands' full features):

```
    VIDIOC_QUERYCAP

    VIDIOC_STREAMON
    VIDIOC_STREAMOFF

    VIDIOC_ENUM_FMT
    VIDIOC_G_FMT
    VIDIOC_S_FMT

    VIDIOC_REQBUFS
    VIDIOC_QUERYBUF
    VIDIOC_QBUF
    VIDIOC_DQBUF

    VIDIOC_G_EXT_CTRLS
    VIDIOC_S_EXT_CTRLS
    VIDIOC_QUERY_EXT_CTRL
```
