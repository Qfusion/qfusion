project (game)

include_directories(${ANGELSCRIPT_INCLUDE_DIR})

file(GLOB GAME_HEADERS
	"*.h"
	"../gameshared/*.h"
	"../qalgo/base64.h"
	"../qalgo/md5.h"
)

file(GLOB GAME_SOURCES
	"*.cpp"
	"../gameshared/*.cpp"
	"../qalgo/base64.cpp"
	"../qalgo/md5.cpp"
)

add_library(game SHARED ${GAME_SOURCES} ${GAME_HEADERS})
target_link_libraries(game PRIVATE)
set_property(TARGET game PROPERTY CXX_STANDARD 11)
qf_set_output_dir(game ${QFUSION_GAME_DIR})
