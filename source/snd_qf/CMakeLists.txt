project(snd_qf)

include_directories(${STB_INCLUDE_DIR} "../snd_common" ".")

if (USE_SDL2 OR (UNIX AND NOT APPLE))
    include_directories(${SDL2_INCLUDE_DIR})
endif()

file(GLOB SND_QF_HEADERS
    "*.h"
	"../gameshared/q_*.h"
	"../client/snd_public.h"
)

file(GLOB SND_QF_SOURCES
    "*.c"
    "../snd_common/*.c"
    "../gameshared/q_*.c"
)

if (USE_SDL2 OR (UNIX AND NOT APPLE))
    file(GLOB SND_QF_PLATFORM_SOURCES
        "../sdl/sdl_snd.c"
    )

    if (UNIX AND NOT APPLE)
	set(SND_QF_PLATFORM_LIBRARIES ${SDL2_LIBRARY} m)
    else()
	set(SND_QF_PLATFORM_LIBRARIES ${SDL2_LIBRARY})
    endif()
else()
    file(GLOB SND_QF_PLATFORM_SOURCES
        "../win32/win_snd.c"
    )

    set(SND_QF_PLATFORM_LIBRARIES winmm.lib)
endif()

add_library(snd_qf SHARED ${SND_QF_HEADERS} ${SND_QF_SOURCES} ${SND_QF_PLATFORM_SOURCES})
target_link_libraries(snd_qf PRIVATE ${SND_QF_PLATFORM_LIBRARIES})
qf_set_output_dir(snd_qf libs)
