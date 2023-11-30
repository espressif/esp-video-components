if(CONFIG_CAMERA_OV5640)
    set(srcs "ov5640.c")
endif()

idf_component_register(SRCS ${srcs}
                       PRIV_INCLUDE_DIRS "private_include"
                       PRIV_REQUIRES "esp-video"
                       )

if(CONFIG_CAMERA_OV5640)
    include(package_manager)
    cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
endif()