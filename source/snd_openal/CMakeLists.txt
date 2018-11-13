project(snd_openal)

include_directories(${VORBIS_INCLUDE_DIR} ${OGG_INCLUDE_DIR} "../../libs" "../snd_common" ".")

add_definitions(-DAL_LIBTYPE_STATIC)

file(GLOB SND_OPENAL_HEADERS
	"*.h"
	"../gameshared/q_*.h"
	"../client/snd_public.h"
	)

file(GLOB SND_OPENAL_SOURCES
	"*.c"
	"../snd_common/*.c"
	"../gameshared/q_*.c"
	)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(SND_OPENAL_PLATFORM_LIBRARIES "m" "dl")
else ()
	set(SND_OPENAL_PLATFORM_LIBRARIES "winmm")
endif()

add_library(snd_openal SHARED ${SND_OPENAL_HEADERS} ${SND_OPENAL_SOURCES})
target_link_libraries(snd_openal PRIVATE ${OPENAL_LIBRARY} ${OGG_LIBRARY} ${VORBIS_LIBRARIES} ${SND_OPENAL_PLATFORM_LIBRARIES})
qf_set_output_dir(snd_openal libs)
