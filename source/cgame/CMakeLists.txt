project (cgame)

file(GLOB CGAME_HEADES
    "*.h"
)

file(GLOB CGAME_SOURCES
    "*.cpp"
    "../gameshared/*.c"
    "../qalgo/*.c"
)

qf_add_library(cgame SHARED ${CGAME_SOURCES} ${CGAME_HEADES})
qf_set_output_dir(cgame base)