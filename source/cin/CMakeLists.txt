project(cin)

include_directories()

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
target_link_libraries(cin PRIVATE ${CIN_PLATFORM_LIBRARIES})
qf_set_output_dir(cin libs)
