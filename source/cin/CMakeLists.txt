project(cin)

include_directories(${OGG_INCLUDE_DIR} ${VORBIS_INCLUDE_DIR} ${THEORA_INCLUDE_DIR})

file(GLOB CIN_HEADERS
    "*.h"
	"../gameshared/q_*.h"
)

file(GLOB CIN_SOURCES
    "*.c"
    "../gameshared/q_*.c"
)

if (UNIX AND NOT APPLE)
        set(CIN_PLATFORM_LIBRARIES m)
endif()

add_library(cin SHARED ${CIN_SOURCES} ${CIN_HEADERS})
target_link_libraries(cin PRIVATE ${OGG_LIBRARY} ${VORBIS_LIBRARIES} ${THEORA_LIBRARY} ${CIN_PLATFORM_LIBRARIES})
qf_set_output_dir(cin libs)
