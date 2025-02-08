cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-rtti -g0 -O3")
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)

add_library(lz4 SHARED
        lz4.c
        lz4hc.c
        xxhash.c
        lz4frame.c
        lz4file.c
        threadpool.c
        timefn.c
        util.c
        lz4io.c)
