cmake_minimum_required(VERSION 3.15)
project(hash_table C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(src
        hash_table.h
        hash_table.c
        main.c prime.c prime.h)
