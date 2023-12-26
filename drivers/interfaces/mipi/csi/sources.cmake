# TODO: Modify to CONFIG_SOC_CSI_SUPPORTED
if(CONFIG_IDF_TARGET_ESP32P4)
    list(APPEND srcs "drivers/interfaces/mipi/csi/${IDF_TARGET}/csi.c")

    list(APPEND include_dirs "drivers/interfaces/mipi/csi/include")
endif()
