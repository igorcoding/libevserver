include_directories(../include/)

file( GLOB APP_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.c )
foreach( f ${APP_SOURCES} )
    string( REPLACE ".c" "" app ${f} )
    add_executable( ${app} ${f} )
    target_link_libraries( ${app} evserver )
endforeach( f ${APP_SOURCES} )

add_executable(ev_cpp ex_cpp.cpp)
target_link_libraries( ev_cpp evserver )
