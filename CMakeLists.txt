cmake_minimum_required(VERSION 3.2)
project(libevserver)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
set(CMAKE_INCLUDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_INCLUDE_PATH})

find_package(LibEV REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

include_directories(src/)

set(SOURCE_FILES src/evserver.c
                 src/evserver.h
                 src/util.h
)
add_library(evserver ${SOURCE_FILES})
target_link_libraries(evserver ev)


add_executable(ex_server ex/ex_server.c)
target_link_libraries(ex_server evserver)