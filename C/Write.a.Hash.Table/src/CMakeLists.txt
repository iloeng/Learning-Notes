# cmake_minimum_required(VERSION <specify CMake version here>)
project(src C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(src
        hash_table.h
        hash_table.c)
