
set(SOURCES libthinkdock.cpp libthinkdock.h)
add_library(thinkdock SHARED ${SOURCES})
target_include_directories (thinkdock PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(thinkdock X11 Xrandr)