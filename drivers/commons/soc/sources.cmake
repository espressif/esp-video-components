if(CONFIG_SOC_LCDCAM_SUPPORTED)
    list(APPEND srcs "drivers/commons/soc/${IDF_TARGET}/cam_periph.c")
endif()

list(APPEND include_dirs "drivers/commons/soc/${IDF_TARGET}/include"
                         "drivers/commons/soc/include")
