# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include(ExternalProject)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(why2025_firmware)

ExternalProject_Add(host_tests
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/host_tests
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/host_tests
    BUILD_COMMAND ninja
    INSTALL_COMMAND ninja test
    CONFIGURE_HANDLED_BY_BUILD false
    BUILD_ALWAYS 1
)
