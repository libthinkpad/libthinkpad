cmake_minimum_required(VERSION 3.5)

project(libthinkpad)

set(CMAKE_CXX_STANDARD 11)
add_definitions(-pthread)

set(SYSTEMD on)

set(SOURCES
    src/libthinkpad.cpp
    src/libthinkpad.h
)

add_library(thinkpad SHARED ${SOURCES})
set_property(TARGET thinkpad PROPERTY VERSION "2.4")
set_property(TARGET thinkpad PROPERTY SOVERSION 1)

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

set(CPACK_PACKAGE_VENDOR "Ognjen Galic")
set(CPACK_PACKAGE_VERSION_MAJOR 2)
set(CPACK_PACKAGE_VERSION_MINOR 4)
set(CPACK_SOURCE_PACKAGE_FILE_NAME ${PROJECT_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR})
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES "doc/out;\.git;\.idea;CMakeLists\.txt\.user")

include(CPack)
