if(CONFIG_DVP_ENABLE)
    list(APPEND srcs "drivers/interfaces/dvp/dvp.c")
    if(EXISTS "${COMPONENT_DIR}/drivers/interfaces/dvp/${IDF_TARGET}/dvp.c")
        list(APPEND srcs "drivers/interfaces/dvp/${IDF_TARGET}/dvp.c")
    endif()

    list(APPEND include_dirs "drivers/interfaces/dvp/include")
endif()