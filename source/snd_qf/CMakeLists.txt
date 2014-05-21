project(snd_qf)

include_directories(${VORBIS_INCLUDE_DIR} ${OGG_INCLUDE_DIR} ${SDL_INCLUDE_DIR} "../snd_common" ".")

file(GLOB SND_QF_HEADERS
    "*.h"
)

file(GLOB SND_QF_SOURCES
    "*.c"
    "../snd_common/*.c"
    "../gameshared/q_*.c"
    "../qalgo/*.c"
    "../unix/unix_snd.c"
)

qf_add_library(snd_qf SHARED ${SND_QF_SOURCES} ${SND_QF_HEADERS})
target_link_libraries(snd_qf ${OGG_LIBRARY} ${VORBIS_LIBRARIES} ${SDL_LIBRARY})
qf_set_output_dir(snd_qf libs)