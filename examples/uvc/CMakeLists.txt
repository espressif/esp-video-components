cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/examples/p4_sample/csi_camera_usb/components/usb_device_uvc"
                        "$ENV{IDF_PATH}/examples/p4_sample/csi_camera_usb/components/tinyusb")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(uvc)
