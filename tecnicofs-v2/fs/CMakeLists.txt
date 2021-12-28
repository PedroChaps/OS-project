cmake_minimum_required(VERSION 3.21)
project(fs C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(fs
        config.h
        operations.c
        operations.h
        state.c
        state.h)
