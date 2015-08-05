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

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(ANGELWRAP_PLATFORM_LIBRARIES "pthread")
else ()
        set(ANGELWRAP_PLATFORM_LIBRARIES "")
endif()

add_library(angelwrap SHARED ${angelwrap_SOURCES} ${angelwrap_HEADERS})
target_link_libraries(angelwrap PRIVATE ${ANGELSCRIPT_LIBRARY} ${ANGELWRAP_PLATFORM_LIBRARIES})
qf_set_output_dir(angelwrap libs)

# TODO: Remove this hack from here
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR MINGW)
    add_dependencies(angelwrap ${ANGELSCRIPT})
endif()
