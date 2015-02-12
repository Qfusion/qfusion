project (game)

include_directories(${ANGELSCRIPT_INCLUDE_DIR})

file(GLOB GAME_HEADERS
    "*.h"
    "ai/*.h"
)

file(GLOB GAME_SOURCES
    "*.cpp"
    "ai/*.cpp"
    "../gameshared/*.c"
    "../qalgo/*.c"
    "../matchmaker/mm_rating.c"
)

add_library(game SHARED ${GAME_SOURCES} ${GAME_HEADERS})
target_link_libraries(game PRIVATE ${ANGELSCRIPT_LIBRARY})
add_dependencies(game angelwrap)
qf_set_output_dir(game base)