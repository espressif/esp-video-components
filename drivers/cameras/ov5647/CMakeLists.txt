if(CONFIG_CAMERA_OV5647)
    set(srcs "ov5647.c")
endif()

idf_component_register(SRCS ${srcs}
                       PRIV_INCLUDE_DIRS "private_include"
                       PRIV_REQUIRES "esp-video" "driver"
                       )

if(CONFIG_CAMERA_OV5647_AUTO_DETECT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u ov5647_csi_detect")
endif()

if(CONFIG_CAMERA_OV5647)
    include(package_manager)
    cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
endif()
