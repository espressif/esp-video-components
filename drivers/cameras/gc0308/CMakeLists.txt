if(CONFIG_CAMERA_GC0308)
    set(srcs "gc0308.c")
endif()

idf_component_register(SRCS ${srcs}
                       PRIV_INCLUDE_DIRS "private_include"
                       PRIV_REQUIRES "esp-video-components" "driver"
                       )

if(CONFIG_CAMERA_GC0308_AUTO_DETECT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u gc0308_dvp_detect")
endif()

if(CONFIG_CAMERA_GC0308)
    include(package_manager)
    cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
endif()
