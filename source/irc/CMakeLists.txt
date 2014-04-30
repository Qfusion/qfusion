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