# Project CMakeLists.txt for the registry-consumer regression test.
#
# Purpose: build esp_ipa the way an external component-registry consumer does,
# i.e. under its namespaced name `espressif__esp_ipa` (see main/idf_component.yml).
# The in-tree test apps consume it by the bare short name `esp_ipa`, which masks
# prebuilt PRIV_REQUIRES resolution bugs that only surface for namespaced
# consumers. This app fails at CMake configure if that regression returns.
cmake_minimum_required(VERSION 3.16)

# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
set(COMPONENTS main)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(esp_ipa_registry_consumer)
