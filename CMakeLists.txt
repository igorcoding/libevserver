cmake_minimum_required(VERSION 3.2)
project(libevserver)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
set(CMAKE_INCLUDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_INCLUDE_PATH})

find_package(LibEV REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")

include_directories(include/)

set(HEADER_FILES
        include/evserver.h
        include/evsrv_sockaddr.h
        include/util.h
        include/platform.h
)

set(SOURCE_FILES
        src/evserver.c
        src/util.c
)

add_library(evserver ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(evserver ev)


add_executable(ex_srv ex/ex_srv.c)
target_link_libraries(ex_srv evserver)

add_executable(ex_server ex/ex_server.c)
target_link_libraries(ex_server evserver)

add_executable(tcp_echo_server_simple demo/tcp_echo_server_simple.c)
target_link_libraries(tcp_echo_server_simple evserver)

add_executable(tcp_server_custom_conn demo/tcp_server_custom_conn.c)
target_link_libraries(tcp_server_custom_conn evserver)