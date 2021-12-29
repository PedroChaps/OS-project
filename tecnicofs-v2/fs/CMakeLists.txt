cmake_minimum_required(VERSION 3.19)
project(fs C)

set(CMAKE_C_STANDARD 11)
set(GCC_COVERAGE_COMPILE_FLAGS "-lm")


include_directories(.)

add_executable(fs
        config.h
        operations.c
        operations.h
        state.c
        state.h
        tests/copy_to_external_indirect_blocks.c)

target_link_libraries(fs PRIVATE m)