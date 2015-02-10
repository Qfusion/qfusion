project(steamlib)

file(GLOB STEAMLIB_HEADERS
    "*.h"
)

file(GLOB STEAMLIB_SOURCES
    "*.cpp"
)

add_library(steamlib SHARED ${STEAMLIB_SOURCES} ${STEAMLIB_HEADERS})
qf_set_output_dir(steamlib libs)