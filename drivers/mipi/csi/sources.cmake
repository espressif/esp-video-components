if(EXISTS "${COMPONENT_DIR}/drivers/mipi/csi/${IDF_TARGET}")
    list(APPEND srcs "drivers/mipi/csi/${IDF_TARGET}/csi.c"
                     "drivers/mipi/csi/${IDF_TARGET}/csi_hal.c"
                     "drivers/mipi/csi/${IDF_TARGET}/csi_video.c")
endif()