cmake_minimum_required(VERSION 3.2)
project(libevserver)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
set(CMAKE_INCLUDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_INCLUDE_PATH})

find_package(LibEV REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -g -std=gnu99")

include_directories(include/)

set(HEADER_FILES
        include/common.h
        include/util.h
        include/evsrv_manager.h
        include/evsrv.h
        include/evsrv_conn.h
)

set(SOURCE_FILES
        src/common.c
        src/evsrv_manager.c
        src/evsrv.c
        src/evsrv_conn.c
)

add_library(evserver ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(evserver ev)

add_subdirectory(demo/)
add_subdirectory(ex/)