idf_component_register(
    SRCS "why2025_firmware.c" "khash.h" ${CMAKE_CURRENT_BINARY_DIR}/generated_symbols.c
    INCLUDE_DIRS "." 
    EMBED_FILES "${CMAKE_BINARY_DIR}/test_elfs/test_basic_a.elf" "${CMAKE_BINARY_DIR}/test_elfs/test_basic_b.elf" "${CMAKE_BINARY_DIR}/test_elfs/test_shell.elf"
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated_symbols.c
    COMMAND ${PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/generate_symbols.py 
            ${CMAKE_CURRENT_SOURCE_DIR}/symbols.yml 
            ${CMAKE_CURRENT_BINARY_DIR}/generated_symbols.c
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/symbols.yml
            ${CMAKE_CURRENT_SOURCE_DIR}/generate_symbols.py
    COMMENT "Generating symbol table from symbols.yml"
    VERBATIM
)

add_custom_target(generated_symbols 
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/generated_symbols.c
)

add_dependencies(${COMPONENT_LIB} generated_symbols)
target_link_libraries(${COMPONENT_LIB} PRIVATE m g)
