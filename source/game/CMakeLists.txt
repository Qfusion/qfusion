project (game)

include_directories(${ANGELSCRIPT_INCLUDE_DIR})

file(GLOB GAME_HEADERS
	"*.h"
	"ai/*.h"
	"../gameshared/*.h"
	"../qalgo/base64.h"
	"../qalgo/md5.h"
)

file(GLOB GAME_SOURCES
	"*.cpp"
	"../gameshared/*.c"
	"../gameshared/gs_ascript.cpp"
	"../matchmaker/mm_rating.c"
	"../qalgo/base64.c"
	"../qalgo/md5.c"
)

add_library(game SHARED ${GAME_SOURCES} ${GAME_HEADERS})
target_link_libraries(game PRIVATE)
add_dependencies(game angelwrap)
set_property(TARGET game PROPERTY CXX_STANDARD 11)
qf_set_output_dir(game ${QFUSION_GAME_DIR})
