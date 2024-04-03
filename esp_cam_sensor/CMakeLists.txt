set(srcs "src/esp_cam_sensor.c")

set(include_dirs "include")
set(requires "driver" "esp_sccb_intf")

if(CONFIG_CAMERA_SC2336)
    list(APPEND srcs "sensors/sc2336/sc2336.c")
    list(APPEND include_dirs "sensors/sc2336/include")
    list(APPEND priv_include_dirs "sensors/sc2336/private_include")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_INCLUDE_DIRS ${priv_include_dirs}
                       REQUIRES ${requires}
                       LDFRAGMENTS linker.lf)

if(CONFIG_CAMERA_SC2336_AUTO_DETECT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u sc2336_detect")
endif()

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})