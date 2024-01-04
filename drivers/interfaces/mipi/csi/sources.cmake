if(CONFIG_MIPI_CSI_ENABLE)
    list(APPEND srcs "drivers/interfaces/mipi/csi/${IDF_TARGET}/csi.c" "drivers/interfaces/mipi/csi/csi_video.c")

    list(APPEND include_dirs "drivers/interfaces/mipi/csi/include")
endif()
