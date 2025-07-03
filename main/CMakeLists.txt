idf_component_register(
    SRCS "why2025_firmware.c" "khash.h"
    INCLUDE_DIRS "."
    EMBED_FILES "${CMAKE_BINARY_DIR}/test_elfs/test_basic_a.elf" "${CMAKE_BINARY_DIR}/test_elfs/test_basic_b.elf"
)
