# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include(ExternalProject)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(why2025_firmware)
add_subdirectory(apps)

ExternalProject_Add(host_tests
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/host_tests
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/host_tests
    BUILD_COMMAND ninja
    INSTALL_COMMAND ninja test
    CONFIGURE_HANDLED_BY_BUILD false
    BUILD_ALWAYS 1
)

set(FATFS_STAGING_DIR ${CMAKE_BINARY_DIR}/fatfs_staging)
set(FATFS_STATIC_DIR ${CMAKE_SOURCE_DIR}/fatfs_image)
file(MAKE_DIRECTORY ${FATFS_STAGING_DIR})

function(prepare_fatfs_image)
    if(EXISTS ${FATFS_STATIC_DIR})
        file(COPY ${FATFS_STATIC_DIR}/ DESTINATION ${FATFS_STAGING_DIR})
    endif()

    get_property(ALL_APP_ELFS GLOBAL PROPERTY APP_ELFS)

    foreach(APP_ELF ${ALL_APP_ELFS})
        get_filename_component(ELF_NAME ${APP_ELF} NAME)
        add_custom_command(
            OUTPUT ${FATFS_STAGING_DIR}/${ELF_NAME}
            COMMAND ${CMAKE_COMMAND} -E copy ${APP_ELF} ${FATFS_STAGING_DIR}/${ELF_NAME}
            DEPENDS ${APP_ELF}
            COMMENT "Copying ${ELF_NAME} to fatfs staging"
        )
        list(APPEND FATFS_STAGED_FILES ${FATFS_STAGING_DIR}/${ELF_NAME})
    endforeach()

    add_custom_target(prepare_fatfs_staging ALL DEPENDS ${FATFS_STAGED_FILES})
endfunction()

prepare_fatfs_image()

fatfs_create_spiflash_image(storage ${FATFS_STAGING_DIR} FLASH_IN_PROJECT PRESERVE_TIME)
add_dependencies(fatfs_storage_bin prepare_fatfs_staging)
