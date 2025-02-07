cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-rtti -g0 -O3 ")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-rtti -g0 -O3")
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/liblz4)

add_library(lz4 STATIC
        liblz4/lz4.c
        liblz4/lz4hc.c
        liblz4/xxhash.c
        liblz4/lz4frame.c
        liblz4/lz4file.c
        liblz4/threadpool.c
        liblz4/timefn.c
        liblz4/util.c
        liblz4/lz4io.c)

add_library(abik SHARED
        Jni.cc
        Log.cc
        Tools.cc
        unpackbootimg/utils.cc
        unpackbootimg/bootimg.cc
        unpackbootimg/vendorbootimg.cc
        mkbootimg/utils.cc
        mkbootimg/bootimg.cc
        mkbootimg/vendorbootimg.cc
)

set_target_properties(lz4 PROPERTIES C_VISIBILITY_PRESET default CXX_VISIBILITY_PRESET default)

set(HEADER_FILES
        liblz4/lz4.h
)

target_include_directories(abik PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/liblz4)

target_link_libraries(abik PRIVATE log)
target_link_libraries(abik PRIVATE lz4)
target_link_libraries(abik PRIVATE z)