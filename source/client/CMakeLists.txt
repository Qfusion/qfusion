project(qfusion)

file(GLOB CLIENT_HEADERS
    "*.h"
)

file(GLOB CLIENT_SOURCES
    "../qcommon/*.c"
    "../server/*.c"
    "*.c"
    "../unix/unix_input.c"
    "../unix/keysym2ucs.c"
    "../unix/unix_fs.c"
    "../unix/unix_lib.c"
    "../unix/unix_net.c"
    "../unix/unix_sys.c"
    "../unix/unix_vid.c"
    "../unix/unix_threads.c"
    "../gameshared/q_*.c"
    "../qalgo/*.c"
    "../matchmaker/*.c"
)

qf_add_executable(qfusion ${CLIENT_HEADERS} ${CLIENT_SOURCES})
target_link_libraries(qfusion cin ${CURL_LIBRARY} ${ZLIB_LIBRARY} "pthread" "dl" "m" "X11" "Xext" "Xi" "Xrandr" "rt")
qf_set_output_dir(qfusion "")