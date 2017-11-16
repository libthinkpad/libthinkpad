cmake_minimum_required(VERSION 3.5)

project(libthinkpad)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -pthread)

set(SYSTEMD on)

set(SOURCES
    src/libthinkpad.cpp
    src/libthinkpad.h
)

add_library(thinkpad SHARED ${SOURCES})

configure_file(src/config.h.in config.h)

set(THINKPAD_LINK udev m)

if(DEFINED SYSTEMD)
    set(THINKPAD_LINK ${THINKPAD_LINK} systemd)
endif(DEFINED SYSTEMD)

target_include_directories (thinkpad PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories (thinkpad PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(thinkpad ${THINKPAD_LINK})

set_target_properties(thinkpad PROPERTIES PUBLIC_HEADER "src/libthinkpad.h")

install(TARGETS thinkpad
        LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include
)