if(CONFIG_DVP_ENABLE)
    list(APPEND srcs "drivers/interfaces/dvp/dvp.c" "drivers/interfaces/dvp/dvp_video.c")
    if(EXISTS "${COMPONENT_DIR}/drivers/interfaces/dvp/${IDF_TARGET}/dvp.c")
        list(APPEND srcs "drivers/interfaces/dvp/${IDF_TARGET}/dvp.c")
    endif()
endif()

# Althougth CONFIG_DVP_ENABLE is not select, dvp.h also can be included
list(APPEND include_dirs "drivers/interfaces/dvp/include")