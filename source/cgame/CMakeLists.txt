project (cgame)

include_directories(${ANGELSCRIPT_INCLUDE_DIR})

file(GLOB CGAME_HEADES
	"*.h"
	"../gameshared/*.h"
)

file(GLOB CGAME_SOURCES
	"*.cpp"
	"../gameshared/*.c"
	"../gameshared/gs_ascript.cpp"
)

add_library(cgame SHARED ${CGAME_SOURCES} ${CGAME_HEADES})
add_dependencies(cgame angelwrap)
set_property(TARGET cgame PROPERTY CXX_STANDARD 11)
qf_set_output_dir(cgame ${QFUSION_GAME_DIR})