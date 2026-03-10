# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

if (EXISTS "$ENV{IDF_PATH}/tools/unit-test-app/components")
    set(EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/tools/unit-test-app/components")
elseif (EXISTS "$ENV{IDF_PATH}/tools/test_apps/components")
    set(EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/tools/test_apps/components")
else()
    message(FATAL_ERROR "Could not find unit-test-app or test_apps components")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# (Not part of the boilerplate)
# This example uses extra components for the following -
# 1. Check sd card pins info
set(SD_CARD_COMPONENT_DIR "$ENV{IDF_PATH}/examples/storage/sd_card/sdmmc/components/sd_card")
if (EXISTS "${SD_CARD_COMPONENT_DIR}")
    list(APPEND EXTRA_COMPONENT_DIRS "${SD_CARD_COMPONENT_DIR}")
else()
    message(FATAL_ERROR "Could not find sd_card component at ${SD_CARD_COMPONENT_DIR}")
endif()

project(test_apps_posix)
