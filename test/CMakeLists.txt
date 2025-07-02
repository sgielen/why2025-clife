set(TEST_COMPILER ${CMAKE_C_COMPILER})
if(NOT TEST_COMPILER OR TEST_COMPILER STREQUAL "")
    find_program(TEST_COMPILER riscv32-esp-elf-gcc REQUIRED)
endif()

message(STATUS "Building test ELFs with compiler: ${TEST_COMPILER}")

set(TEST_ELF_DIR ${CMAKE_BINARY_DIR}/test_elfs)
file(MAKE_DIRECTORY ${TEST_ELF_DIR})

function(build_test_elf name source)
    add_custom_command(
        OUTPUT ${TEST_ELF_DIR}/${name}.elf
        COMMAND ${TEST_COMPILER}
	    -march=rv32imafc_zicsr_zifencei
            -mabi=ilp32f
	    -ffunction-sections
            -fdata-sections
	    -nostartfiles
	    -fno-builtin
	    -fno-builtin-function
            -Og
            -fstrict-volatile-bitfields
            -fno-jump-tables
            -fno-tree-switch-conversion
            -nostdlib
            -fPIC
            -shared
            -fvisibility=hidden
	    -Wl,--gc-sections
	    -e main
            -o ${TEST_ELF_DIR}/${name}.elf
            ${CMAKE_CURRENT_SOURCE_DIR}/${source}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${source}
        COMMENT "Building test ELF: ${name}"
        VERBATIM
    )
    
    add_custom_target(${name}_elf ALL DEPENDS ${TEST_ELF_DIR}/${name}.elf)
endfunction()

build_test_elf(test_basic_a test_basic_a.c)
build_test_elf(test_basic_b test_basic_b.c)
