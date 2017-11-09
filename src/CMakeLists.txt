
set(SOURCES libthinkpad.cpp libthinkpad.h)
add_library(thinkpad SHARED ${SOURCES})

configure_file(config.h.in config.h)

set(THINKPAD_LINK X11 Xrandr udev m)

if(DEFINED SYSTEMD)
    set(THINKPAD_LINK ${THINKPAD_LINK} systemd)
endif(DEFINED SYSTEMD)

target_include_directories (thinkpad PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories (thinkpad PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(thinkpad ${THINKPAD_LINK})

set_target_properties(thinkpad PROPERTIES PUBLIC_HEADER "libthinkpad.h")

install(TARGETS thinkpad
        LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include
)
