project(snd_qf)

include_directories(${VORBIS_INCLUDE_DIR} ${OGG_INCLUDE_DIR})

file(GLOB SND_QF_HEADERS
    "*.h"
)

file(GLOB SND_QF_SOURCES
    "*.c"
    "../gameshared/q_*.c"
    "../qalgo/*.c"
    "../unix/unix_snd.c"
)

qf_add_library(snd_qf SHARED ${SND_QF_SOURCES} ${SND_QF_HEADERS})
target_link_libraries(snd_qf ${OGG_LIBRARY} ${VORBIS_LIBRARIES})
qf_set_output_dir(snd_qf libs)