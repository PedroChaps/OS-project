cmake_minimum_required(VERSION 3.19)
project(fs C)

set(CMAKE_C_STANDARD 11)
set(GCC_COVERAGE_COMPILE_FLAGS "-lm")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

include_directories(.)

add_executable(fs
        config.h
        operations.c
        operations.h
        state.c
        state.h
        tests/TR_write_and_read.c)

target_link_libraries(fs PRIVATE m)