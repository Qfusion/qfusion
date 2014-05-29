project(cin)

include_directories(${OGG_INCLUDE_DIR} ${VORBIS_INCLUDE_DIR} ${THEORA_INCLUDE_DIR})

file(GLOB CIN_HEADERS
    "*.h"
)

file(GLOB CIN_SOURCES
    "*.c"
    "../gameshared/q_*.c"
    "../qalgo/*.c"
)

qf_add_library(cin SHARED ${CIN_SOURCES} ${CIN_HEADERS})
target_link_libraries(cin ${OGG_LIBRARY} ${VORBIS_LIBRARIES} ${THEORA_LIBRARY})
qf_set_output_dir(cin libs)