project(ref_gl)

include_directories(${JPEG_INCLUDE_DIR} ${PNG_INCLUDE_DIR})

file(GLOB REF_GL_HEADERS
    "*.h"
)

file(GLOB REF_GL_SOURCES
    "*.c"
    "../gameshared/q_*.c"
    "../qalgo/*.c"
    "../qcommon/bsp.c"
    "../qcommon/patch.c"

    "../unix/unix_glw.c"
    "../unix/unix_xpm.c"
    "../unix/unix_qgl.c"
)

qf_add_library(ref_gl SHARED ${REF_GL_HEADERS} ${REF_GL_SOURCES})
target_link_libraries(ref_gl ${JPEG_LIBRARIES} ${PNG_LIBRARIES} "X11" "Xext" "Xinerama" "Xrandr" "Xxf86vm")
qf_set_output_dir(ref_gl libs)