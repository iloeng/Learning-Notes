cmake_minimum_required(VERSION 3.18)
project(src C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(redis
        adlist.c
        adlist.h
        ae.c
        ae.h
        anet.c
        anet.h
        dict.c
        dict.h
        picol.c
        picol.h
        redis.c
        sds.c
        sds.h)
