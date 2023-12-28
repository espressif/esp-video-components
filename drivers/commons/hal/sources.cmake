if(CONFIG_SOC_LCDCAM_SUPPORTED)
    list(APPEND srcs "drivers/commons/hal/${IDF_TARGET}/cam_hal.c")
endif()

# TODO: Modify to CONFIG_SOC_CSI_SUPPORTED
if(CONFIG_IDF_TARGET_ESP32P4)
    list(APPEND srcs "drivers/commons/hal/${IDF_TARGET}/mipi_csi_hal.c")
endif()

list(APPEND include_dirs "drivers/commons/hal/${IDF_TARGET}/include"
                         "drivers/commons/hal/include")
