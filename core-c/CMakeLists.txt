cmake_minimum_required(VERSION 3.15)
project(zero_core_c C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(zero_core_c STATIC
    src/assert.c
    src/pool.c
    src/arena.c
    src/buffer.c
    src/spsc.c
    src/zerowire.c
    src/timer.c
    src/gpio.c
    src/uart.c
    src/tasklet.c
    src/rtos_baremetal.c
    src/power.c
    src/dispatcher.c
    src/watchdog.c
    src/fsm.c
    src/filter.c
    src/nvs.c
    src/crash_dump.c
    src/ota.c
    src/log.c
    src/sha256.c
    src/chacha20.c
)

add_library(Zero::core_c ALIAS zero_core_c)

target_include_directories(zero_core_c PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Compiler optimization warnings & flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(zero_core_c PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -O3
    )
elseif(MSVC)
    target_compile_options(zero_core_c PRIVATE
        /W4
        /WX
        /O2
    )
endif()

# ==============================================================================
# Installation & CMake Export Configuration
# ==============================================================================
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS zero_core_c
    EXPORT ZeroEmbeddedTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT ZeroEmbeddedTargets
    FILE ZeroEmbeddedTargets.cmake
    NAMESPACE Zero::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ZeroEmbedded
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ZeroEmbeddedConfigVersion.cmake"
    VERSION 0.7.0
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ZeroEmbeddedConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ZeroEmbeddedConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ZeroEmbedded
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ZeroEmbeddedConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ZeroEmbeddedConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ZeroEmbedded
)
