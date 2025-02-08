cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-rtti -g0 -O3")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-rtti -g0 -O3")
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/liblz4)

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

add_subdirectory(liblz4)

add_library(lzma SHARED IMPORTED)
set_target_properties(
        lzma
        PROPERTIES IMPORTED_LOCATION
        "${CMAKE_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/liblzma.so"
)

target_include_directories(abik PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/liblz4)

target_link_libraries(abik PRIVATE log)
target_link_libraries(abik PRIVATE lz4)
target_link_libraries(abik PRIVATE z)
target_link_libraries(abik PRIVATE lzma)