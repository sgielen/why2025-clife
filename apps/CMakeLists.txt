cmake_minimum_required(VERSION 3.16)

get_property(SDK_LIBRARIES GLOBAL PROPERTY SDK_LIBRARIES)
set(SDK_LIB_DIR ${CMAKE_BINARY_DIR}/sdk_staging/lib)
set(SDK_INCLUDE_DIR ${CMAKE_BINARY_DIR}/sdk_staging/include)

set(APP_COMPILE_FLAGS
    -O2
    -fPIC
    -fdata-sections
    -ffunction-sections
    -flto
    -fno-builtin
    -fno-builtin-function
    -fno-jump-tables
    -fno-tree-switch-conversion
    -fstrict-volatile-bitfields
    -fvisibility=hidden
    -g3
    -mabi=ilp32f
    -march=rv32imafc_zicsr_zifencei
    -nostartfiles
    -nostdlib
    -shared
)

set(APP_LINK_FLAGS
    -Wl,--strip-debug
    -Wl,--gc-sections
    -e main
)

set(APP_ELF_DIR ${CMAKE_BINARY_DIR}/app_elfs)
file(MAKE_DIRECTORY ${APP_ELF_DIR})

function(build_app app_name)
    cmake_parse_arguments(APP "" "" "SOURCES;LIBRARIES" ${ARGN})

    if(NOT APP_SOURCES)
        message(FATAL_ERROR "build_app: SOURCES must be specified for ${app_name}")
    endif()

    set(APP_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/)

    # Process sources
    set(ABSOLUTE_SOURCES "")
    foreach(SOURCE ${APP_SOURCES})
        if(IS_ABSOLUTE ${SOURCE})
            list(APPEND ABSOLUTE_SOURCES ${SOURCE})
        else()
            list(APPEND ABSOLUTE_SOURCES ${APP_SOURCE_DIR}/${SOURCE})
        endif()
    endforeach()

    set(LIBRARY_FLAGS "")
    set(LIBRARY_DEPENDENCIES "")
    if(APP_LIBRARIES)
        list(APPEND LIBRARY_FLAGS -L${SDK_LIB_DIR})
        foreach(LIB ${APP_LIBRARIES})
            list(APPEND LIBRARY_FLAGS -l${LIB})
            # This prevents unused symbols from ending up in the final elf
            list(APPEND LIBRARY_FLAGS -Wl,--exclude-libs,lib${LIB}.a)
            list(APPEND LIBRARY_DEPENDENCIES ${SDK_LIB_DIR}/lib${LIB}.a)
        endforeach()
    endif()

    add_custom_command(
        OUTPUT ${APP_ELF_DIR}/${app_name}.elf
        COMMAND ${CMAKE_C_COMPILER}
            ${APP_COMPILE_FLAGS}
            ${APP_LINK_FLAGS}
            -isystem${SDK_INCLUDE_DIR}
            -o ${APP_ELF_DIR}/${app_name}.elf
            ${ABSOLUTE_SOURCES}
            ${LIBRARY_FLAGS}
        DEPENDS ${ABSOLUTE_SOURCES} ${LIBRARY_DEPENDENCIES}
        COMMENT "Building app ELF: ${app_name}"
        VERBATIM
    )

    add_custom_target(${app_name}_elf ALL DEPENDS ${APP_ELF_DIR}/${app_name}.elf)

    if(APP_LIBRARIES)
        add_dependencies(${app_name}_elf build_sdk)
    endif()

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
    LIBRARIES
    sdl3
)
