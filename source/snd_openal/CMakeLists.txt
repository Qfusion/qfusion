project(snd_openal)

include_directories(${VORBIS_INCLUDE_DIR} ${OGG_INCLUDE_DIR} "../snd_common" ".")

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    include_directories("../win32/include") # For al.h
endif()

file(GLOB SND_OPENAL_HEADERS
    "*.h"
)

file(GLOB SND_OPENAL_SOURCES
    "*.c"
    "../snd_common/*.c"
    "../gameshared/q_*.c"
    "../qalgo/*.c"
)

add_library(snd_openal SHARED ${SND_OPENAL_HEADERS} ${SND_OPENAL_SOURCES})
target_link_libraries(snd_openal PRIVATE ${OGG_LIBRARY} ${VORBIS_LIBRARIES})
qf_set_output_dir(snd_openal libs)