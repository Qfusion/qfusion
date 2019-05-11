project (angelwrap)

include_directories(${ANGELSCRIPT_INCLUDE_DIR})

file(GLOB angelwrap_HEADERS
    "*.h"
    "addon/*.h"
	"../gameshared/q_*.h"
)

file(GLOB angelwrap_SOURCES
    "*.cpp"
    "*.c"
    "addon/*.cpp"
    "../gameshared/q_*.c"
)

if (UNIX AND NOT APPLE)
	set(ANGELWRAP_PLATFORM_LIBRARIES pthread)
endif()

add_library(angelwrap SHARED ${angelwrap_SOURCES} ${angelwrap_HEADERS})
target_link_libraries(angelwrap PRIVATE angelscript ${ANGELWRAP_PLATFORM_LIBRARIES})
qf_set_output_dir(angelwrap libs)
