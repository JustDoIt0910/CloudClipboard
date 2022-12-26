cmake_minimum_required(VERSION 2.8)
project(clipserver)

SET(CMAKE_BUILD_TYPE "Debug")
link_directories(${CMAKE_SOURCE_DIR}/muduo/lib)

add_executable(server clipserver.cc codec.h)

target_link_libraries(server muduo_base muduo_net pthread)

target_include_directories(server PUBLIC
            ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/muduo/include
        )