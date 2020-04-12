project(ref_gl)

file(GLOB REF_GL_HEADERS
    "*.h"
	"../gameshared/q_*.h"
	"../gameshared/anorms.h"
	"../gameshared/config.h"
	"../qcommon/bsp.h"
	"../qcommon/patch.h"
	"../qcommon/iqm.h"
	"../qcommon/qfiles.h"
	"../qalgo/*.h"
)

file(GLOB REF_GL_COMMON_SOURCES
    "*.c"
    "../gameshared/q_*.c"
    "../qalgo/*.c"
    "../qcommon/bsp.c"
    "../qcommon/patch.c"
)

file(GLOB NANOSVG_HEADERS
    "${NANOSVG_INCLUDE_DIR}/*.h"
)

file(GLOB STB_HEADERS
    "${STB_INCLUDE_DIR}/stb_image.h"
    "${STB_INCLUDE_DIR}/stb_image_write.h"
)

if (USE_SDL2)
   	file(GLOB REF_GL_PLATFORM_SOURCES
       	    "../sdl/sdl_glw.c"
            "../sdl/sdl_glw_icon.c"
       	    "../sdl/sdl_qgl.c"
        )

    set(REF_GL_PLATFORM_LIBRARIES ${SDL2_LIBRARY})
else()
    if (WIN32)
        file(GLOB REF_GL_PLATFORM_SOURCES
            "../win32/win_glw.c"
            "../win32/win_qgl.c"
        )

        set(REF_GL_PLATFORM_LIBRARIES "")
    endif()
endif()

source_group("nanosvg\\Header Files" FILES ${NANOSVG_HEADERS})

add_library(ref_gl SHARED ${REF_GL_HEADERS} ${REF_GL_COMMON_SOURCES} ${REF_GL_PLATFORM_SOURCES} ${NANOSVG_HEADERS} ${STB_HEADERS})

target_include_directories(ref_gl PRIVATE ${OPENGL_INCLUDE_DIR} ${STB_INCLUDE_DIR} ${NANOSVG_INCLUDE_DIR})

if (USE_SDL2)
    target_include_directories(ref_gl PRIVATE ${SDL2_INCLUDE_DIR})
endif()

target_link_libraries(ref_gl PRIVATE ${REF_GL_PLATFORM_LIBRARIES})

qf_set_output_dir(ref_gl libs)
