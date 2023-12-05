# (Automatically converted from project Makefile by convert_to_cmake.py.)

# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS ../..
                        ../../drivers/sccb
                        ../../drivers/isp
                        ../../sensors)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(pipeline_test)
