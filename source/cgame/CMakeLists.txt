project (cgame)

file(GLOB CGAME_HEADES
    "*.h"
)

file(GLOB CGAME_SOURCES
    "*.cpp"
    "../gameshared/*.c"
    "../qalgo/*.c"
)

add_library(cgame SHARED ${CGAME_SOURCES} ${CGAME_HEADES})
set_property(TARGET cgame PROPERTY CXX_STANDARD 11)
qf_set_output_dir(cgame base)