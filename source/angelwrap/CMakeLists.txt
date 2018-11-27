project (angelwrap)

include_directories(${ANGELSCRIPT_INCLUDE_DIR})

file(GLOB angelwrap_HEADERS
	"*.h"
	"addon/*.h"
	"../gameshared/q_*.h"
	"../qalgo/rng.h"
	)

file(GLOB angelwrap_SOURCES
	"*.cpp"
	"*.c"
	"addon/*.cpp"
	"../gameshared/q_*.cpp"
	"../qalgo/rng.cpp"
	)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(ANGELWRAP_PLATFORM_LIBRARIES "pthread")
else ()
	set(ANGELWRAP_PLATFORM_LIBRARIES "")
endif()

add_library(angelwrap SHARED ${angelwrap_SOURCES} ${angelwrap_HEADERS})
target_link_libraries(angelwrap PRIVATE angelscript ${ANGELWRAP_PLATFORM_LIBRARIES})
qf_set_output_dir(angelwrap libs)
