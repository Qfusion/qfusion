project (game)

include_directories(${ANGELSCRIPT_INCLUDE_DIR})

file(GLOB GAME_HEADERS
	"*.h"
	"ai/*.h"
	"../gameshared/*.h"
)

file(GLOB GAME_SOURCES
	"*.cpp"
	"ai/*.cpp"
	"../gameshared/*.c"
	"../gameshared/gs_*.cpp"
	"../matchmaker/mm_rating.c"
)

add_library(game SHARED ${GAME_SOURCES} ${GAME_HEADERS})
target_link_libraries(game PRIVATE)
add_dependencies(game angelwrap)
set_property(TARGET game PROPERTY CXX_STANDARD 11)
qf_set_output_dir(game ${QFUSION_GAME_DIR})
