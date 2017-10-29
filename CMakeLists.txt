cmake_minimum_required(VERSION 3.5)
project(libthinkdock)

set(CMAKE_CXX_STANDARD 14)

set(SYSTEMD on)

add_subdirectory(lib)
add_subdirectory(exe)