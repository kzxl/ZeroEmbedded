cmake_minimum_required(VERSION 3.15)
project(zero_core_c C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(zero_core_c STATIC
    src/assert.c
)

target_include_directories(zero_core_c PUBLIC
    include
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
