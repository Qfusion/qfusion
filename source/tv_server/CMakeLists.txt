project(qfusiontv_server)

file(GLOB TV_SERVER_HEADERS
    "*.h"
)

file(GLOB TV_SERVER_SOURCES
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
    "../qcommon/svnrev.c"
    "../qcommon/snap_demos.c"
    "../qcommon/snap_read.c"
    "../qcommon/snap_write.c"
    "../qcommon/wswcurl.c"
    "../qcommon/threads.c"
    "../qcommon/steam.c"
    "*.c"
    "../null/cl_null.c"
    "../null/ascript_null.c"
    "../null/mm_null.c"
    "../unix/unix_fs.c"
    "../unix/unix_net.c"
    "../unix/unix_sys.c"
    "../unix/unix_lib.c"
    "../unix/unix_threads.c"
    "../gameshared/q_*.c"
    "../qalgo/*.c"
    "tv_module/*.c"
)

qf_add_executable(qfusiontv_server ${TV_SERVER_HEADERS} ${TV_SERVER_SOURCES})
target_link_libraries(qfusiontv_server cin ${CURL_LIBRARIES} ${ZLIB_LIBRARY} "pthread" "dl" "m")
qf_set_output_dir(qfusiontv_server "")

set_target_properties(qfusiontv_server PROPERTIES COMPILE_DEFINITIONS "DEDICATED_ONLY;TV_SERVER_ONLY;TV_MODULE_HARD_LINKED")