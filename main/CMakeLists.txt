idf_component_register(
    SRCS
	 ${CMAKE_CURRENT_BINARY_DIR}/generated_symbols.c
	 "device.c"
	 "drivers/fatfs.c"
	 "drivers/tty.c"
	 "logical_names.c"
	 "memory.c"
	 "pathfuncs.c"
	 "static-buddy.c"
	 "task.c"
	 "thirdparty/dlmalloc.c"
	 "wrapped_funcs.c"
    	 "why2025_firmware.c"
    INCLUDE_DIRS
	 ${CMAKE_CURRENT_SOURCE_DIR}/
	 ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty
    EMBED_FILES
#    	"${CMAKE_BINARY_DIR}/test_elfs/test_basic_a.elf"
#	"${CMAKE_BINARY_DIR}/test_elfs/test_basic_b.elf"
#	"${CMAKE_BINARY_DIR}/test_elfs/test_basic_c.elf"
#	"${CMAKE_BINARY_DIR}/test_elfs/test_shell.elf"
	"${CMAKE_BINARY_DIR}/test_elfs/bench_basic_a.elf"
	"${CMAKE_BINARY_DIR}/test_elfs/bench_basic_b.elf"
     PRIV_REQUIRES
        "esp_mm"
	"esp_psram"
	"vfs"
	"fatfs"
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
target_link_libraries(${COMPONENT_LIB} PRIVATE m g 
	"-Wl,--wrap=esp_panic_handler"
	"-Wl,--wrap=xt_unhandled_exception"
	)
target_compile_definitions(${COMPONENT_LIB} PRIVATE
	USE_DL_PREFIX
	HAVE_MORECORE=1
	HAVE_MMAP=0
	HAVE_MREMAP=0
	MORECORE=why_sbrk
	MORECORE_CONTIGUOUS=1
	NO_MALLINFO
	malloc_getpagesize=0x10000
)
target_compile_options(${COMPONENT_LIB} PRIVATE
	-Wno-empty-body	
)

set(image ../fatfs_image)
fatfs_create_spiflash_image(storage ${image} FLASH_IN_PROJECT PRESERVE_TIME)
