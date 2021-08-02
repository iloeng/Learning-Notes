cmake_minimum_required(VERSION 3.18)
project(redis-0-091 C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(redis-server
        adlist.c
        adlist.h
        ae.c
        ae.h
        anet.c
        anet.h
        dict.c
        dict.h
        lzf_c.c
        lzf_d.c
        redis.c
        sds.c
        sds.h
        zmalloc.c
        zmalloc.h)

add_executable(redis-benchmark
        ae.c
        ae.h
        anet.c
        anet.h
        benchmark.c
        sds.c
        sds.h
        adlist.c
        adlist.h
        zmalloc.c
        zmalloc.h
)

add_executable(redis-cli
        anet.c
        anet.h
        sds.c
        sds.h
        adlist.c
        adlist.h
        zmalloc.c
        zmalloc.h
        redis-cli.c
)