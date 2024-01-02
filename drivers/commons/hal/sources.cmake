if(CONFIG_DVP_ENABLE)
    list(APPEND srcs "drivers/commons/hal/${IDF_TARGET}/cam_hal.c")
endif()

if(CONFIG_MIPI_CSI_ENABLE)
    list(APPEND srcs "drivers/commons/hal/${IDF_TARGET}/mipi_csi_hal.c")
endif()

list(APPEND include_dirs "drivers/commons/hal/include")

if(EXISTS "${COMPONENT_DIR}/drivers/commons/hal/${IDF_TARGET}/include")
    list(APPEND include_dirs "drivers/commons/hal/${IDF_TARGET}/include")
endif()
