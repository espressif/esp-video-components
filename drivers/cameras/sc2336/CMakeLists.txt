if(CONFIG_CAMERA_SC2336)
    set(srcs "sc2336.c")
endif()

idf_component_register(SRCS ${srcs}
                       PRIV_INCLUDE_DIRS "private_include"
                       PRIV_REQUIRES "esp-video-components" "driver"
                       )

if(CONFIG_CAMERA_SC2336_AUTO_DETECT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u sc2336_csi_detect")
endif()

if(CONFIG_CAMERA_SC2336)
    include(package_manager)
    cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
endif()
