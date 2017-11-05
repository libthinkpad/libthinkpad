cmake_minimum_required(VERSION 3.5)
project(libthinkpad)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -pthread)

set(SYSTEMD on)

add_subdirectory(src)
