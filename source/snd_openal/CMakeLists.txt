project(snd_openal)

include_directories(${VORBIS_INCLUDE_DIR} ${OGG_INCLUDE_DIR})

file(GLOB SND_OPENAL_HEADERS
    "*.h"
)

file(GLOB SND_OPENAL_SOURCES
    "*.c"
    "../gameshared/q_*.c"
    "../qalgo/*.c"
)

qf_add_library(snd_openal SHARED ${SND_OPENAL_SOURCES} ${SND_OPENAL_HEADERS})
target_link_libraries(snd_openal ${OGG_LIBRARY} ${VORBIS_LIBRARIES})
qf_set_output_dir(snd_openal libs)