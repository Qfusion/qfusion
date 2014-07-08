project(${QFUSION_SERVER_NAME})

include_directories(${ZLIB_INCLUDE_DIR} ${CURL_INCLUDE_DIR})

file(GLOB SERVER_HEADERS
    "*.h"
)

file(GLOB SERVER_SOURCES
    "../qcommon/cm_main.c"
    "../qcommon/cm_q3bsp.c"
    "../qcommon/cm_trace.c"
    "../qcommon/bsp.c"
    "../qcommon/patch.c"
    "../qcommon/common.c"
    "../qcommon/files.c"
    "../qcommon/cmd.c"
    "../qcommon/mem.c"
    "../qcommon/net.c"
    "../qcommon/net_chan.c"
    "../qcommon/msg.c"
    "../qcommon/cvar.c"
    "../qcommon/dynvar.c"
    "../qcommon/irc.c"
    "../qcommon/library.c"
    "../qcommon/mlist.c"
    "../qcommon/webdownload.c"
    "../qcommon/svnrev.c"
    "../qcommon/snap_demos.c"
    "../qcommon/snap_write.c"
    "../qcommon/ascript.c"
    "../qcommon/anticheat.c"
    "../qcommon/wswcurl.c"
    "../qcommon/cjson.c"
    "../qcommon/threads.c"
    "../qcommon/steam.c"
    "*.c"
    "../null/cl_null.c"
    "../gameshared/q_*.c"
    "../qalgo/*.c"
    "../matchmaker/mm_*.c"
)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    file(GLOB SERVER_PLATFORM_SOURCES 
        "../win32/win_fs.c"
        "../win32/win_net.c"
        "../win32/win_sys.c"
        "../win32/win_lib.c"
        "../win32/win_threads.c"
        "../win32/conproc.c"
    )

    set(SERVER_PLATFORM_LIBRARIES "ws2_32.lib" "winmm.lib")
    set(SERVER_BINARY_TYPE WIN32)
else()
    file(GLOB SERVER_PLATFORM_SOURCES 
        "../unix/unix_fs.c"
        "../unix/unix_net.c"
        "../unix/unix_sys.c"
        "../unix/unix_lib.c"
        "../unix/unix_threads.c"
    )

    set(SERVER_PLATFORM_LIBRARIES "pthread" "dl" "m")
    set(SERVER_BINARY_TYPE "")
endif()

qf_add_executable(${QFUSION_SERVER_NAME} ${SERVER_BINARY_TYPE} ${SERVER_HEADERS} ${SERVER_SOURCES} ${SERVER_PLATFORM_SOURCES})
target_link_libraries(${QFUSION_SERVER_NAME} cin ${CURL_LIBRARIES} ${ZLIB_LIBRARY} ${SERVER_PLATFORM_LIBRARIES})
qf_set_output_dir(${QFUSION_SERVER_NAME} "")

set_target_properties(${QFUSION_SERVER_NAME} PROPERTIES COMPILE_DEFINITIONS "DEDICATED_ONLY")