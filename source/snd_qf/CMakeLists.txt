project(snd_qf)

include_directories(${VORBIS_INCLUDE_DIR} ${OGG_INCLUDE_DIR} "../snd_common" ".")

if (USE_SDL2)
    include_directories(${SDL2_INCLUDE_DIR})
else()
    include_directories(${SDL_INCLUDE_DIR})
endif()

file(GLOB SND_QF_HEADERS
    "*.h"
)

file(GLOB SND_QF_SOURCES
    "*.c"
    "../snd_common/*.c"
    "../gameshared/q_*.c"
    "../qalgo/*.c"
)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    file(GLOB SND_QF_PLATFORM_SOURCES
        "../win32/win_snd.c"
    )

    set(SND_QF_PLATFORM_LIBRARIES "winmm.lib")
else()
    file(GLOB SND_QF_PLATFORM_SOURCES
        "../unix/unix_snd.c"
    )

    if (USE_SDL2)
        set(SND_QF_PLATFORM_LIBRARIES ${SDL2_LIBRARY})
    else()  
        set(SND_QF_PLATFORM_LIBRARIES ${SDL_LIBRARY})
    endif()
endif()

add_library(snd_qf SHARED ${SND_QF_HEADERS} ${SND_QF_SOURCES} ${SND_QF_PLATFORM_SOURCES})
target_link_libraries(snd_qf ${OGG_LIBRARY} ${VORBIS_LIBRARIES} ${SND_QF_PLATFORM_LIBRARIES})
qf_set_output_dir(snd_qf libs)