project(irc)

file(GLOB IRC_HEADERS
    "*.h"
)

file(GLOB IRC_SOURCES
    "*.c"
    "../gameshared/q_*.c"
    "../qalgo/*.c"
)

qf_add_library(irc SHARED ${IRC_SOURCES} ${IRC_HEADERS})
qf_set_output_dir(irc libs)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    target_link_libraries(irc "ws2_32.lib")
endif()