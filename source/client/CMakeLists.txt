project(qfusion)

file(GLOB CLIENT_HEADERS
    "*.h"
)

file(GLOB CLIENT_COMMON_SOURCES
    "../qcommon/*.c"
    "../server/*.c"
    "*.c"
    "../unix/unix_fs.c"
    "../unix/unix_lib.c"
    "../unix/unix_net.c"
    "../unix/unix_sys.c"
    "../unix/unix_threads.c"
    "../gameshared/q_*.c"
    "../qalgo/*.c"
    "../matchmaker/*.c"
)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    file(GLOB CLIENT_PLATFORM_SOURCES
        "../mac/SDLMain.m"
        "../mac/mac_input.m"
        "../mac/mac_vid.m"
        "../mac/mac_sys.m"
    )

    set(CLIENT_PLATFORM_LIBRARIES ${SDL_LIBRARY} "-framework IOKit")
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    file(GLOB CLIENT_PLATFORM_SOURCES
        "../unix/keysym2ucs.c"
        "../unix/unix_input.c"
        "../unix/unix_vid.c"
    )
    set(CLIENT_PLATFORM_LIBRARIES "pthread" "dl" "m" "X11" "Xext" "Xi" "Xrandr" "rt")
endif()

qf_add_executable(qfusion ${CLIENT_HEADERS} ${CLIENT_COMMON_SOURCES} ${CLIENT_PLATFORM_SOURCES})
target_link_libraries(qfusion cin ${CURL_LIBRARY} ${ZLIB_LIBRARY} ${CLIENT_PLATFORM_LIBRARIES})
qf_set_output_dir(qfusion "")