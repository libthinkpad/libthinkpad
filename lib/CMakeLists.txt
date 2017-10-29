
set(SOURCES libthinkdock.cpp libthinkdock.h)
add_library(thinkdock SHARED ${SOURCES})

configure_file(config.h.in config.h)

set(THINKDOCK_LINK X11 Xrandr)

if(DEFINED SYSTEMD)
    set(THINKDOCK_LINK ${THINKDOCK_LINK} systemd)
endif(DEFINED SYSTEMD)

target_include_directories (thinkdock PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(thinkdock PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(thinkdock ${THINKDOCK_LINK})