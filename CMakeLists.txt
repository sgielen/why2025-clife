# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

add_subdirectory(test)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(why2025_firmware)

idf_build_get_property(build_dir BUILD_DIR)
if(TARGET test_basic_a_elf AND TARGET test_basic_b_elf)
    add_dependencies(__idf_build_target test_basic_a_elf test_basic_b_elf)
endif()
