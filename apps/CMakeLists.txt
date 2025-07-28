cmake_minimum_required(VERSION 3.16)

set(APP_COMPILER ${CMAKE_C_COMPILER})
if(NOT APP_COMPILER OR APP_COMPILER STREQUAL "")
    find_program(APP_COMPILER riscv32-esp-elf-gcc REQUIRED)
endif()

message(STATUS "Building app ELFs with compiler: ${APP_COMPILER}")

set(APP_COMPILE_FLAGS
    -march=rv32imafc_zicsr_zifencei
    -mabi=ilp32f
    -ffunction-sections
    -fdata-sections
    -nostartfiles
    -fno-builtin
    -fno-builtin-function
    -Og
    -g3
    -fstrict-volatile-bitfields
    -fno-jump-tables
    -fno-tree-switch-conversion
    -nostdlib
    -fPIC
    -shared
    -fvisibility=hidden
)

set(APP_LINK_FLAGS
    -Wl,--strip-debug
    -Wl,--gc-sections
    -e main
)

set(APP_ELF_DIR ${CMAKE_BINARY_DIR}/app_elfs)
file(MAKE_DIRECTORY ${APP_ELF_DIR})

function(build_app app_name)
    cmake_parse_arguments(APP "" "" "SOURCES" ${ARGN})
    
    if(NOT APP_SOURCES)
        message(FATAL_ERROR "build_app: SOURCES must be specified for ${app_name}")
    endif()
    
    set(APP_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/)
    
    set(ABSOLUTE_SOURCES "")
    foreach(SOURCE ${APP_SOURCES})
        if(IS_ABSOLUTE ${SOURCE})
            list(APPEND ABSOLUTE_SOURCES ${SOURCE})
        else()
            list(APPEND ABSOLUTE_SOURCES ${APP_SOURCE_DIR}/${SOURCE})
        endif()
    endforeach()

    add_custom_command(
        OUTPUT ${APP_ELF_DIR}/${app_name}.elf
        COMMAND ${APP_COMPILER}
            ${APP_COMPILE_FLAGS}
            ${APP_LINK_FLAGS}
            -isystem${CMAKE_SOURCE_DIR}/include
            -I${CMAKE_SOURCE_DIR}/include/badgevms
            -o ${APP_ELF_DIR}/${app_name}.elf
            ${ABSOLUTE_SOURCES}
        DEPENDS ${ABSOLUTE_SOURCES}
        COMMENT "Building app ELF: ${app_name}"
        VERBATIM
    )

    add_custom_target(${app_name}_elf ALL DEPENDS ${APP_ELF_DIR}/${app_name}.elf)
    set_property(GLOBAL APPEND PROPERTY APP_ELFS ${APP_ELF_DIR}/${app_name}.elf)
endfunction()

build_app(bench_basic_a
    SOURCES 
    bench_basic_a/bench_basic_a.c
)

build_app(bench_basic_b
    SOURCES
    bench_basic_b/bench_basic_b.c
)

build_app(framebuffer_test
    SOURCES
    framebuffer_test/framebuffer_test_a.c
)

build_app(hardware_test
    SOURCES
    hardware_test/thirdparty/microui.c
    hardware_test/test_badge.c
)

build_app(sdl_test
    SOURCES
    sdl_test/sdl_test.c
)
