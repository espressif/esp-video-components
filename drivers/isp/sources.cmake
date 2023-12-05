if(EXISTS "${COMPONENT_DIR}/drivers/isp/${IDF_TARGET}")
    list(APPEND srcs "drivers/isp/${IDF_TARGET}/camera_isp.c")
    list(APPEND include_dirs "drivers/isp/include")
endif()
