project (angelwrap)

include_directories(${ANGELSCRIPT_INCLUDE_DIR})

file(GLOB angelweap_HEADES
    "*.h"
    "addon/*.h"
)

file(GLOB angelwrap_SOURCES
    "*.cpp"
    "*.c"
    "addon/*.cpp"
    "../gameshared/q_*.c"
    "qalgo/*.c"
)

add_library(angelwrap SHARED ${angelwrap_SOURCES} ${angelwrap_HEADERS})
target_link_libraries(angelwrap ${ANGELSCRIPT_LIBRARY})
qf_set_output_dir(angelwrap libs)

# TODO: Remove this hack from here
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR MINGW)
    add_dependencies(angelwrap ${ANGELSCRIPT})
endif()