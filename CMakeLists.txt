cmake_minimum_required(VERSION 3.15)
project(ZeroEmbedded VERSION 0.2.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(ZERO_BUILD_TESTS "Build ZeroEmbedded unit & hardened tests" ON)
option(ZERO_BUILD_BENCHMARKS "Build ZeroEmbedded benchmark suite" ON)
option(ZERO_BUILD_EXAMPLES "Build ZeroEmbedded PoC & host simulator examples" ON)

# 1. Core C Foundation Tier
add_subdirectory(core-c)

# 2. Hardened Tests & CTest Integration
if(ZERO_BUILD_TESTS)
    enable_testing()

    add_executable(test_core_memory tests/test_core_memory.c)
    target_link_libraries(test_core_memory PRIVATE Zero::core_c)
    add_test(NAME CoreMemoryTests COMMAND test_core_memory)

    add_executable(test_hal_and_tasklet tests/test_hal_and_tasklet.c)
    target_link_libraries(test_hal_and_tasklet PRIVATE Zero::core_c)
    add_test(NAME HalAndTaskletTests COMMAND test_hal_and_tasklet)

    add_executable(test_advanced tests/test_advanced.c)
    target_link_libraries(test_advanced PRIVATE Zero::core_c)
    add_test(NAME AdvancedFeatureTests COMMAND test_advanced)

    add_executable(test_primitives tests/test_primitives.c)
    target_link_libraries(test_primitives PRIVATE Zero::core_c)
    add_test(NAME ModernPrimitivesTests COMMAND test_primitives)
endif()

# 3. Comprehensive Performance Benchmark Suite
if(ZERO_BUILD_BENCHMARKS)
    add_executable(bench_suite benchmarks/bench_suite.c)
    target_link_libraries(bench_suite PRIVATE Zero::core_c)

    add_executable(bench_zero_cost benchmarks/bench_zero_cost.c)
    target_link_libraries(bench_zero_cost PRIVATE Zero::core_c)
endif()

# 4. Examples & Demonstrations
if(ZERO_BUILD_EXAMPLES)
    add_executable(example_stm32_poc examples/stm32_poc/main.c)
    target_link_libraries(example_stm32_poc PRIVATE Zero::core_c)

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/host_sim/main.c")
        add_executable(example_host_sim examples/host_sim/main.c)
        target_link_libraries(example_host_sim PRIVATE Zero::core_c)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/interactive_demo/main.c")
        add_executable(zero_device_demo examples/interactive_demo/main.c)
        target_link_libraries(zero_device_demo PRIVATE Zero::core_c)
    endif()
endif()
