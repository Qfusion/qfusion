project(ref_gl)

include_directories(${JPEG_INCLUDE_DIR} ${PNG_INCLUDE_DIR} ${SDL2_INCLUDE_DIR})

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
	"*.cpp"
	"../gameshared/q_*.c"
	"../qalgo/*.c"
	"../qcommon/bsp.c"
	"../qcommon/patch.c"
	)

file(GLOB REF_GL_PLATFORM_SOURCES
	"../sdl/sdl_glw.c"
	"../sdl/sdl_glw_icon.c"
	"../sdl/sdl_qgl.c"
	)

set(REF_GL_PLATFORM_LIBRARIES ${SDL2_LIBRARY})

add_library(ref_gl SHARED ${REF_GL_HEADERS} ${REF_GL_COMMON_SOURCES} ${REF_GL_PLATFORM_SOURCES})
target_link_libraries(ref_gl PRIVATE ${JPEG_LIBRARIES} ${PNG_LIBRARIES} ${REF_GL_PLATFORM_LIBRARIES})
qf_set_output_dir(ref_gl libs)
