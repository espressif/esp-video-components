if(CONFIG_CAMERA_SIM)
    set(srcs "sim.c")
endif()

idf_component_register(SRCS ${srcs}
                       PRIV_INCLUDE_DIRS "private_include"
                       PRIV_REQUIRES "esp-video"
                       )

if(CONFIG_CAMERA_SIM)
    include(package_manager)
    cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
endif()