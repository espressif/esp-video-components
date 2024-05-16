## 0.1.1

- Modify API `esp_video_create` and put camera device management from video core to hardware device driver

## 0.1.0

- Inititial version for esp_video component

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
